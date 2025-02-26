#include <windows.h>
#include <stdio.h>

SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

void WINAPI ServiceCtrlHandler(DWORD controlCode);
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
BOOL RunGuiPrompt();
void LogMessage(const wchar_t* message);

void ReportStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
    g_ServiceStatus.dwCurrentState = currentState;
    g_ServiceStatus.dwWin32ExitCode = win32ExitCode;
    g_ServiceStatus.dwWaitHint = waitHint;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

void LogMessage(const wchar_t* message) {
    FILE* file;
    _wfopen_s(&file, L"C:\\GIT\\SWiT\\swit_log.txt", L"a");
    if (file) {
        fwprintf(file, L"%s\n", message);
        fclose(file);
    }
}

void WINAPI ServiceCtrlHandler(DWORD controlCode) {
    switch (controlCode) {
    case SERVICE_CONTROL_SHUTDOWN:
        {
            BOOL userConfirmed = FALSE;
            LogMessage(L"Shutdown detected");
            ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000); // 30s wait hint
            
            while (!userConfirmed && !RunGuiPrompt()) {
                LogMessage(L"Retrying GUI prompt...");
                ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000);
                Sleep(5000); // Wait 5s, loop until success
            }
            userConfirmed = RunGuiPrompt();

            if (userConfirmed) {
                LogMessage(L"User confirmed shutdown");
                ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
                ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
            } else {
                LogMessage(L"User canceled or GUI failed, attempting to abort");
                ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
            }
        }
        break;

    case SERVICE_CONTROL_STOP:
        LogMessage(L"Service stop requested");
        ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
        SetEvent(g_ServiceStopEvent);
        break;

    default:
        LogMessage(L"Unknown control code");
        break;
    }
}

BOOL RunGuiPrompt() {
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    wchar_t cmdLine[] = L"C:\\GIT\\SWiT\\swit_gui.exe";

    LogMessage(L"Attempting to launch swit_gui.exe directly");
    BOOL success = CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 
                                 CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS, 
                                 NULL, NULL, &si, &pi);

    if (success) {
        LogMessage(L"swit_gui.exe launched, waiting...");
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitCode == 1); // TRUE if OK, FALSE if Cancel
    } else {
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, 256, L"Failed to launch swit_gui.exe, error: %lu", GetLastError());
        LogMessage(errorMsg);
        return FALSE;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    LogMessage(L"Service starting");
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    g_StatusHandle = RegisterServiceCtrlHandler(L"SWiTService", ServiceCtrlHandler);
    if (!g_StatusHandle) {
        LogMessage(L"Failed to register handler");
        return;
    }

    ReportStatus(SERVICE_START_PENDING, NO_ERROR, 1000);
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        LogMessage(L"Failed to create stop event");
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
    LogMessage(L"Connecting to SCM");
    StartServiceCtrlDispatcher(serviceTable);
    return 0;
}