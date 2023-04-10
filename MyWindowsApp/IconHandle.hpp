#pragma once
#include <cassert>
#include <Windows.h>

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
