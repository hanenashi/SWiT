@echo off
setlocal

set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1

if not defined VSCMD_ARG_TGT_ARCH (
    if not exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        echo vcvars64.bat not found at "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        exit /b 1
    )
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)

if not exist build mkdir build

cl.exe /nologo /DUNICODE /D_UNICODE /EHsc /W4 /permissive- ^
    /Fobuild\swit_agent.obj /Febuild\swit-agent.exe ^
    src\swit_agent.cpp user32.lib shell32.lib || exit /b 1

cl.exe /nologo /DUNICODE /D_UNICODE /EHsc /W4 /permissive- ^
    /Fobuild\swit_send.obj /Febuild\swit-send.exe ^
    src\swit_send.cpp user32.lib || exit /b 1

echo Built build\swit-agent.exe and build\swit-send.exe
popd
