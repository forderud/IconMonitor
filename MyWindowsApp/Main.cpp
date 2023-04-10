#include <cassert>
#include <iostream>
#include <string>
#include "IconHandle.hpp"


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static IconHandle m_titlebar(LoadIconW(NULL, IDI_EXCLAMATION), false);
static IconHandle m_taskbar(LoadIconW(NULL, IDI_ERROR), false);


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"My Window Class";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    DWORD thread_id = GetCurrentThreadId();
    std::wstring window_title = L"My Windows app (thread " + std::to_wstring(thread_id) + L")";
    {
        // copy thread ID to clipboard
        std::string str = std::to_string(thread_id); // in decimal format
        // copy string to separate buffer
        const size_t len = str.size() + 1;
        HGLOBAL buf = GlobalAlloc(GMEM_MOVEABLE, len);
        memcpy(GlobalLock(buf), str.c_str(), len);
        GlobalUnlock(buf);
        // move buffer to clipboard
        OpenClipboard(0);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, buf); // transfer ownership to clipboard
        buf = nullptr;
        CloseClipboard();
    }

    // Create the window.
    POINT position = {CW_USEDEFAULT, CW_USEDEFAULT};
    POINT size = {600, 400};

    HWND hwnd = CreateWindowEx(0, CLASS_NAME,
        window_title.c_str(),
        WS_OVERLAPPEDWINDOW,    // Window style
        position.x, position.y, size.x, size.y,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );
    assert(hwnd);

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
        }
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
