#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    HWND hwnd = GetForegroundWindow(); // Try to grab focus
    if (hwnd) {
        SetForegroundWindow(hwnd);
    }
    
    int result = MessageBox(NULL,
                            L"Do you really want to shut down?",
                            L"Shutdown Confirmation",
                            MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL);

    return (result == IDOK) ? 1 : 0; // 1 for OK, 0 for Cancel
}