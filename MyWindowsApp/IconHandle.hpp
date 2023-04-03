#pragma once
#include <cassert>
#include <Windows.h>

class IconHandle {
public:
    IconHandle(HICON icon) : m_icon(icon){
    }
    ~IconHandle() {
    }

    void Swap(IconHandle & other) {
        std::swap(m_icon, other.m_icon);
    }

    void Activate(HWND wnd, bool as_big) {
        assert(m_icon);
        LRESULT res = 0;
        if (as_big)
            res = SendMessageW(wnd, WM_SETICON, ICON_BIG, (LPARAM)m_icon);   // update windws taskbar at bottom om screen
        else
            res = SendMessageW(wnd, WM_SETICON, ICON_SMALL, (LPARAM)m_icon); // update window titlebar (top-left icon)
        // res contains prev icon handle
    }
private:
    HICON m_icon = 0;
};
