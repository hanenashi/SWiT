#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Force this to be topmost and in foreground
    AllowSetForegroundWindow(ASFW_ANY);
    SetProcessDPIAware();
    
    // Create and show a topmost dialog
    int result = MessageBoxW(NULL,
                          L"System Shutdown Requested\n\nDo you want to proceed with shutdown?",
                          L"Shutdown Confirmation",
                          MB_YESNO | MB_ICONWARNING | MB_TOPMOST | 
                          MB_SETFOREGROUND | MB_SYSTEMMODAL);
    
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
    }
    
    return (result == IDYES) ? 1 : 0;
}