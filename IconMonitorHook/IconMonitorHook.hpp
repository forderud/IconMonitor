#pragma once
#include <cassert>
#include <Windows.h>
#include "Message.hpp"

/** Client-side hook function setup. */
class InconMonitorHook {
public:
    InconMonitorHook(DWORD thread_id) {
        m_module = LoadLibraryW(L"IconMonitorHook.dll"); // DLL handle
        assert(m_module);
        auto callback = (HOOKPROC)GetProcAddress(m_module, "Hookproc");

        // might fail on invalid thread_id
        m_hook = SetWindowsHookExW(WH_CALLWNDPROCRET, callback, m_module, thread_id);
    }
    ~InconMonitorHook() {
        if (m_hook) {
            UnhookWindowsHookEx(m_hook);
            m_hook = 0;
        }

        FreeLibrary(m_module);
        m_module = 0;
    }

    operator bool() const {
        return m_hook;
    }

private:
    HINSTANCE m_module = 0;
    HHOOK     m_hook = 0;
};


class IconQuery {
public:
    IconQuery(HWND wnd) : m_wnd(wnd) {
    }

    std::wstring GetText() const {
        // query titlebar text
        wchar_t text[1024] = {};
        int res = GetWindowTextW(m_wnd, text, (int)std::size(text));
        assert(res);
        return text;
    }

    HICON GetIcon() const {
        // query windws taskbar icon (ICON_BIG)
        auto icon = (HICON)SendMessageW(m_wnd, WM_GETICON, ICON_BIG, 0);
        return icon;
    }

private:
    HWND m_wnd = 0;
};