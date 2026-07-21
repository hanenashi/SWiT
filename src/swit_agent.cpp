#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <share.h>
#include <string>

#include "swit_protocol.h"

namespace {

constexpr DWORD kSwitShutdownLevel = 0x3FF;
constexpr DWORD kSwitShutdownFlags = 0;

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
UINT g_test_message = 0;
FILE* g_log = nullptr;
bool g_test_mode = false;
bool g_block_reason_created = false;

enum class QueryMode {
    Block,
    Allow,
};

enum class EndSessionKind {
    Unknown,
    Shutdown,
    Restart,
    Logoff,
};

QueryMode g_query_mode = QueryMode::Block;

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

    wchar_t message[2048]{};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);
    va_end(args);

    std::wstring line = L"[" + NowText() + L"] " + message + L"\r\n";
    int byteCount = WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
                                        static_cast<int>(line.size()), nullptr,
                                        0, nullptr, nullptr);
    if (byteCount <= 0) {
        return;
    }

    std::string utf8(static_cast<size_t>(byteCount), '\0');
    WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
                        static_cast<int>(line.size()), &utf8[0], byteCount,
                        nullptr, nullptr);
    fwrite(utf8.data(), 1, utf8.size(), g_log);
    fflush(g_log);
}

std::wstring DefaultLogPath() {
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring path = module;
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash);
    }
    path += L"\\..\\logs\\swit-agent.log";
    return path;
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

    if (CreateDirectoryW(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }

    return false;
}

bool OpenLog(const std::wstring& path) {
    EnsureParentDirectory(path);
    g_log = _wfsopen(path.c_str(), L"ab", _SH_DENYWR);
    return g_log != nullptr;
}

const wchar_t* TestCommandName(WPARAM command) {
    switch (command) {
    case SWIT_TEST_PING:
        return L"PING";
    case SWIT_TEST_QUERY_SHUTDOWN:
        return L"QUERY_SHUTDOWN";
    case SWIT_TEST_QUERY_RESTART:
        return L"QUERY_RESTART";
    case SWIT_TEST_QUERY_LOGOFF:
        return L"QUERY_LOGOFF";
    case SWIT_TEST_EXIT:
        return L"EXIT";
    default:
        return L"UNKNOWN";
    }
}

const wchar_t* QueryModeName(QueryMode mode) {
    switch (mode) {
    case QueryMode::Block:
        return L"block";
    case QueryMode::Allow:
        return L"allow";
    }
    return L"unknown";
}

const wchar_t* EndSessionKindName(EndSessionKind kind) {
    switch (kind) {
    case EndSessionKind::Shutdown:
        return L"shutdown";
    case EndSessionKind::Restart:
        return L"restart";
    case EndSessionKind::Logoff:
        return L"logoff";
    case EndSessionKind::Unknown:
        return L"shutdown-or-restart";
    }
    return L"unknown";
}

std::wstring EndSessionReasonText(LPARAM reason) {
    if (reason == 0) {
        return L"none";
    }

    std::wstring text;
    if ((reason & ENDSESSION_CLOSEAPP) != 0) {
        text += L"CLOSEAPP ";
    }
    if ((reason & ENDSESSION_CRITICAL) != 0) {
        text += L"CRITICAL ";
    }
    if ((reason & ENDSESSION_LOGOFF) != 0) {
        text += L"LOGOFF ";
    }
    if (text.empty()) {
        text = L"unknown";
    } else if (text.back() == L' ') {
        text.pop_back();
    }
    return text;
}

bool ConfigureShutdownOrder() {
    DWORD beforeLevel = 0;
    DWORD beforeFlags = 0;
    if (GetProcessShutdownParameters(&beforeLevel, &beforeFlags)) {
        Log(L"shutdown parameters before: level=0x%03lX flags=0x%08lX",
            beforeLevel, beforeFlags);
    } else {
        Log(L"GetProcessShutdownParameters before failed: %ls",
            LastErrorText(GetLastError()).c_str());
    }

    if (!SetProcessShutdownParameters(kSwitShutdownLevel, kSwitShutdownFlags)) {
        Log(L"SetProcessShutdownParameters(0x%03lX, 0x%08lX) failed: %ls",
            kSwitShutdownLevel, kSwitShutdownFlags,
            LastErrorText(GetLastError()).c_str());
        return false;
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

    return true;
}

bool CreateShutdownBlockReason(HWND hwnd) {
    if (ShutdownBlockReasonCreate(
            hwnd,
            L"SWiT caught this session-end request. Choose Cancel to keep "
            L"working, or continue anyway to proceed.")) {
        Log(L"ShutdownBlockReasonCreate succeeded");
        g_block_reason_created = true;
        return true;
    }

    Log(L"ShutdownBlockReasonCreate failed: %ls",
        LastErrorText(GetLastError()).c_str());
    return false;
}

bool DecideShutdown() {
    return g_query_mode == QueryMode::Allow;
}

LRESULT HandleShutdownQuery(LPARAM reason) {
    bool allow = DecideShutdown();

    Log(L"WM_QUERYENDSESSION reason=0x%p flags=%ls mode=%ls decision=%ls",
        reinterpret_cast<void*>(reason), EndSessionReasonText(reason).c_str(),
        QueryModeName(g_query_mode), allow ? L"allow" : L"cancel");
    if ((reason & ENDSESSION_CRITICAL) != 0 && !allow) {
        Log(L"warning: ENDSESSION_CRITICAL is forced shutdown; cancel may be ignored");
    }
    return allow ? TRUE : FALSE;
}

void HandleTestMessage(WPARAM command, LPARAM value) {
    Log(L"test message command=%ls(%Iu) value=0x%p", TestCommandName(command),
        static_cast<size_t>(command), reinterpret_cast<void*>(value));

    bool isSyntheticQuery = command == SWIT_TEST_QUERY_SHUTDOWN ||
                            command == SWIT_TEST_QUERY_RESTART ||
                            command == SWIT_TEST_QUERY_LOGOFF;
    if (isSyntheticQuery && !g_test_mode) {
        Log(L"ignoring synthetic query because --test-mode is not enabled");
        return;
    }

    switch (command) {
    case SWIT_TEST_QUERY_SHUTDOWN:
    case SWIT_TEST_QUERY_RESTART:
    case SWIT_TEST_QUERY_LOGOFF: {
        EndSessionKind kind = EndSessionKind::Shutdown;
        if (command == SWIT_TEST_QUERY_RESTART) {
            kind = EndSessionKind::Restart;
        } else if (command == SWIT_TEST_QUERY_LOGOFF) {
            kind = EndSessionKind::Logoff;
        }

        bool allow = DecideShutdown();
        Log(L"synthetic query kind=%ls mode=%ls decision=%ls",
            EndSessionKindName(kind), QueryModeName(g_query_mode),
            allow ? L"allow" : L"cancel");
        break;
    }
    case SWIT_TEST_EXIT:
        Log(L"test requested exit");
        PostQuitMessage(0);
        break;
    default:
        break;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == g_test_message) {
        HandleTestMessage(wparam, lparam);
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        Log(L"WM_CREATE hwnd=0x%p", hwnd);
        if (g_query_mode == QueryMode::Block) {
            if (!CreateShutdownBlockReason(hwnd)) {
                return -1;
            }
        } else {
            Log(L"shutdown block reason disabled in allow mode");
        }
        return 0;

    case WM_CLOSE:
        Log(L"WM_CLOSE");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        Log(L"WM_DESTROY");
        if (g_block_reason_created) {
            ShutdownBlockReasonDestroy(hwnd);
            g_block_reason_created = false;
        }
        PostQuitMessage(0);
        return 0;

    case WM_QUERYENDSESSION:
        return HandleShutdownQuery(lparam);

    case WM_ENDSESSION:
        Log(L"WM_ENDSESSION ending=%Iu reason=0x%p flags=%ls",
            static_cast<size_t>(wparam), reinterpret_cast<void*>(lparam),
            EndSessionReasonText(lparam).c_str());
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
    wc.lpszClassName = SWIT_WINDOW_CLASS;

    if (RegisterClassExW(&wc) == 0) {
        Log(L"RegisterClassEx failed: %ls", LastErrorText(GetLastError()).c_str());
        return false;
    }

    Log(L"registered window class %ls", SWIT_WINDOW_CLASS);
    return true;
}

bool CreateAgentWindow() {
    g_window = CreateWindowExW(0, SWIT_WINDOW_CLASS, L"SWiT Agent",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                              g_instance, nullptr);
    if (g_window == nullptr) {
        Log(L"CreateWindowEx failed: %ls", LastErrorText(GetLastError()).c_str());
        return false;
    }

    Log(L"created hidden top-level window hwnd=0x%p", g_window);
    return true;
}

std::wstring ArgValue(int argc, wchar_t** argv, const wchar_t* name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (wcscmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return L"";
}

bool HasArg(int argc, wchar_t** argv, const wchar_t* name) {
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
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

    g_test_mode = HasArg(argc, argv, L"--test-mode");
    bool cancelOnQuery = HasArg(argc, argv, L"--cancel-on-query");
    bool allowOnQuery = HasArg(argc, argv, L"--allow-on-query");
    if (cancelOnQuery && allowOnQuery) {
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 2;
    }
    if (cancelOnQuery) {
        g_query_mode = QueryMode::Block;
    } else if (allowOnQuery) {
        g_query_mode = QueryMode::Allow;
    }

    if (!OpenLog(logPath)) {
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }

    Log(L"SWiT agent starting");
    Log(L"log path=%ls", logPath.c_str());
    Log(L"test mode=%u", g_test_mode ? 1u : 0u);
    Log(L"query mode=%ls", QueryModeName(g_query_mode));

    g_test_message = RegisterWindowMessageW(SWIT_TEST_MESSAGE_NAME);
    if (g_test_message == 0) {
        Log(L"RegisterWindowMessage failed: %ls", LastErrorText(GetLastError()).c_str());
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }
    Log(L"registered test message id=%u", g_test_message);

    bool shutdownOrderOk = ConfigureShutdownOrder();
    if (!shutdownOrderOk || !RegisterWindowClass() || !CreateAgentWindow()) {
        if (!shutdownOrderOk) {
            Log(L"fatal: application-first shutdown order is required");
        }
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }

    Log(L"agent ready shutdown_order_ok=%u", shutdownOrderOk ? 1u : 0u);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Log(L"SWiT agent exiting code=%ld", static_cast<long>(msg.wParam));
    if (g_log != nullptr) {
        fclose(g_log);
        g_log = nullptr;
    }

    if (argv != nullptr) {
        LocalFree(argv);
    }
    return static_cast<int>(msg.wParam);
}
