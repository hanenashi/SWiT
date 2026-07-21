#ifndef AppVersion
  #define AppVersion "0.1.0-alpha.1"
#endif
#ifndef AppNumericVersion
  #define AppNumericVersion "0.1.0.1"
#endif

#define AppName "SWiT"
#define AppExeName "swit-agent.exe"
#define AppIdValue "{E491A3F7-4579-4659-A8D8-B714B44E6049}"

[Setup]
AppId={{E491A3F7-4579-4659-A8D8-B714B44E6049}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=SWiT project
AppPublisherURL=https://github.com/hanenashi/SWiT
AppSupportURL=https://github.com/hanenashi/SWiT/issues
AppUpdatesURL=https://github.com/hanenashi/SWiT/releases
AppCopyright=Copyright (C) 2026 SWiT contributors
DefaultDirName={localappdata}\Programs\SWiT
DefaultGroupName=SWiT
DisableProgramGroupPage=auto
PrivilegesRequired=lowest
SetupArchitecture=x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir=..\dist
OutputBaseFilename=SWiT-Setup-{#AppVersion}-x64
SetupIconFile=..\assets\swit-icon.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes
UsePreviousTasks=yes
VersionInfoVersion={#AppNumericVersion}
VersionInfoCompany=SWiT project
VersionInfoDescription=SWiT Setup
VersionInfoProductName=SWiT
VersionInfoProductVersion={#AppNumericVersion}

[Tasks]
Name: autostart; Description: "Start SWiT when I sign in"; GroupDescription: "Startup:"; Check: IsFreshInstall

[Files]
Source: "..\build\swit-agent.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\swit-send.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SWiT"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall SWiT"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "SWiT"; ValueData: """{app}\{#AppExeName}"""; Tasks: autostart; Check: IsFreshInstall

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Start SWiT"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\SWiT"

[Code]
const
  AgentMutex = 'Local\SWiT.Agent.SingleInstance';
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';
  RunValueName = 'SWiT';
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppIdValue}_is1';

var
  UpgradeInstall: Boolean;

function IsFreshInstall: Boolean;
begin
  Result := not UpgradeInstall;
end;

procedure StopInstalledAgent;
var
  ResultCode: Integer;
  SenderPath: String;
begin
  SenderPath := ExpandConstant('{app}\swit-send.exe');
  if FileExists(SenderPath) then
  begin
    if not Exec(SenderPath, 'exit', ExpandConstant('{app}'), SW_HIDE,
      ewWaitUntilTerminated, ResultCode) then
      Log('Could not launch swit-send.exe to stop SWiT.');
  end;
end;

function InitializeSetup: Boolean;
begin
  UpgradeInstall := RegKeyExists(HKCU64, UninstallKey);
  Result := True;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Attempts: Integer;
begin
  StopInstalledAgent;
  for Attempts := 1 to 20 do
  begin
    if not CheckForMutexes(AgentMutex) then
      Break;
    Sleep(100);
  end;

  if CheckForMutexes(AgentMutex) then
    Result := 'SWiT is still running. Exit it from the notification-area menu and retry.'
  else
    Result := '';
end;

function InitializeUninstall: Boolean;
begin
  StopInstalledAgent;
  Sleep(500);
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ExpectedCommand: String;
  RunCommand: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    ExpectedCommand := '"' + ExpandConstant('{app}\{#AppExeName}') + '"';
    if RegQueryStringValue(HKCU64, RunKey, RunValueName, RunCommand) and
      (CompareText(RunCommand, ExpectedCommand) = 0) then
      RegDeleteValue(HKCU64, RunKey, RunValueName);
  end;
end;
