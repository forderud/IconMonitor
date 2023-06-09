#pragma once
#include <sstream>
#include <string>
#include <vector>

static constexpr wchar_t PIPE_NAME_BASE[] = L"\\\\.\\pipe\\IconMonitor_";


static std::wstring HandleHexStr(HANDLE window) {
    std::wstringstream window_str; // in hex format
    window_str << std::hex << (size_t)window;
    return window_str.str();
}


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
        std::wstring result = L"[IconUpdateMessage] window=" + HandleHexStr(window)
            + L", param=" + std::to_wstring(param)
            + L", icon=" + HandleHexStr(icon);

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
        return BaseMessage::IsValid() && (type == WM_SETTEXT) && title[0];
    }

    void Initialize(std::wstring new_title) {
        size_t title_len = new_title.length(); // excl. null-termination
        size_t title_bytes = (title_len + 1) * sizeof(wchar_t); // number of bytes required, incl. zero-termination

        if (title_bytes > sizeof(title))
            throw std::runtime_error("TitlepdateMessage title overflow");

        memcpy(title, new_title.c_str(), title_bytes);
    }

    std::wstring ToString() const {
        std::wstring result = L"[TitlepdateMessage] window=" + HandleHexStr(window);
        result += L" title=" + std::wstring(title);

        return result;
    }

private:
    wchar_t title[1024] = {};
};
// verify packed storage
static_assert(sizeof(TitlepdateMessage) == sizeof(BaseMessage) + sizeof(TitlepdateMessage::title));
