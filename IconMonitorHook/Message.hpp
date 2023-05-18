#pragma once
#include <string>
#include <vector>

static constexpr wchar_t PIPE_NAME_BASE[] = L"\\\\.\\pipe\\IconMonitor_";

class BaseMessage {
public:
    BaseMessage(UINT t, HWND wnd) : type(t), window(wnd) {
    }

    bool IsValid() const {
        return type && window;
    }

    const UINT type = 0;
    const HWND window = 0;
};
    
class IconUpdateMessage : public BaseMessage {
public:
    WPARAM param = 0;
    HICON  icon = 0;

    IconUpdateMessage(HWND wnd = 0) : BaseMessage(WM_SETICON, wnd) {
    }

    bool IsValid() const {
        return BaseMessage::IsValid() && (type == WM_SETICON) && param && icon;
    }

    std::wstring ToString() const {
        std::wstring result = L"[IconUpdateMessage] window=" + std::to_wstring((size_t)window)
            + L", param=" + std::to_wstring(param)
            + L", icon=" + std::to_wstring((size_t)icon);

        auto size = IconSize(icon);
        result += L" size={" + std::to_wstring(size.x) + L"," + std::to_wstring(size.y) + L"}";
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
// verify packed storage
static_assert(sizeof(IconUpdateMessage) == sizeof(BaseMessage) + sizeof(IconUpdateMessage::param) + sizeof(IconUpdateMessage::icon));


class TitlepdateMessage : public BaseMessage {
public:
    TitlepdateMessage(HWND wnd) : BaseMessage(WM_SETTEXT, wnd) {
    }

    bool IsValid() const {
        return BaseMessage::IsValid() && (type == WM_SETTEXT) && title_len;
    }

    void Initialize(std::wstring new_title) {
        title_len = new_title.length();
        if (TitleBytes() > sizeof(title))
            throw std::runtime_error("TitlepdateMessage title overflow");

        memcpy(title, new_title.c_str(), TitleBytes());
    }

    DWORD Size() {
        return (DWORD)(sizeof(BaseMessage) + sizeof(title_len) + TitleBytes());
    }

private:
    size_t TitleBytes() const {
        assert(title_len);
        return (title_len + 1) * sizeof(wchar_t); // number of bytes required, incl. zero-termination
    }

    size_t  title_len = 0; // title length [characters] (excl. zero-termination)
    wchar_t title[1024] = {};
};
// verify packed storage
static_assert(sizeof(TitlepdateMessage) == sizeof(BaseMessage) + sizeof(TitlepdateMessage::title_len) + sizeof(TitlepdateMessage::title));


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
