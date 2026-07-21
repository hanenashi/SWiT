#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <cwchar>
#include <share.h>
#include <string>

#include "swit_protocol.h"
#include "swit_resources.h"

namespace {

constexpr DWORD kSwitShutdownLevel = 0x3FF;
constexpr DWORD kSwitShutdownFlags = 0;
constexpr wchar_t kSingleInstanceMutexName[] =
    L"Local\\SWiT.Agent.SingleInstance";
constexpr wchar_t kRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kDefaultAutostartValueName[] = L"SWiT";
constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kTrayRetryTimerId = 1;
constexpr UINT kCommandToggleProtection = 1001;
constexpr UINT kCommandToggleStartup = 1002;
constexpr UINT kCommandExit = 1003;
// Windows binds GUID-based tray identities to the executable path. Keep the
// development identity separate so local builds cannot claim the installed app.
#ifdef NDEBUG
constexpr GUID kTrayIconGuid = {
    0x0168a881,
    0x7b8e,
    0x48f3,
    {0xb0, 0x5c, 0xa4, 0x9c, 0xf5, 0xda, 0x59, 0x96},
};
#else
constexpr GUID kTrayIconGuid = {
    0x36d382d2,
    0x33f4,
    0x4b12,
    {0xad, 0xbb, 0x61, 0x80, 0x4a, 0xee, 0x21, 0x26},
};
#endif

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
UINT g_test_message = 0;
UINT g_taskbar_created_message = 0;
FILE* g_log = nullptr;
HANDLE g_single_instance_mutex = nullptr;
bool g_test_mode = false;
bool g_block_reason_created = false;
bool g_tray_icon_added = false;
std::wstring g_autostart_value_name = kDefaultAutostartValueName;

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
    PWSTR localAppData = nullptr;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT,
                                          nullptr, &localAppData);
    if (FAILED(result) || localAppData == nullptr) {
        if (localAppData != nullptr) {
            CoTaskMemFree(localAppData);
        }
        return {};
    }

    std::wstring path = localAppData;
    CoTaskMemFree(localAppData);
    path += L"\\SWiT\\logs\\swit-agent.log";
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

    int result = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    return result == ERROR_SUCCESS || result == ERROR_FILE_EXISTS ||
           result == ERROR_ALREADY_EXISTS;
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
    case SWIT_CONTROL_ENABLE_PROTECTION:
        return L"ENABLE_PROTECTION";
    case SWIT_CONTROL_DISABLE_PROTECTION:
        return L"DISABLE_PROTECTION";
    case SWIT_CONTROL_ENABLE_STARTUP:
        return L"ENABLE_STARTUP";
    case SWIT_CONTROL_DISABLE_STARTUP:
        return L"DISABLE_STARTUP";
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

bool DestroyShutdownBlockReason(HWND hwnd) {
    if (!g_block_reason_created) {
        return true;
    }

    if (!ShutdownBlockReasonDestroy(hwnd)) {
        Log(L"ShutdownBlockReasonDestroy failed: %ls",
            LastErrorText(GetLastError()).c_str());
        return false;
    }

    g_block_reason_created = false;
    Log(L"ShutdownBlockReasonDestroy succeeded");
    return true;
}

HICON LoadTrayIcon() {
    HICON icon = nullptr;
    if (g_query_mode == QueryMode::Block) {
        icon = static_cast<HICON>(LoadImageW(
            g_instance, MAKEINTRESOURCEW(IDI_SWIT_ICON), IMAGE_ICON, 0, 0,
            LR_DEFAULTSIZE | LR_SHARED));
    } else {
        icon = LoadIconW(nullptr, IDI_WARNING);
    }
    if (icon == nullptr) {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    return icon;
}

void SetTrayTip(NOTIFYICONDATAW* data) {
    const wchar_t* tip = g_query_mode == QueryMode::Block
                             ? L"SWiT - protection enabled"
                             : L"SWiT - protection disabled";
    wcscpy_s(data->szTip, tip);
}

bool AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd;
    data.uID = kTrayIconId;
    data.uFlags =
        NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_GUID;
    data.uCallbackMessage = kTrayCallbackMessage;
    data.hIcon = LoadTrayIcon();
    data.guidItem = kTrayIconGuid;
    SetTrayTip(&data);

    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        Log(L"Shell_NotifyIcon NIM_ADD failed: %ls",
            LastErrorText(GetLastError()).c_str());
        SetTimer(hwnd, kTrayRetryTimerId, 2000, nullptr);
        return false;
    }

    data.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &data)) {
        Log(L"Shell_NotifyIcon NIM_SETVERSION failed: %ls",
            LastErrorText(GetLastError()).c_str());
    }

    KillTimer(hwnd, kTrayRetryTimerId);
    g_tray_icon_added = true;
    Log(L"tray icon added");
    return true;
}

void UpdateTrayIcon(HWND hwnd) {
    if (!g_tray_icon_added) {
        AddTrayIcon(hwnd);
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd;
    data.uID = kTrayIconId;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_GUID;
    data.hIcon = LoadTrayIcon();
    data.guidItem = kTrayIconGuid;
    SetTrayTip(&data);

    if (!Shell_NotifyIconW(NIM_MODIFY, &data)) {
        Log(L"Shell_NotifyIcon NIM_MODIFY failed: %ls",
            LastErrorText(GetLastError()).c_str());
        g_tray_icon_added = false;
        SetTimer(hwnd, kTrayRetryTimerId, 2000, nullptr);
    }
}

void RemoveTrayIcon(HWND hwnd) {
    KillTimer(hwnd, kTrayRetryTimerId);
    if (!g_tray_icon_added) {
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd;
    data.uID = kTrayIconId;
    data.uFlags = NIF_GUID;
    data.guidItem = kTrayIconGuid;
    Shell_NotifyIconW(NIM_DELETE, &data);
    g_tray_icon_added = false;
    Log(L"tray icon removed");
}

bool SetProtectionEnabled(HWND hwnd, bool enabled, const wchar_t* source) {
    bool currentlyEnabled = g_query_mode == QueryMode::Block;
    if (enabled == currentlyEnabled) {
        Log(L"protection unchanged source=%ls enabled=%u", source,
            enabled ? 1u : 0u);
        return true;
    }

    if (enabled) {
        if (!CreateShutdownBlockReason(hwnd)) {
            Log(L"protection enable failed source=%ls", source);
            return false;
        }
        g_query_mode = QueryMode::Block;
    } else {
        if (!DestroyShutdownBlockReason(hwnd)) {
            Log(L"protection disable failed source=%ls", source);
            return false;
        }
        g_query_mode = QueryMode::Allow;
    }

    Log(L"protection changed source=%ls enabled=%u", source,
        enabled ? 1u : 0u);
    UpdateTrayIcon(hwnd);
    return true;
}

bool CurrentExecutablePath(std::wstring* path) {
    wchar_t module[32768]{};
    DWORD length = GetModuleFileNameW(nullptr, module, _countof(module));
    if (length == 0 || length >= _countof(module)) {
        Log(L"GetModuleFileName for startup failed: %ls",
            LastErrorText(GetLastError()).c_str());
        return false;
    }

    *path = module;
    return true;
}

bool QueryAutostart(bool* enabled, std::wstring* command) {
    wchar_t value[32768]{};
    DWORD bytes = sizeof(value);
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER, kRunKeyPath, g_autostart_value_name.c_str(),
        RRF_RT_REG_SZ, nullptr, value, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) {
        *enabled = false;
        command->clear();
        return true;
    }
    if (status != ERROR_SUCCESS) {
        Log(L"RegGetValue startup failed: %ls",
            LastErrorText(static_cast<DWORD>(status)).c_str());
        return false;
    }

    *enabled = true;
    *command = value;
    return true;
}

bool SetAutostartEnabled(bool enabled, const wchar_t* source) {
    if (enabled) {
        std::wstring modulePath;
        if (!CurrentExecutablePath(&modulePath)) {
            return false;
        }
        std::wstring command = L"\"" + modulePath + L"\"";

        HKEY key = nullptr;
        LSTATUS status = RegCreateKeyExW(
            HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &key, nullptr);
        if (status != ERROR_SUCCESS) {
            Log(L"RegCreateKey startup failed: %ls",
                LastErrorText(static_cast<DWORD>(status)).c_str());
            return false;
        }

        DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        status = RegSetValueExW(
            key, g_autostart_value_name.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()), bytes);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) {
            Log(L"RegSetValue startup failed: %ls",
                LastErrorText(static_cast<DWORD>(status)).c_str());
            return false;
        }

        Log(L"startup changed source=%ls enabled=1 value=%ls command=%ls",
            source, g_autostart_value_name.c_str(), command.c_str());
        return true;
    }

    HKEY key = nullptr;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0,
                                   KEY_SET_VALUE, &key);
    if (status == ERROR_FILE_NOT_FOUND) {
        Log(L"startup unchanged source=%ls enabled=0 value=%ls", source,
            g_autostart_value_name.c_str());
        return true;
    }
    if (status != ERROR_SUCCESS) {
        Log(L"RegOpenKey startup failed: %ls",
            LastErrorText(static_cast<DWORD>(status)).c_str());
        return false;
    }

    status = RegDeleteValueW(key, g_autostart_value_name.c_str());
    RegCloseKey(key);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
        Log(L"RegDeleteValue startup failed: %ls",
            LastErrorText(static_cast<DWORD>(status)).c_str());
        return false;
    }

    Log(L"startup changed source=%ls enabled=0 value=%ls", source,
        g_autostart_value_name.c_str());
    return true;
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        Log(L"CreatePopupMenu failed: %ls", LastErrorText(GetLastError()).c_str());
        return;
    }

    UINT protectionFlags = MF_STRING;
    if (g_query_mode == QueryMode::Block) {
        protectionFlags |= MF_CHECKED;
    }
    AppendMenuW(menu, protectionFlags, kCommandToggleProtection,
                L"Protection enabled");

    bool autostartEnabled = false;
    std::wstring autostartCommand;
    QueryAutostart(&autostartEnabled, &autostartCommand);
    UINT startupFlags = MF_STRING;
    if (autostartEnabled) {
        startupFlags |= MF_CHECKED;
    }
    AppendMenuW(menu, startupFlags, kCommandToggleStartup,
                L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenuEx(menu,
                                    TPM_RIGHTBUTTON | TPM_BOTTOMALIGN |
                                        TPM_RETURNCMD,
                                    point.x, point.y, hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    if (command == kCommandToggleProtection) {
        bool enabled = g_query_mode != QueryMode::Block;
        if (!SetProtectionEnabled(hwnd, enabled, L"tray")) {
            MessageBoxW(hwnd, L"SWiT could not change protection state.",
                        L"SWiT", MB_OK | MB_ICONERROR | MB_TOPMOST);
        }
    } else if (command == kCommandToggleStartup) {
        if (!SetAutostartEnabled(!autostartEnabled, L"tray")) {
            MessageBoxW(hwnd, L"SWiT could not change its startup setting.",
                        L"SWiT", MB_OK | MB_ICONERROR | MB_TOPMOST);
        }
    } else if (command == kCommandExit) {
        DestroyWindow(hwnd);
    }
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
    case SWIT_CONTROL_ENABLE_PROTECTION:
        SetProtectionEnabled(g_window, true, L"swit-send");
        break;
    case SWIT_CONTROL_DISABLE_PROTECTION:
        SetProtectionEnabled(g_window, false, L"swit-send");
        break;
    case SWIT_CONTROL_ENABLE_STARTUP:
        SetAutostartEnabled(true, L"swit-send");
        break;
    case SWIT_CONTROL_DISABLE_STARTUP:
        SetAutostartEnabled(false, L"swit-send");
        break;
    case SWIT_TEST_EXIT:
        Log(L"test requested exit");
        DestroyWindow(g_window);
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
    if (g_taskbar_created_message != 0 &&
        message == g_taskbar_created_message) {
        Log(L"TaskbarCreated received");
        g_tray_icon_added = false;
        AddTrayIcon(hwnd);
        return 0;
    }
    if (message == kTrayCallbackMessage) {
        UINT event = LOWORD(lparam);
        if (event == WM_CONTEXTMENU || event == NIN_SELECT ||
            event == NIN_KEYSELECT || event == WM_LBUTTONUP ||
            event == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
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
        AddTrayIcon(hwnd);
        return 0;

    case WM_CLOSE:
        Log(L"WM_CLOSE");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        Log(L"WM_DESTROY");
        RemoveTrayIcon(hwnd);
        DestroyShutdownBlockReason(hwnd);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wparam == kTrayRetryTimerId && !g_tray_icon_added) {
            AddTrayIcon(hwnd);
        }
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

    SetLastError(ERROR_SUCCESS);
    g_single_instance_mutex =
        CreateMutexW(nullptr, FALSE, kSingleInstanceMutexName);
    if (g_single_instance_mutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_single_instance_mutex);
        g_single_instance_mutex = nullptr;
        return 0;
    }

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring logPath = ArgValue(argc, argv, L"--log");
    if (logPath.empty()) {
        logPath = DefaultLogPath();
    }

    std::wstring autostartValueName =
        ArgValue(argc, argv, L"--autostart-value-name");
    if (!autostartValueName.empty()) {
        g_autostart_value_name = autostartValueName;
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
    Log(L"single instance acquired name=%ls", kSingleInstanceMutexName);
    Log(L"log path=%ls", logPath.c_str());
    Log(L"test mode=%u", g_test_mode ? 1u : 0u);
    Log(L"query mode=%ls", QueryModeName(g_query_mode));
    bool autostartEnabled = false;
    std::wstring autostartCommand;
    if (QueryAutostart(&autostartEnabled, &autostartCommand)) {
        Log(L"startup setting enabled=%u value=%ls command=%ls",
            autostartEnabled ? 1u : 0u, g_autostart_value_name.c_str(),
            autostartCommand.empty() ? L"none" : autostartCommand.c_str());
    }

    g_test_message = RegisterWindowMessageW(SWIT_TEST_MESSAGE_NAME);
    if (g_test_message == 0) {
        Log(L"RegisterWindowMessage failed: %ls", LastErrorText(GetLastError()).c_str());
        if (argv != nullptr) {
            LocalFree(argv);
        }
        return 1;
    }
    Log(L"registered test message id=%u", g_test_message);

    g_taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    if (g_taskbar_created_message == 0) {
        Log(L"RegisterWindowMessage TaskbarCreated failed: %ls",
            LastErrorText(GetLastError()).c_str());
    } else {
        Log(L"registered TaskbarCreated message id=%u",
            g_taskbar_created_message);
    }

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

    if (g_single_instance_mutex != nullptr) {
        CloseHandle(g_single_instance_mutex);
        g_single_instance_mutex = nullptr;
    }

    if (argv != nullptr) {
        LocalFree(argv);
    }
    return static_cast<int>(msg.wParam);
}
