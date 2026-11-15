@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "WINVI_VS=%%i"
if not defined WINVI_VS (echo Visual Studio C++ x64 tools not found. & exit /b 1)
call "%WINVI_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
:ready
if not exist hex-print-obj mkdir hex-print-obj
set "PRINT_SOURCES="
for %%f in (..\*.c) do if /i not "%%~nxf"=="Print.c" set PRINT_SOURCES=!PRINT_SOURCES! %%f
set "PRINT_SAN="
if /i "%1"=="asan" set "PRINT_SAN=/fsanitize=address /Zi"
cl /nologo /GS /guard:cf /O1 /MT %PRINT_SAN% /DWIN32 /D_WIN32_WINNT=0x0A00 /D_WIN32_IE=0x0A00 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fohex-print-obj\ /Fdhex-print-obj\compiler.pdb /Fehex-print-test.exe !PRINT_SOURCES! hex-print-test.c /link /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > hex-print-build.log 2>&1
if errorlevel 1 (type hex-print-build.log & exit /b 1)
if /i "%1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" . >nul
hex-print-test.exe
exit /b %errorlevel%
