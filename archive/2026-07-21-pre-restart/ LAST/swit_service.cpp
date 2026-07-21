#include <windows.h>
#include <stdio.h>
#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")

SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
HANDLE g_ShutdownBlockReasonHandle = NULL;

void WINAPI ServiceCtrlHandler(DWORD);
void WINAPI ServiceMain(DWORD, LPTSTR*);
BOOL RunGuiPrompt();
void LogMessage(const wchar_t*);

void ReportStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
    g_ServiceStatus.dwCurrentState = currentState;
    g_ServiceStatus.dwWin32ExitCode = win32ExitCode;
    g_ServiceStatus.dwWaitHint = waitHint;
    
    if (currentState == SERVICE_RUNNING)
        g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PRESHUTDOWN;
    
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

void LogMessage(const wchar_t* message) {
    FILE* file;
    _wfopen_s(&file, L"C:\\GIT\\SWiT\\swit_log.txt", L"a");
    if (file) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(file, L"[%02d:%02d:%02d.%03d] %s\n", 
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
        fclose(file);
    }
}

BOOL InitiateSystemShutdownBlocker() {
    HANDLE token;
    TOKEN_PRIVILEGES privileges;
    
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return FALSE;
    
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid);
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    AdjustTokenPrivileges(token, FALSE, &privileges, 0, NULL, NULL);
    CloseHandle(token);

    return ShutdownBlockReasonCreate(NULL, L"Waiting for user confirmation");
}

void RemoveSystemShutdownBlocker() {
    ShutdownBlockReasonDestroy(NULL);
}

void WINAPI ServiceCtrlHandler(DWORD controlCode) {
    switch (controlCode) {
    case SERVICE_CONTROL_PRESHUTDOWN:
        LogMessage(L"PRE-SHUTDOWN received!");
        ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 180000);
        
        if (InitiateSystemShutdownBlocker()) {
            LogMessage(L"Shutdown blocked, showing prompt");
            BOOL userConfirmed = RunGuiPrompt();
            RemoveSystemShutdownBlocker();
            
            if (!userConfirmed) {
                LogMessage(L"User canceled shutdown, aborting");
                AbortSystemShutdown(NULL);
                ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
            } else {
                LogMessage(L"User confirmed shutdown, proceeding");
                ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
            }
        }
        break;
        
    case SERVICE_CONTROL_SHUTDOWN:
        LogMessage(L"SHUTDOWN received!");
        break;

    case SERVICE_CONTROL_STOP:
        LogMessage(L"STOP received");
        ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
        SetEvent(g_ServiceStopEvent);
        break;
    }
}

BOOL RunGuiPrompt() {
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == 0xFFFFFFFF) return TRUE;
    
    HANDLE userToken = NULL;
    if (!WTSQueryUserToken(sessionId, &userToken)) return TRUE;
    
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.lpDesktop = L"Winsta0\\Default";
    si.wShowWindow = SW_SHOWNORMAL;
    si.dwFlags = STARTF_USESHOWWINDOW;
    
    BOOL success = CreateProcessAsUser(
        userToken,
        L"C:\\GIT\\SWiT\\swit_gui.exe",
        NULL, NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE | HIGH_PRIORITY_CLASS,
        NULL, NULL, &si, &pi);
        
    CloseHandle(userToken);
    
    if (!success) return TRUE;
    
    DWORD result = WaitForSingleObject(pi.hProcess, 30000);
    DWORD exitCode = 1;
    
    if (result != WAIT_TIMEOUT)
        GetExitCodeProcess(pi.hProcess, &exitCode);
    else
        TerminateProcess(pi.hProcess, 0);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return (exitCode == 1);
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PRESHUTDOWN;

    g_StatusHandle = RegisterServiceCtrlHandler(L"SWiTService", ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    ReportStatus(SERVICE_START_PENDING, NO_ERROR, 1000);
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_ServiceStopEvent) {
        ReportStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
    LogMessage(L"Service running");

    WaitForSingleObject(g_ServiceStopEvent, INFINITE);
}

int main() {
    SERVICE_TABLE_ENTRY serviceTable[] = {
        {L"SWiTService", (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    
    StartServiceCtrlDispatcher(serviceTable);
    return 0;
}