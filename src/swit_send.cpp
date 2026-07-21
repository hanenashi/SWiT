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
    if (wcscmp(value, L"enable") == 0) {
        return SWIT_CONTROL_ENABLE_PROTECTION;
    }
    if (wcscmp(value, L"disable") == 0) {
        return SWIT_CONTROL_DISABLE_PROTECTION;
    }
    if (wcscmp(value, L"exit") == 0) {
        return SWIT_TEST_EXIT;
    }
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::wcerr << L"Usage: swit-send [--helper] "
                      L"<ping|shutdown|restart|logoff|enable|disable|exit>\n";
        return 2;
    }

    bool helper = false;
    const wchar_t* commandArg = argv[1];
    if (argc == 3) {
        if (wcscmp(argv[1], L"--helper") != 0) {
            std::wcerr << L"Unsupported option: " << argv[1] << L"\n";
            return 2;
        }
        helper = true;
        commandArg = argv[2];
    }

    unsigned int command = ParseCommand(commandArg);
    if (command == 0) {
        std::wcerr << L"Unsupported command: " << commandArg << L"\n";
        return 2;
    }
    if (helper && command != SWIT_TEST_PING && command != SWIT_TEST_EXIT) {
        std::wcerr << L"Helper supports only ping and exit\n";
        return 2;
    }

    HWND window = FindWindowW(helper ? SWIT_HELPER_WINDOW_CLASS : SWIT_WINDOW_CLASS, nullptr);
    if (window == nullptr) {
        std::wcerr << (helper ? L"SWiT helper window not found\n"
                              : L"SWiT agent window not found\n");
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

    std::wcout << L"sent " << commandArg << (helper ? L" to helper\n" : L"\n");
    return 0;
}
