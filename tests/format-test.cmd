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
cl /nologo /O2 /Gy /MT /DWIN32 /D_CRT_SECURE_NO_WARNINGS /Feformat-test.exe format-test.c /link bcrypt.lib /OPT:REF user32.lib shell32.lib advapi32.lib > format-build.log 2>&1
if errorlevel 1 (type format-build.log & exit /b 1)
format-test.exe

