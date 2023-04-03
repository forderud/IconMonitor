#include <cassert>
#include <iostream>
#include <tuple>
#include <Windows.h>
#include "Message.hpp"


struct PIPEINST : public OVERLAPPED {
    PIPEINST(HANDLE _pipe) : pipe(_pipe) {
        // clear inherited fields
        Internal = 0;
        InternalHigh = 0;
        Pointer = 0;
        hEvent = 0;
    }

    ~PIPEINST() {
        if (!DisconnectNamedPipe(pipe)) {
            printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
        }

        CloseHandle(pipe);

        printf("Pipe disconnected.\n");
    }

    HANDLE pipe = 0;

    // read message
    IconUpdateMessage request;
};


std::tuple<BOOL,HANDLE> CreateAndConnectInstance(OVERLAPPED& overlap) {
    HANDLE pipe = CreateNamedPipeW(PIPE_NAME,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // read/write access | overlapped mode
        PIPE_TYPE_MESSAGE |       // message-type pipe 
        PIPE_READMODE_MESSAGE |   // message read mode 
        PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // unlimited instances 
        sizeof(IconUpdateMessage),// output buffer size 
        sizeof(IconUpdateMessage),// input buffer size 
        PIPE_TIMEOUT,             // client time-out 
        NULL);                    // security
    if (pipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipe failed with %d.\n", GetLastError());
        assert(false);
        exit(-1);
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
        exit(-1);
    }
}

// This routine is called as a completion routine after writing to 
// the pipe, or when a new client has connected to a pipe instance.
void CompletedReadRoutine(DWORD err, DWORD bRead, OVERLAPPED* overLap) {
    auto* pipeInst = (PIPEINST*)overLap;

    if ((err != 0) || (bRead != sizeof(pipeInst->request))) {
        // previous read failed so clean up and return
        delete pipeInst;
        return;
    }

    // print result of previous read
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

    // event for the connect operation
    OVERLAPPED oConnect = {};
    oConnect.hEvent = CreateEventW(NULL, true, true, NULL);
    assert(oConnect.hEvent);

    BOOL  fPendingIO = false;
    HANDLE pipe = 0;
    std::tie(fPendingIO, pipe) = CreateAndConnectInstance(oConnect);

    for(;;) {
        // Wait for a client to connect, or for a read or write operation to be completed,
        // which causes a completion routine to be queued for execution. 
        DWORD res = WaitForSingleObjectEx(oConnect.hEvent, INFINITE, true); // alertable wait

        switch (res) {
        case WAIT_OBJECT_0:
            // The wait conditions are satisfied by a completed connect operation. 
        {
            // If an operation is pending, get the result of the connect operation. 
            if (fPendingIO) {
                DWORD cbRet = 0;
                BOOL ok = GetOverlappedResult(pipe, &oConnect, &cbRet, false); // non-blocking
                if (!ok) {
                    printf("ConnectNamedPipe (%d)\n", GetLastError());
                    return 0;
                }
            }

            // Start the read operation for this client (move pipe to new PIPEINST object)
            CompletedReadRoutine(0, sizeof(PIPEINST::request), new PIPEINST(pipe));
            pipe = 0;

            // Create new pipe instance for the next client. 
            std::tie(fPendingIO, pipe) = CreateAndConnectInstance(oConnect);
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
