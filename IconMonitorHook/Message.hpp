#pragma once
#include <string>

static constexpr wchar_t PIPE_NAME_BASE[] = L"\\\\.\\pipe\\IconMonitor_";
static constexpr DWORD PIPE_TIMEOUT = 5000;

class IconUpdateMessage {
public:
    HWND   window = 0;
    WPARAM param = 0;
    HICON  icon = 0;

    bool IsValid() const {
        return window || param || icon;
    }

    std::string ToString() const {
        std::string result = "[IconUpdateMessage] window=" + std::to_string((size_t)window)
            + ", param=" + std::to_string(param)
            + ", icon=" + std::to_string((size_t)icon);

        auto size = IconSize(icon);
        result += " size={" + std::to_string(size.x) + "," + std::to_string(size.y) + "}\n";
        return result;
    }

private:
    POINT IconSize(HICON icon) const {
        ICONINFO ii = {};
        BOOL ok = GetIconInfo(icon, &ii);
        assert(ok);

        BITMAP mask = {};
        ok = GetObject(ii.hbmMask, sizeof(mask), &mask) == sizeof(mask);
        assert(ok);

        POINT size = {
            mask.bmWidth,
            ii.hbmColor ? mask.bmHeight : mask.bmHeight / 2,
        };

        if (ii.hbmMask) {
            DeleteObject(ii.hbmMask);
            ii.hbmMask = nullptr;
        }
        if (ii.hbmColor) {
            DeleteObject(ii.hbmColor);
            ii.hbmColor = nullptr;
        }

        return size;
    }
};

class MonitorIconUpdate {
public:
    MonitorIconUpdate(DWORD thread_id) {
        m_module = LoadLibraryW(L"IconMonitorHook.dll"); // DLL handle
        assert(m_module);
        auto callback = (HOOKPROC)GetProcAddress(m_module, "Hookproc");

        // might fail on invalid thread_id
        m_hook = SetWindowsHookExW(WH_CALLWNDPROCRET, callback, m_module, thread_id);
    }
    ~MonitorIconUpdate() {
        if (m_hook) {
            UnhookWindowsHookEx(m_hook);
            m_hook = 0;
        }

        FreeLibrary(m_module);
        m_module = 0;
    }

    operator bool () const {
        return m_hook;
    }

private:
    HINSTANCE m_module = 0;
    HHOOK     m_hook = 0;
};
