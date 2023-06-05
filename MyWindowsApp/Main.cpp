#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include "IconHandle.hpp"


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static IconHandle m_titlebar;
static IconHandle m_taskbar;


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"My Window Class";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Create the window.
    POINT position = {CW_USEDEFAULT, CW_USEDEFAULT};
    POINT size = {600, 400};

    HWND hwnd = CreateWindowEx(0, CLASS_NAME,
        CLASS_NAME,
        WS_OVERLAPPEDWINDOW,    // Window style
        position.x, position.y, size.x, size.y,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );
    assert(hwnd);

    std::stringstream hwnd_str; // in ASCII hex format
    hwnd_str << std::hex << (size_t)hwnd;

    std::string window_title = "My Windows app (HWND " + hwnd_str.str() + ")";
    SetWindowTextA(hwnd, window_title.c_str());

    {
        // copy window handle to clipboard
        // copy ASCII string to separate buffer
        const size_t len = hwnd_str.str().size() + 1;
        HGLOBAL buf = GlobalAlloc(GMEM_MOVEABLE, len);
        memcpy(GlobalLock(buf), hwnd_str.str().c_str(), len);
        GlobalUnlock(buf);
        // move buffer to clipboard
        OpenClipboard(0);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, buf); // transfer ownership to clipboard
        buf = nullptr;
        CloseClipboard();
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!m_titlebar) {
        m_titlebar = IconHandle(LoadIconW(NULL, IDI_EXCLAMATION), L"Title 1", false);
        m_taskbar = IconHandle(CreateIconFromRGB(hwnd), L"Title 2", true);
    }

    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            // All painting occurs here, between BeginPaint and EndPaint
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
        }
        return 0;
    case WM_LBUTTONDOWN:
        {
            // swap & update icons
            m_titlebar.Swap(m_taskbar);
            m_titlebar.Activate(hwnd, ICON_BIG);
            m_taskbar.Activate(hwnd, ICON_SMALL);

            // update window title (will trigger a WM_SETTEXT message)
            SetWindowTextW(hwnd, m_titlebar.Title());
        }
        break;
    case WM_POWERBROADCAST:
        {
            if (wParam == PBT_APMSUSPEND) {
                assert(!lParam);
                SetWindowTextW(hwnd, L"System is suspending");
            } else if (wParam == PBT_APMRESUMESUSPEND) {
                assert(!lParam);
                SetWindowTextW(hwnd, L"Resuming from low-power state");
            } else if (wParam == PBT_APMRESUMEAUTOMATIC) {
                assert(!lParam);
                SetWindowTextW(hwnd, L"Resuming automatically from low-power state");
            }
        }
        break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
