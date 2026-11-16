@echo off
setlocal
cd /d "%~dp0.."
if /i "%~1"=="asan" (call build-env.cmd modern) else (call build-env.cmd legacy)
if errorlevel 1 exit /b 1
set "RECOVERY_FLAGS=/O2 /MT"
if /i "%~1"=="asan" set "RECOVERY_FLAGS=/O1 /Zi /fsanitize=address /MT"
if not exist tests\recovery-obj mkdir tests\recovery-obj
cl /nologo %RECOVERY_FLAGS% /DWIN32 /D_WIN32_WINNT=0x0601 /D_CRT_SECURE_NO_WARNINGS /Fotests\recovery-obj\ /Fdtests\recovery-obj\compiler.pdb /Fetests\recovery-test.exe tests\recovery-test.c /link /STACK:4194304 advapi32.lib bcrypt.lib > tests\recovery-build.log 2>&1
if errorlevel 1 (type tests\recovery-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" tests\ >nul
tests\recovery-test.exe
