#include <iostream>
#include <Windows.h>
#include "..\IconMonitorClient\Message.hpp"


int main() {
    // TODO: Consider replacing with CallNamedPipeW
    HANDLE pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
        0,              // no sharing 
        NULL,           // default security attributes
        OPEN_EXISTING,  // opens existing pipe 
        0,              // default attributes 
        NULL);          // no template file 
    if (pipe == INVALID_HANDLE_VALUE) {
        printf("ERROR: Unable to connect to pipe. GLE=%d\n", GetLastError());
        return -1;
    }

    // change pipe from byte- to message-mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    BOOL ok = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
    if (!ok) {
        printf("SetNamedPipeHandleState failed. GLE=%d\n", GetLastError());
        return -1;
    }

    for (size_t i = 0; i < 10; ++i) {
        IconUpdateMessage msg;
        msg.window = (HWND)41;
        msg.param = i;
        msg.icon = (HICON)1024;

        // write message to pipe
        DWORD bWritten = 0;
        ok = WriteFile(pipe, &msg, sizeof(msg), &bWritten, NULL);
        if (!ok) {
            printf("WriteFile to pipe failed. GLE=%d\n", GetLastError());
            return -1;
        }

        printf("Sent message.\n");
    }

    CloseHandle(pipe);
    return 0;
}
