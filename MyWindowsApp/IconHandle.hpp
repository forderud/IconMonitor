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

        if (as_big)
            SendMessageW(wnd, WM_SETICON, ICON_BIG, (LPARAM)m_icon);   // update windws taskbar at bottom om screen
        else
            SendMessageW(wnd, WM_SETICON, ICON_SMALL, (LPARAM)m_icon); // update window titlebar (top-left icon)
    }
private:
    HICON m_icon = 0;
};
