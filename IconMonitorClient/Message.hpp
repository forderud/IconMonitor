#pragma once
#include <string>

static constexpr wchar_t PIPE_NAME[] = L"\\\\.\\pipe\\IconMonitor";
static constexpr DWORD PIPE_TIMEOUT = 5000;

struct IconUpdateMessage {
    HWND   window = 0;
    WPARAM param = 0;
    HICON  icon = 0;

    std::string ToString() const {
        std::string result = "[IconUpdateMessage] window=" + std::to_string((size_t)window)
            + ", param=" + std::to_string(param)
            + ", icon=" + std::to_string((size_t)icon) + "\n";
        return result;
    }
};
