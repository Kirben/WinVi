@echo off
setlocal
cd /d "%~dp0"
set "COMPAT_FLAGS=/O2 /MT"
if /i "%~1"=="asan" set "COMPAT_FLAGS=/Od /Zi /MT /fsanitize=address"
if /i "%~1"=="asan" (call ..\build-env.cmd modern) else (call ..\build-env.cmd legacy)
if errorlevel 1 exit /b 1
cl /nologo %COMPAT_FLAGS% /DWIN32 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /Fecompatibility-test.exe compatibility-test.c /link user32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib > compatibility-build.log 2>&1
if errorlevel 1 (type compatibility-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" . >nul
compatibility-test.exe
