#include <iostream>
#include <cassert>
#include <Windows.h>
#include "Message.hpp"


static HANDLE g_pipe = 0;


static HANDLE InitializePipe() {
    DWORD thread_id = GetCurrentThreadId();
    std::wstring pipe_name = PIPE_NAME_BASE + std::to_wstring(thread_id);

    // connect to existing pipe
    HANDLE pipe = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
        0,              // no sharing 
        NULL,           // default security attributes
        OPEN_EXISTING,  // opens existing pipe 
        0,              // default attributes 
        NULL);          // no template file 
    if (pipe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        assert((err != ERROR_ACCESS_DENIED) && "when trying to open pipe"); // sandboxing problem
        assert(false);
        abort();
    }

    // change pipe from byte- to message-mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    BOOL ok = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
    if (!ok) {
        assert(false && "SetNamedPipeHandleState failure");
        abort();
    }

    return pipe;
}

static void OnIconUpdated(HWND wnd, WPARAM wParam, HICON icon) {
    IconUpdateMessage msg;
    msg.window = (HWND)wnd;
    msg.param = wParam;
    msg.icon = icon;

    // write message to pipe
    DWORD bWritten = 0;
    BOOL ok = WriteFile(g_pipe, &msg, sizeof(msg), &bWritten, NULL);
    if (!ok) {
        assert(false && "WriteFile to pipe failed");
        abort();
    }
}

extern "C" __declspec(dllexport) LRESULT Hookproc(int code, WPARAM sent_by_current_proc, LPARAM param) {
    if (code < 0) // do not process message 
        return CallNextHookEx(NULL, code, sent_by_current_proc, param);

    if (!sent_by_current_proc)
        CallNextHookEx(NULL, code, sent_by_current_proc, param);

    if (!g_pipe) {
        // on-demand pipe opening
        g_pipe = InitializePipe();
    }

    auto* cwp = (CWPRETSTRUCT*)param;

    // filter successful actions for the specified window
    if (code == HC_ACTION) {
        // filter set-icon messages
        if (cwp->message == WM_SETICON) {
            // cwp->lResult contains the prev. icon handle
            OnIconUpdated(cwp->hwnd, cwp->wParam, (HICON)cwp->lParam);
        } else if (cwp->message == WM_SETTEXT) {
            // log window title updates in Visual Studio "Output" window
            OutputDebugStringW(L"WM_SETTEXT: ");
            OutputDebugStringW((wchar_t*)cwp->lParam);
            OutputDebugStringW(L"\n");
        }
    }

    return CallNextHookEx(NULL, code, sent_by_current_proc, param);
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  nReason, LPVOID lpReserved) {
    switch (nReason) {
    case DLL_PROCESS_ATTACH:
        // delay load pipe
        break;
    case DLL_PROCESS_DETACH:
        if (g_pipe) {
            CloseHandle(g_pipe);
            g_pipe = 0;
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return true;
}
