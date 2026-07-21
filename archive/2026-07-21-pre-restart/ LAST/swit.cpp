#include <windows.h>
#include <wtsapi32.h>  // Required for WTSRegisterSessionNotification
#pragma comment(lib, "wtsapi32.lib")

bool g_ShutdownInterceptionDisabled = false;

// Function to block shutdown and show a reason
void BlockShutdown(HWND hWnd) {
    ShutdownBlockReasonCreate(hWnd, L"Waiting for user confirmation...");
}

// Function to remove the shutdown block
void UnblockShutdown(HWND hWnd) {
    ShutdownBlockReasonDestroy(hWnd);
}

// Window procedure for handling shutdown interception
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    int result = 0;  // Declare 'result' outside the switch block to avoid errors

    switch (message) {
    case WM_WTSSESSION_CHANGE:
        if (lParam == WTS_SESSION_LOCK) {
            // User locked the PC; we could log this if needed
        } else if (lParam == WTS_SESSION_LOGOFF) {
            // A logoff/shutdown is happening
            SendMessage(hWnd, WM_QUERYENDSESSION, 0, ENDSESSION_LOGOFF);
        }
        break;

    case WM_QUERYENDSESSION:
        if (g_ShutdownInterceptionDisabled)
            return TRUE;

        BlockShutdown(hWnd);  // Tell Windows we are blocking shutdown

        result = MessageBox(hWnd,
                            L"Do you really want to shut down?",
                            L"Shutdown Confirmation",
                            MB_OKCANCEL | MB_ICONQUESTION);

        if (result == IDOK) {
            g_ShutdownInterceptionDisabled = true;
            UnblockShutdown(hWnd);
            ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
        }
        return FALSE;

    case WM_DESTROY:
        PostQuitMessage(0);
        WTSUnRegisterSessionNotification(hWnd); // Unregister when closing
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

    WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION);  // Register for early shutdown/logoff notifications

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
