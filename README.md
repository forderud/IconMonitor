# IconMonitor

Sample project of DLL injection using [Windows hooks](https://learn.microsoft.com/en-us/windows/win32/winmsg/hooks) for intercepting icon update messages in different processes. The intercepted messages are communicated back through a [Windows pipe](https://learn.microsoft.com/en-us/windows/win32/ipc/using-pipes).


## How to test
* Start `MyWindowsApp.exe`. This will copy the window handle (HWND) for the application into the clipboard.
* Start `IconMonitorClient.exe <HWND>` to start the client application that monitors the windows app.
* Click inside the `MyWindowsApp` window body.
* Observe that the titlebar and Windows taskbar icons are updated.
* Observe that `IconMonitorClient` reports icon update events.
