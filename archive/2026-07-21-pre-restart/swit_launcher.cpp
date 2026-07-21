#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")

void LogMessage(const wchar_t* message) {
    FILE* file;
    _wfopen_s(&file, L"C:\\GIT\\SWiT\\swit_log.txt", L"a");
    if (file) {
        fwprintf(file, L"%s\n", message);
        fclose(file);
    }
}

int wmain() {
    LogMessage(L"Launcher starting");
    HMODULE hDll = LoadLibrary(L"C:\\GIT\\SWiT\\swit_hook.dll");
    if (!hDll) {
        wchar_t errMsg[256];
        swprintf_s(errMsg, 256, L"Failed to load DLL, error: %lu", GetLastError());
        LogMessage(errMsg);
        return 1;
    }

    typedef void (*InstallHookFunc)();
    InstallHookFunc InstallHook = (InstallHookFunc)GetProcAddress(hDll, "InstallHook");
    if (!InstallHook) {
        LogMessage(L"Failed to get InstallHook address");
        FreeLibrary(hDll);
        return 1;
    }

    InstallHook();
    LogMessage(L"Launcher running, press Ctrl+C to exit");
    Sleep(INFINITE); // Keep it alive

    typedef void (*UninstallHookFunc)();
    UninstallHookFunc UninstallHook = (UninstallHookFunc)GetProcAddress(hDll, "UninstallHook");
    if (UninstallHook) UninstallHook();
    FreeLibrary(hDll);
    return 0;
}