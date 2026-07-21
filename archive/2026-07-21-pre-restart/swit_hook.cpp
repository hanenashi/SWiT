#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")

HHOOK g_hHook = NULL;
HINSTANCE g_hInst = NULL;

void LogMessage(const wchar_t* message) {
    FILE* file;
    _wfopen_s(&file, L"C:\\GIT\\SWiT\\swit_log.txt", L"a");
    if (file) {
        fwprintf(file, L"%s\n", message);
        fclose(file);
    }
}

LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        CWPSTRUCT* pCwp = (CWPSTRUCT*)lParam;
        wchar_t msgLog[256];
        swprintf_s(msgLog, 256, L"Message: 0x%X, wParam: 0x%X, lParam: 0x%X", pCwp->message, pCwp->wParam, pCwp->lParam);
        LogMessage(msgLog);

        if (pCwp->message == WM_COMMAND && LOWORD(pCwp->wParam) == 0x2FA) {
            LogMessage(L"Shutdown command intercepted (WM_COMMAND)");
            goto ShowPopup;
        }
        else if (pCwp->message == WM_SYSCOMMAND && (pCwp->wParam & 0xFFF0) == SC_CLOSE) {
            LogMessage(L"Shutdown command intercepted (WM_SYSCOMMAND)");
            goto ShowPopup;
        }
        else if (pCwp->message == WM_POWERBROADCAST || pCwp->message == WM_ENDSESSION || pCwp->message == WM_QUERYENDSESSION) {
            LogMessage(L"Shutdown-related message detected");
        }

    ShowPopup:
        if (pCwp->message == WM_COMMAND || pCwp->message == WM_SYSCOMMAND) {
            int result = MessageBox(NULL, L"Do you really want to shut down?", L"SWiT Shutdown",
                                   MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST | MB_SYSTEMMODAL | MB_SETFOREGROUND);
            wchar_t resultMsg[256];
            swprintf_s(resultMsg, 256, L"MessageBox result: %d", result);
            LogMessage(resultMsg);
            if (result == IDCANCEL) {
                LogMessage(L"User canceled shutdown");
                return -1; // Block
            }
            LogMessage(L"User confirmed shutdown");
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

LRESULT CALLBACK ShellProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HSHELL_WINDOWCREATED || nCode == HSHELL_WINDOWDESTROYED) {
        wchar_t shellLog[256];
        swprintf_s(shellLog, 256, L"Shell event: %d, wParam: 0x%X", nCode, wParam);
        LogMessage(shellLog);
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInstance;
        LogMessage(L"DLL attached to process");
    } else if (dwReason == DLL_PROCESS_DETACH) {
        LogMessage(L"DLL detached from process");
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void InstallHook() {
    // Try Start menu window (Windows 10/11 uses StartMenuExperienceHost or Explorer)
    HWND hStart = FindWindow(L"Windows.UI.Core.CoreWindow", NULL); // Start menu in Win 11
    if (!hStart) hStart = FindWindow(L"Start", NULL); // Fallback for Win 10
    DWORD threadId = hStart ? GetWindowThreadProcessId(hStart, NULL) : 0;
    if (threadId) {
        g_hHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hInst, threadId);
        if (g_hHook) {
            wchar_t msg[256];
            swprintf_s(msg, 256, L"WndProc hook installed on thread %lu", threadId);
            LogMessage(msg);
        } else {
            wchar_t errMsg[256];
            swprintf_s(errMsg, 256, L"WndProc hook failed, error: %lu", GetLastError());
            LogMessage(errMsg);
            // Fallback to shell hook
            g_hHook = SetWindowsHookEx(WH_SHELL, ShellProc, g_hInst, 0);
            LogMessage(g_hHook ? L"Shell hook installed as fallback" : L"Shell hook failed too");
        }
    } else {
        LogMessage(L"No Start menu window found, using system-wide hook");
        g_hHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hInst, 0);
        LogMessage(g_hHook ? L"System-wide WndProc hook installed" : L"Hook failed");
    }
}

extern "C" __declspec(dllexport) void UninstallHook() {
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
        LogMessage(L"Hook uninstalled");
    }
}