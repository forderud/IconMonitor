#include <cassert>
#include <iostream>
#include <tuple>
#include <Windows.h>
#include <aclapi.h> // for SE_KERNEL_OBJECT
#include <sddl.h> // for SDDL_REVISION_1

#include "../IconMonitorHook/Message.hpp"


struct PIPEINST : public OVERLAPPED {
    PIPEINST(HANDLE _pipe) : pipe(_pipe) {
        assert(_pipe);
        // clear inherited fields
        Internal = 0;
        InternalHigh = 0;
        Pointer = 0;
        hEvent = 0;
    }

    ~PIPEINST() {
        BOOL ok = DisconnectNamedPipe(pipe);
        assert(ok);
        CloseHandle(pipe);

        printf("Pipe disconnected.\n");
    }

    HANDLE pipe = 0;

    // read message
    IconUpdateMessage request;
};


std::tuple<BOOL,HANDLE> CreateAndConnectInstance(OVERLAPPED& overlap, DWORD thread_id, bool first) {
    std::wstring pipe_name = PIPE_NAME_BASE;
#ifdef _DEBUG
    pipe_name += L"debug"; // deterministic pipe name in debug builds
#else
    pipe_name += std::to_wstring(thread_id);
#endif

    DWORD mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED; // read/write access | overlapped mode
    if (first)
        mode |= WRITE_OWNER; // enable changing permissions

    HANDLE pipe = CreateNamedPipeW(pipe_name.c_str(),
        mode, // read/write access | overlapped mode
        PIPE_TYPE_MESSAGE |       // message-type pipe 
        PIPE_READMODE_MESSAGE |   // message read mode 
        PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // unlimited instances 
        sizeof(IconUpdateMessage),// output buffer size 
        sizeof(IconUpdateMessage),// input buffer size 
        PIPE_TIMEOUT,             // client time-out 
        NULL);                    // security
    assert(pipe != INVALID_HANDLE_VALUE);

    if (first) {
        ACL* sacl = nullptr; // system access control list (weak ptr.)
        PSECURITY_DESCRIPTOR sd = {}; // must outlive SetSecurityInfo to avoid sporadic failures
        {
            // initialize "low IL" System Access Control List (SACL)
            // Security Descriptor String interpretation: (based on sddl.h)
            // SACL:(ace_type=Mandatory integrity Label (ML); ace_flags=; rights=SDDL_NO_WRITE_UP (NW); object_guid=; inherit_object_guid=; account_sid=Low mandatory level (LW))
            BOOL ok  = ConvertStringSecurityDescriptorToSecurityDescriptorW(L"S:(ML;;NW;;;LW)", SDDL_REVISION_1, &sd, NULL);
            assert(ok);
            BOOL sacl_present = FALSE;
            BOOL sacl_defaulted = FALSE;
            ok = GetSecurityDescriptorSacl(sd, &sacl_present, &sacl, &sacl_defaulted);
            assert(ok);
        }
        DWORD res = SetSecurityInfo(pipe, SE_KERNEL_OBJECT, LABEL_SECURITY_INFORMATION, /*owner*/NULL, /*group*/NULL, /*dacl*/NULL, sacl);
        assert((res != ERROR_ACCESS_DENIED) && "when modifying pipe permissions"); // sandboxing problem
        assert((res == ERROR_SUCCESS) && "when modifying pipe permissions"); // sandboxing problem

        LocalFree(sd);
    }

    // connect to the new client. 
    // wait for client to connect
    BOOL ok = ConnectNamedPipe(pipe, &overlap);
    assert(!ok); // overlapped ConnectNamedPipe should fail

    switch (GetLastError()) {
    case ERROR_IO_PENDING:
        // overlapped connection in progress
        return { true, pipe };

    case ERROR_PIPE_CONNECTED:
        // Client already connected, so signal an event.
        // This is unlikely, but can happen if the client connects between CreateNamedPipe and ConnectNamedPipe.
        if (SetEvent(overlap.hEvent))
            return { false, pipe };

    default:
        // error occured during the connect operation... 
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        assert(false);
        abort();
    }
}

// This routine is called as a completion routine after writing to 
// the pipe, or when a new client has connected to a pipe instance.
void CompletedReadRoutine(DWORD err, DWORD bRead, OVERLAPPED* overLap) {
    auto* pipeInst = static_cast<PIPEINST*>(overLap);

    if ((err != 0) || (bRead != sizeof(pipeInst->request))) {
        // previous read failed so clean up and return
        delete pipeInst;
        return;
    }

    // print result of previous read
    if (pipeInst->request.IsValid())
        printf(pipeInst->request.ToString().c_str());

    // schedule next read
    BOOL ok = ReadFileEx(pipeInst->pipe, &pipeInst->request, sizeof(pipeInst->request),
        pipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
    if (!ok)
        delete pipeInst;
}

int main(int argc, char* argv[]) {
    printf("IconMonitorClient.\n");

    if (argc < 2) {
        std::cerr << "ERROR: Please provide a thread handle as argument.\n";
        exit(-1);
    }

    DWORD thread_id = std::stoi(argv[1]);
    std::cerr << "Injecting hook DLL into proces with thread-id=" << thread_id << std::endl;
    MonitorIconUpdate monitor(thread_id);
    if (!monitor) {
        std::cerr << "ERROR: Invalid thread handle argument.\n";
        exit(-1);
    }

    // event for the connect operation
    OVERLAPPED connect = {};
    connect.hEvent = CreateEventW(NULL, true, true, NULL);
    assert(connect.hEvent);

    BOOL  pending_io = false;
    HANDLE pipe = 0;
    std::tie(pending_io, pipe) = CreateAndConnectInstance(connect, thread_id, true);

    while(pipe) {
        // Wait for a client to connect, or for a read or write operation to be completed,
        // which causes a completion routine to be queued for execution. 
        DWORD res = WaitForSingleObjectEx(connect.hEvent, INFINITE, true); // alertable wait

        switch (res) {
        case WAIT_OBJECT_0:
            // The wait conditions are satisfied by a completed connect operation. 
        {
            // If an operation is pending, get the result of the connect operation. 
            if (pending_io) {
                DWORD bytes_xfered = 0;
                BOOL ok = GetOverlappedResult(pipe, &connect, &bytes_xfered, false); // non-blocking
                if (!ok) {
                    printf("ConnectNamedPipe (%d)\n", GetLastError());
                    return 0;
                }
            }

            // Start the read operation for this client (move pipe to new PIPEINST object)
            CompletedReadRoutine(0, sizeof(PIPEINST::request), new PIPEINST(pipe));
            pipe = 0;
            pending_io = false;

            // Create new pipe instance for the next client. 
            std::tie(pending_io, pipe) = CreateAndConnectInstance(connect, thread_id, false);
            break;
        }

        case WAIT_IO_COMPLETION:
            // The wait is satisfied by a completed read or write operation. This allows the system to execute the completion routine. 
            break;

        default:
            // An error occurred in the wait function.
            printf("WaitForSingleObjectEx (%d)\n", GetLastError());
            exit(-2);
        }
    }

    // never reached
    return 0;
}
