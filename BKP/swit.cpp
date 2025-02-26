#include <windows.h>
#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")

#define TIMER_ID 1
bool g_ShutdownInterceptionDisabled = false;

void BlockShutdown(HWND hWnd) {
    ShutdownBlockReasonCreate(hWnd, L"Waiting for user confirmation...");
}

void UnblockShutdown(HWND hWnd) {
    ShutdownBlockReasonDestroy(hWnd);
}

void CheckShutdown(HWND hWnd) {
    if (g_ShutdownInterceptionDisabled) return;

    int result = MessageBox(NULL,
                            L"Do you really want to shut down?",
                            L"Shutdown Confirmation",
                            MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);

    if (result == IDOK) {
        g_ShutdownInterceptionDisabled = true;
        UnblockShutdown(hWnd);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == TIMER_ID) {
            // Check if shutdown is pending (crude approximation)
            if (GetSystemMetrics(SM_SHUTTINGDOWN)) {
                KillTimer(hWnd, TIMER_ID); // Stop polling
                CheckShutdown(hWnd);
            }
        }
        break;

    case WM_WTSSESSION_CHANGE:
        if (lParam == WTS_SESSION_LOGOFF) {
            SendMessage(hWnd, WM_QUERYENDSESSION, 0, ENDSESSION_LOGOFF);
        }
        break;

    case WM_QUERYENDSESSION:
        if (g_ShutdownInterceptionDisabled) {
            return TRUE;
        }
        KillTimer(hWnd, TIMER_ID); // Stop timer during shutdown handling
        CheckShutdown(hWnd);
        return g_ShutdownInterceptionDisabled ? TRUE : FALSE;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        UnblockShutdown(hWnd);
        WTSUnRegisterSessionNotification(hWnd);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    const wchar_t CLASS_NAME[] = L"SWiTDaemonClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc))
        return 0;

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"SWiT Daemon", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!hWnd)
        return 0;

    BlockShutdown(hWnd);
    WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION);
    SetTimer(hWnd, TIMER_ID, 500, NULL); // Poll every 500ms

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}