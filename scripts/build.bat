@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1

if not defined VSCMD_ARG_TGT_ARCH (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
    )
    if not defined VSINSTALL if exist "C:\Program Files\Microsoft Visual Studio\2022\Community" set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Community"
    if not defined VSINSTALL (
        echo Visual Studio 2022 C++ tools were not found.
        exit /b 1
    )
    call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)

if not exist build mkdir build

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
if /i not "%CONFIG%"=="debug" if /i not "%CONFIG%"=="release" (
    echo Usage: scripts\build.bat [debug^|release]
    exit /b 2
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\generate-version.ps1 || exit /b 1

set "COMMON_CFLAGS=/nologo /DUNICODE /D_UNICODE /EHsc /W4 /permissive- /GS /guard:cf"
set "COMMON_LDFLAGS=/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /guard:cf /MANIFESTUAC"
if /i "%CONFIG%"=="release" (
    set "CONFIG_CFLAGS=/O2 /GL /Gy /DNDEBUG /MT"
    set "CONFIG_LDFLAGS=/LTCG /OPT:REF /OPT:ICF /Brepro"
) else (
    set "CONFIG_CFLAGS=/Od /D_DEBUG /MTd"
    set "CONFIG_LDFLAGS="
)

rc.exe /nologo /ibuild /fobuild\swit_agent.res src\swit_agent.rc || exit /b 1
rc.exe /nologo /ibuild /fobuild\swit_send.res src\swit_send.rc || exit /b 1

cl.exe %COMMON_CFLAGS% %CONFIG_CFLAGS% ^
    /Fobuild\swit_agent.obj /Febuild\swit-agent.exe ^
    src\swit_agent.cpp build\swit_agent.res user32.lib shell32.lib advapi32.lib ole32.lib ^
    /link %COMMON_LDFLAGS% %CONFIG_LDFLAGS% || exit /b 1

cl.exe %COMMON_CFLAGS% %CONFIG_CFLAGS% ^
    /Fobuild\swit_send.obj /Febuild\swit-send.exe ^
    src\swit_send.cpp build\swit_send.res user32.lib ^
    /link %COMMON_LDFLAGS% %CONFIG_LDFLAGS% || exit /b 1

cl.exe %COMMON_CFLAGS% %CONFIG_CFLAGS% ^
    /Fobuild\swit_helper.obj /Febuild\swit-helper.exe ^
    src\swit_helper.cpp user32.lib shell32.lib ^
    /link %COMMON_LDFLAGS% %CONFIG_LDFLAGS% || exit /b 1

echo Built %CONFIG% build\swit-agent.exe, build\swit-send.exe, and build\swit-helper.exe
popd
