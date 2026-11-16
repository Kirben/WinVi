@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
if /i "%~1"=="asan" (call build-env.cmd modern) else (call build-env.cmd legacy)
if errorlevel 1 exit /b 1
set "SCRIPT_FLAGS=/O2 /MT"
if /i "%~1"=="asan" set "SCRIPT_FLAGS=/O1 /Zi /fsanitize=address /MT"
set "SCRIPT_SOURCES="
for %%f in (*.c) do if /i not "%%~nxf"=="ExScript.c" set SCRIPT_SOURCES=!SCRIPT_SOURCES! %%f
if not exist tests\ex-script-obj mkdir tests\ex-script-obj
cl /nologo /MP4 %SCRIPT_FLAGS% /DWIN32 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fotests\ex-script-obj\ /Fdtests\ex-script-obj\compiler.pdb /Fetests\ex-script-test.exe !SCRIPT_SOURCES! tests\ex-script-test.c /link /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > tests\ex-script-build.log 2>&1
if errorlevel 1 (type tests\ex-script-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" tests\ >nul
tests\ex-script-test.exe
