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
if /i "%~1"=="asan" (set "WV_TEST_SAN=/O1 /Zi /fsanitize=address") else (set "WV_TEST_SAN=/O2")
cl /nologo /GS /guard:cf %WV_TEST_SAN% /Gy /MT /DWIN32 /D_WIN32_WINNT=0x0A00 /D_CRT_SECURE_NO_WARNINGS /Fesafe-save-test.exe safe-save-test.c ..\Platform.c /link /OPT:REF /STACK:4194304 user32.lib shell32.lib advapi32.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > safe-save-build.log 2>&1
if errorlevel 1 (type safe-save-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" . >nul
safe-save-test.exe
exit /b %errorlevel%
