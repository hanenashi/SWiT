#include <windows.h>

#include <cwchar>
#include <iostream>

#include "swit_protocol.h"

namespace {

unsigned int ParseCommand(const wchar_t* value) {
    if (wcscmp(value, L"ping") == 0) {
        return SWIT_TEST_PING;
    }
    if (wcscmp(value, L"shutdown") == 0) {
        return SWIT_TEST_QUERY_SHUTDOWN;
    }
    if (wcscmp(value, L"restart") == 0) {
        return SWIT_TEST_QUERY_RESTART;
    }
    if (wcscmp(value, L"logoff") == 0) {
        return SWIT_TEST_QUERY_LOGOFF;
    }
    if (wcscmp(value, L"exit") == 0) {
        return SWIT_TEST_EXIT;
    }
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::wcerr << L"Usage: swit-send <ping|shutdown|restart|logoff|exit>\n";
        return 2;
    }

    unsigned int command = ParseCommand(argv[1]);
    if (command == 0) {
        std::wcerr << L"Unsupported command: " << argv[1] << L"\n";
        return 2;
    }

    HWND window = FindWindowW(SWIT_WINDOW_CLASS, nullptr);
    if (window == nullptr) {
        std::wcerr << L"SWiT agent window not found\n";
        return 1;
    }

    UINT message = RegisterWindowMessageW(SWIT_TEST_MESSAGE_NAME);
    if (message == 0) {
        std::wcerr << L"RegisterWindowMessage failed: " << GetLastError() << L"\n";
        return 1;
    }

    if (!PostMessageW(window, message, command, 0)) {
        std::wcerr << L"PostMessage failed: " << GetLastError() << L"\n";
        return 1;
    }

    std::wcout << L"sent " << argv[1] << L"\n";
    return 0;
}
