@echo off
setlocal
cd /d "%~dp0"
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio C++ build tools are required. & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "WINVI_VS=%%i"
if not defined WINVI_VS (echo Visual Studio C++ x64 tools not found. & exit /b 1)
call "%WINVI_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
:ready
if not exist hex-layout-obj mkdir hex-layout-obj
cl /nologo /MP4 /O2 /MT /DWIN32 /D_WIN32_WINNT=0x0A00 /D_WIN32_IE=0x0A00 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fohex-layout-obj\ /Fehex-layout-test.exe ..\*.c hex-layout-test.c /link bcrypt.lib /SUBSYSTEM:CONSOLE /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib > hex-layout-build.log 2>&1
if errorlevel 1 (type hex-layout-build.log & exit /b 1)
hex-layout-test.exe
