@echo off
setlocal
cd /d "%~dp0.."
set "SPLIT_FLAGS=/O2 /MT"
set "SPLIT_OBJ=tests\auto-split-obj"
if /i "%~1"=="asan" (call build-env.cmd modern) else (call build-env.cmd legacy)
if errorlevel 1 exit /b 1
if /i "%~1"=="asan" set "SPLIT_FLAGS=/O1 /Zi /fsanitize=address /MT"
if not exist %SPLIT_OBJ% mkdir %SPLIT_OBJ%
cl /nologo /MP4 %SPLIT_FLAGS% /DWIN32 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fo%SPLIT_OBJ%\ /Fd%SPLIT_OBJ%\compiler.pdb /Fetests\auto-split-test.exe *.c tests\auto-split-test.c /link /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > tests\auto-split-build.log 2>&1
if errorlevel 1 (type tests\auto-split-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" tests\ >nul
tests\auto-split-test.exe
