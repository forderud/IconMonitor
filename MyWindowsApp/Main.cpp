#include <iostream>
#include <string>
#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"My Window Class";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    DWORD thread_id = GetCurrentThreadId();
    std::wstring window_title = L"My Windows app (thread " + std::to_wstring(thread_id) + L")";

    // Create the window.
    HWND hwnd = CreateWindowEx(0, CLASS_NAME,
        window_title.c_str(),
        WS_OVERLAPPEDWINDOW,    // Window style
        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
        return 0;

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.

    MSG msg = { };
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
            // update app icon
            HICON icon_titlebar = LoadIconW(NULL, IDI_EXCLAMATION);
            HICON icon_taskbar = LoadIconW(NULL, IDI_ERROR);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon_titlebar); // update window titlebar (top-left icon)
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon_taskbar);    // update windws taskbar at bottom om screen
        }
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
