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
if not exist asan mkdir asan
rc /nologo /d WIN32 /fo asan\WinVi.res WinVi.rc
cl /nologo /MP4 /O1 /Zi /fsanitize=address /MT /DWIN32 /D_WIN32_WINNT=0x0A00 /D_WIN32_IE=0x0A00 /D_CRT_SECURE_NO_WARNINGS /Foasan\ /Fdasan\compiler.pdb /Feasan\WinVi64.exe *.c /link asan\WinVi.res /MACHINE:X64 /SUBSYSTEM:WINDOWS,6.02 /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib > asan\build.log 2>&1
if errorlevel 1 (type asan\build.log & exit /b 1)
copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" asan\ >nul
copy /y WinVi-*.htm asan\ >nul
echo Built ASan executable

