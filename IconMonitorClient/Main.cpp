#include <cassert>
#include <iostream>
#include <tuple>
#include <Windows.h>
#include <sddl.h> // for SDDL_REVISION_1

#include "../IconMonitorHook/IconMonitorHook.hpp"


union AnyMessage {
    AnyMessage() {}

    IconUpdateMessage icon;
    TitlepdateMessage title;
};

struct PIPEINST : public OVERLAPPED, public std::enable_shared_from_this<PIPEINST> {
    PIPEINST(HANDLE _pipe) : pipe(_pipe) {
        assert(_pipe);
        // clear inherited fields
        Internal = 0;
        InternalHigh = 0;
        Pointer = 0;
        hEvent = 0;
    }

    /** Increase ref-count by one. 
      * Must be done _after_ the constructor. */
    void AddRef() {
        m_self_ref = shared_from_this();
    }

    /** Decrease ref-count by one. */
    void Release() {
        m_self_ref.reset();
    }

    ~PIPEINST() {
        BOOL ok = DisconnectNamedPipe(pipe);
        assert(ok);
        CloseHandle(pipe);

        std::wcout << L"Pipe disconnected." << std::endl;
    }

    HANDLE pipe = 0;

    // read message
    AnyMessage request;

private:
    std::shared_ptr<PIPEINST> m_self_ref;
};


/** Creates a pipe instance and connects to the client.
    Returns TRUE if the connect operation is pending, and FALSE if the connection has been completed. */
std::tuple<BOOL,HANDLE> CreateAndConnectInstance(OVERLAPPED& overlap, DWORD thread_id) {
    SECURITY_ATTRIBUTES sa = {}; // must outlive CreateNamedPipe to avoid sporadic failures
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = false;
    {
        // initialize "low IL" System Access Control List (SACL)
        // Security Descriptor String interpretation: (based on sddl.h)
        // SACL:(ace_type=Mandatory integrity Label (ML); ace_flags=; rights=SDDL_NO_WRITE_UP (NW); object_guid=; inherit_object_guid=; account_sid=Low mandatory level (LW))
        std::wstring sd_str = L"S:(ML;;NW;;;LW)";
        // DACL:(ace_type=Allowed; ace_flags=; rights=FILE_ALL; object_guid=; inherit_object_guid=; account_sid=Interactive (logged-on) User)
        sd_str += L"D:(A;;FA;;;IU)"; // INTERACTIVE_FILE_ALL
        BOOL ok = ConvertStringSecurityDescriptorToSecurityDescriptorW(sd_str.c_str(), SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL);
        assert(ok);
    }

    std::wstring pipe_name = PIPE_NAME_BASE + std::to_wstring(thread_id);
    HANDLE pipe = CreateNamedPipeW(pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // read/write access | overlapped mode
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, // message-type, message read, blocking
        PIPE_UNLIMITED_INSTANCES,   // unlimited instances 
        0,                          // output buffer size (empty)
        4*sizeof(IconUpdateMessage),// input buffer size (allow 4 unprocessed icon updates before the process freezes)
        1000,                       // client connect timeout [ms]
        &sa);                       // security
    assert(pipe != INVALID_HANDLE_VALUE);

    LocalFree(sa.lpSecurityDescriptor);

    // schedule client connection (returns immediately)
    BOOL ok = ConnectNamedPipe(pipe, &overlap);
    assert(!ok); // always fail in asynchronous case

    DWORD err = GetLastError();
    switch (err) {
    case ERROR_IO_PENDING:
        // overlapped connection in progress
        return { true, pipe };

    case ERROR_PIPE_CONNECTED: // client connected between CreateNamedPipe and ConnectNamedPipe
    case ERROR_NO_DATA: // pipe closed remotely (probably due to app termination)
        // signal event manually
        if (!SetEvent(overlap.hEvent)) {
            assert(false && "SetEvent falure");
            abort();
        }
        return { false, pipe };

    default:
        // unknown connection error
        assert(false && "ConnectNamedPipe failure");
        abort();
    }
}

// This routine is called as a completion routine after writing to 
// the pipe, or when a new client has connected to a pipe instance.
void CompletedReadRoutine(DWORD err, DWORD bRead, OVERLAPPED* overLap) {
    auto* pipeInst = static_cast<PIPEINST*>(overLap);

    if (err != 0) {
        // previous read failed so clean up and return
        std::wcerr << L"NOTICE: Read failed. Probably due to disconnect.\n";
        pipeInst->Release();
        return;
    }
    if ((bRead != sizeof(pipeInst->request.icon)) && (bRead != sizeof(pipeInst->request.title))) {
        // previous read truncated so clean up and return
        std::wcerr << L"NOTICE: Read truncated.\n";
        pipeInst->Release();
        return;
    }

    // print result of previous read
    if (pipeInst->request.icon.IsValid())
        std::wcout << pipeInst->request.icon.ToString() << std::endl;
    else if (pipeInst->request.title.IsValid())
        std::wcout << pipeInst->request.title.ToString() << std::endl;

    // schedule next read
    BOOL ok = ReadFileEx(pipeInst->pipe, &pipeInst->request, sizeof(pipeInst->request),
        pipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
    if (!ok)
        pipeInst->Release();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::wcerr << L"ERROR: Please provide a window handle as argument.\n";
        exit(-1);
    }

    auto hwnd = (HWND)std::stoll(argv[1], nullptr, 16); // hex value

    {
        IconQuery win_query(hwnd);
        // query initial window title
        std::wcout << L"Initial window title: " << win_query.GetText() << std::endl;
        // query initial icon
        std::wcout << L"Initial icon: " << HandleHexStr(win_query.GetIcon()) << std::endl;
    }

    DWORD thread_id = GetWindowThreadProcessId(hwnd, nullptr);
    std::wcout << L"Injecting hook DLL into process with HWND=" << HandleHexStr(hwnd) << std::endl;
    InconMonitorHook monitor(thread_id);
    if (!monitor) {
        std::wcerr << L"ERROR: Invalid window handle argument.\n";
        exit(-1);
    }

    // event for the connect operation
    OVERLAPPED connect = {};
    connect.hEvent = CreateEventW(NULL, true, true, NULL);
    assert(connect.hEvent);

    BOOL  pending_io = false;
    HANDLE pipe = 0;
    std::tie(pending_io, pipe) = CreateAndConnectInstance(connect, thread_id);

    // wait for client to connect
    DWORD res = WaitForSingleObjectEx(connect.hEvent, INFINITE, true); // alertable wait

    std::weak_ptr<PIPEINST> pipe_inst;

    // The wait conditions are satisfied by a completed connect operation. 
    switch (res) {
    case WAIT_OBJECT_0:
    {
        // If an operation is pending, get the result of the connect operation. 
        if (pending_io) {
            DWORD bytes_xfered = 0;
            BOOL ok = GetOverlappedResult(pipe, &connect, &bytes_xfered, false); // non-blocking
            if (!ok) {
                assert(false && "ConnectNamedPipe failure");
                abort();
            }
        }

        // Start the read operation for this client (move pipe to new PIPEINST object)
        auto pipe_obj = std::make_shared<PIPEINST>(pipe);
        pipe_obj->AddRef();
        pipe_inst = pipe_obj;
        CompletedReadRoutine(0, sizeof(PIPEINST::request), pipe_obj.get());
        pipe = 0;
        pending_io = false;
        break;
    }

    case WAIT_TIMEOUT:
        std::wcout << L"Timeout before client connected." << std::endl;
        pipe = 0;
        pending_io = false;
        break;

    case WAIT_IO_COMPLETION:
    default:
        assert(false && "WaitForSingleObjectEx failure");
        abort();
    }

    // process completion routines until all clients are disconnected
    while (pipe_inst.use_count() > 0) {
        SleepEx(1000, true); // returns immediately on alert 
    }

    return 0;
}
