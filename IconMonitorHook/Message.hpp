#pragma once
#include <string>
#include <vector>

static constexpr wchar_t PIPE_NAME_BASE[] = L"\\\\.\\pipe\\IconMonitor_";

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

        BITMAP color = {};
        ok = GetObject(ii.hbmColor, sizeof(color), &color) == sizeof(color);
        assert(ok);

        {
            // TEST: Retrieve icon pixels
            HDC dc = CreateCompatibleDC(NULL);

            // call GetDIBits to fill "bmp_info" struct
            BITMAPINFO bmp_info = {};
            bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
            int ok = GetDIBits(dc, ii.hbmColor, 0, color.bmHeight, nullptr, &bmp_info, DIB_RGB_COLORS);
            if (!ok)
                abort();

            bmp_info.bmiHeader.biCompression = BI_RGB; // disable compression

            // call GetDIBits to get image data
            const RGBQUAD ZERO_PIZEL = {};
            std::vector<RGBQUAD> pixels(color.bmHeight*color.bmWidth, ZERO_PIZEL);
            int scan_lines = GetDIBits(dc, ii.hbmColor, 0, color.bmHeight, pixels.data(), &bmp_info, DIB_RGB_COLORS);
            if (scan_lines != color.bmHeight)
                abort();

            DeleteDC(dc);
        }

        POINT size = {
            color.bmWidth,
            ii.hbmColor ? color.bmHeight : color.bmHeight / 2,
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
