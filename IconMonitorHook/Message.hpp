#pragma once
#include <string>

static constexpr wchar_t PIPE_NAME[] = L"\\\\.\\pipe\\IconMonitor";
static constexpr DWORD PIPE_TIMEOUT = 5000;

struct IconUpdateMessage {
    HWND   window = 0;
    WPARAM param = 0;
    HICON  icon = 0;

    bool IsValid() const {
        return window || param || icon;
    }

    std::string ToString() const {
        std::string result = "[IconUpdateMessage] window=" + std::to_string((size_t)window)
            + ", param=" + std::to_string(param)
            + ", icon=" + std::to_string((size_t)icon) + "\n";
        return result;
    }
};

class MonitorIconUpdate {
public:
    MonitorIconUpdate(DWORD thread_id) {
        m_module = LoadLibraryW(L"IconMonitorHook.dll"); // DLL handle
        auto callback = (HOOKPROC)GetProcAddress(m_module, "Hookproc");

        m_hook = SetWindowsHookExW(WH_CALLWNDPROCRET, callback, m_module, thread_id);
        assert(m_hook);
    }
    ~MonitorIconUpdate() {
        UnhookWindowsHookEx(m_hook);
        m_hook = 0;

        FreeLibrary(m_module);
        m_module = 0;
    }

private:
    HINSTANCE m_module = 0;
    HHOOK     m_hook = 0;
};
