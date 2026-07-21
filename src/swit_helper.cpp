#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <string>

#include "swit_protocol.h"

namespace {

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
UINT g_test_message = 0;
FILE* g_log = nullptr;
DWORD g_requested_level = 0;
bool g_has_requested_level = false;

std::wstring NowText() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
               st.wMilliseconds);
    return buffer;
}

std::wstring LastErrorText(DWORD error) {
    if (error == ERROR_SUCCESS) {
        return L"0";
    }

    wchar_t* message = nullptr;
    DWORD chars = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);

    std::wstring result = std::to_wstring(error);
    if (chars != 0 && message != nullptr) {
        result += L" ";
        result += message;
        while (!result.empty() &&
               (result.back() == L'\r' || result.back() == L'\n')) {
            result.pop_back();
        }
    }

    if (message != nullptr) {
        LocalFree(message);
    }
    return result;
}

void Log(const wchar_t* format, ...) {
    if (g_log == nullptr) {
        return;
    }

    fwprintf(g_log, L"[%ls] ", NowText().c_str());

    va_list args;
    va_start(args, format);
    vfwprintf(g_log, format, args);
    va_end(args);

    fwprintf(g_log, L"\n");
    fflush(g_log);
}

bool EnsureParentDirectory(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return true;
    }

    std::wstring dir = path.substr(0, slash);
    if (dir.empty()) {
        return true;
    }

    return CreateDirectoryW(dir.c_str(), nullptr) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring DefaultLogPath() {
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring path = module;
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash);
    }
    path += L"\\..\\logs\\swit-helper.log";
    return path;
}

bool OpenLog(const std::wstring& path) {
    EnsureParentDirectory(path);
    errno_t err = _wfopen_s(&g_log, path.c_str(), L"a, ccs=UTF-8");
    return err == 0 && g_log != nullptr;
}

std::wstring ArgValue(int argc, wchar_t** argv, const wchar_t* name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (wcscmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return L"";
}

bool ParseHexOrDecimal(const std::wstring& text, DWORD* value) {
    wchar_t* end = nullptr;
    unsigned long parsed = wcstoul(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != L'\0') {
        return false;
    }
    *value = static_cast<DWORD>(parsed);
    return true;
}

void ConfigureShutdownLevel() {
    DWORD beforeLevel = 0;
    DWORD beforeFlags = 0;
    if (GetProcessShutdownParameters(&beforeLevel, &beforeFlags)) {
        Log(L"shutdown parameters before: level=0x%03lX flags=0x%08lX",
            beforeLevel, beforeFlags);
    } else {
        Log(L"GetProcessShutdownParameters before failed: %ls",
            LastErrorText(GetLastError()).c_str());
    }

    if (!g_has_requested_level) {
        Log(L"leaving default shutdown level unchanged");
        return;
    }

    if (!SetProcessShutdownParameters(g_requested_level, 0)) {
        Log(L"SetProcessShutdownParameters(0x%03lX, 0) failed: %ls",
            g_requested_level, LastErrorText(GetLastError()).c_str());
        return;
    }

    DWORD afterLevel = 0;
    DWORD afterFlags = 0;
    if (GetProcessShutdownParameters(&afterLevel, &afterFlags)) {
        Log(L"shutdown parameters after: level=0x%03lX flags=0x%08lX",
            afterLevel, afterFlags);
    } else {
        Log(L"GetProcessShutdownParameters after failed: %ls",
            LastErrorText(GetLastError()).c_str());
    }
}

const wchar_t* TestCommandName(WPARAM command) {
    switch (command) {
    case SWIT_TEST_PING:
        return L"PING";
    case SWIT_TEST_EXIT:
        return L"EXIT";
    default:
        return L"OTHER";
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == g_test_message) {
        Log(L"test message command=%ls(%Iu) value=0x%p", TestCommandName(wparam),
            static_cast<size_t>(wparam), reinterpret_cast<void*>(lparam));
        if (wparam == SWIT_TEST_EXIT) {
            Log(L"test requested exit");
            PostQuitMessage(0);
        }
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        Log(L"WM_CREATE hwnd=0x%p", hwnd);
        return 0;

    case WM_CLOSE:
        Log(L"WM_CLOSE");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        Log(L"WM_DESTROY");
        PostQuitMessage(0);
        return 0;

    case WM_QUERYENDSESSION:
        Log(L"WM_QUERYENDSESSION reason=0x%p returning TRUE",
            reinterpret_cast<void*>(lparam));
        return TRUE;

    case WM_ENDSESSION:
        Log(L"WM_ENDSESSION ending=%Iu reason=0x%p", static_cast<size_t>(wparam),
            reinterpret_cast<void*>(lparam));
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

bool RegisterWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = g_instance;
    wc.lpszClassName = SWIT_HELPER_WINDOW_CLASS;

    if (RegisterClassExW(&wc) == 0) {
        Log(L"RegisterClassEx failed: %ls", LastErrorText(GetLastError()).c_str());
        return false;
    }

    Log(L"registered window class %ls", SWIT_HELPER_WINDOW_CLASS);
    return true;
}

bool CreateHelperWindow() {
    g_window = CreateWindowExW(0, SWIT_HELPER_WINDOW_CLASS, L"SWiT Helper",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                              g_instance, nullptr);
    if (g_window == nullptr) {
        Log(L"CreateWindowEx failed: %ls", LastErrorText(GetLastError()).c_str());
        return false;
    }

    Log(L"created hidden helper window hwnd=0x%p", g_window);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_instance = instance;

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring logPath = ArgValue(argc, argv, L"--log");
    if (logPath.empty()) {
        logPath = DefaultLogPath();
    }

    std::wstring levelText = ArgValue(argc, argv, L"--level");
    if (!levelText.empty()) {
        if (!ParseHexOrDecimal(levelText, &g_requested_level)) {
            if (argv != nullptr) {
                LocalFree(argv);
            }
            return 2;
        }
        g_has_requested_level = true;
    }

    if (!OpenLog(logPath)) {
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }

    Log(L"SWiT helper starting");
    Log(L"log path=%ls", logPath.c_str());

    g_test_message = RegisterWindowMessageW(SWIT_TEST_MESSAGE_NAME);
    if (g_test_message == 0) {
        Log(L"RegisterWindowMessage failed: %ls", LastErrorText(GetLastError()).c_str());
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }
    Log(L"registered test message id=%u", g_test_message);

    ConfigureShutdownLevel();

    if (!RegisterWindowClass() || !CreateHelperWindow()) {
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }

    Log(L"helper ready");

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Log(L"SWiT helper exiting code=%ld", static_cast<long>(msg.wParam));
    if (g_log != nullptr) {
        fclose(g_log);
        g_log = nullptr;
    }

    if (argv != nullptr) {
        LocalFree(argv);
    }
    return static_cast<int>(msg.wParam);
}
