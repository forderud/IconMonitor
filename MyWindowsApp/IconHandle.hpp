#pragma once
#include <cassert>
#include <vector>
#include <Windows.h>


static HICON CreateIconFromRGB(HWND wnd) {
    const uint8_t width = 32, height = 32;
    std::vector<uint32_t> bitmap(width * height, (uint32_t)0);

    for (uint8_t y = 0; y < height; y++) {
        for (uint8_t x = 0; x < width; x++) {
            uint8_t A = 255;
            uint8_t R = 8*x;
            uint8_t G = 8*y;
            uint8_t B = 0;
            bitmap[y*width + x] = (A << 24) | (R << 16) | (G << 8) | B;
        }
    }

    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;

    iconInfo.hbmColor = CreateBitmap(width, height, 1, 32/*bit per pixel*/, bitmap.data());
    assert(iconInfo.hbmColor);

    // Obtain a handle to the screen device context.
    HDC hdcScreen = GetDC(wnd);
    assert(hdcScreen);

    iconInfo.hbmMask = CreateCompatibleBitmap(hdcScreen, width, height);
    assert(iconInfo.hbmMask);

    HICON hIcon = CreateIconIndirect(&iconInfo);
    assert(hIcon);

    DeleteObject(iconInfo.hbmMask);
    ReleaseDC(wnd, hdcScreen);
    DeleteObject(iconInfo.hbmColor);

    return hIcon;
}


class IconHandle {
public:
    IconHandle() = default;

    IconHandle(HICON icon, bool destroy) : m_icon(icon), m_destroy(destroy) {
    }

    ~IconHandle() {
        if (m_destroy)
            DestroyIcon(m_icon);
    }

    IconHandle& operator = (IconHandle&& other) {
        Swap(other);
        return *this;
    }

    operator bool() const {
        return m_icon;
    }


    void Swap(IconHandle & other) {
        std::swap(m_icon, other.m_icon);
        std::swap(m_destroy, other.m_destroy);
    }

    void Activate(HWND wnd, WPARAM icon_type) {
        assert(m_icon);
        assert((icon_type == ICON_SMALL) || (icon_type == ICON_BIG));

        // If ICON_SMALL: update window titlebar (top-left icon)
        // If ICON_BIG: update windws taskbar at bottom om screen
        LRESULT res = SendMessageW(wnd, WM_SETICON, icon_type, (LPARAM)m_icon);
        // res contains prev icon handle
    }
private:
    HICON m_icon = 0;
    bool  m_destroy = false; ///< destroy on close
};
