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
if not defined WINVI_OUTPUT_NAME set "WINVI_OUTPUT_NAME=WinVi64.exe"
if not exist x64 mkdir x64
rc /nologo /d WIN32 /fo x64\WinVi.res WinVi.rc > x64\resources.log 2>&1
if errorlevel 1 (type x64\resources.log & exit /b 1)
cl /nologo /MP4 /O2 /MT /W3 /DWIN32 /DWINVI_TRACE /D_WIN32_WINNT=0x0A00 /D_WIN32_IE=0x0A00 /D_CRT_SECURE_NO_WARNINGS /Fox64\ /Fex64\%WINVI_OUTPUT_NAME% *.c /link x64\WinVi.res /MACHINE:X64 /SUBSYSTEM:WINDOWS,6.02 /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib > x64\build.log 2>&1
if errorlevel 1 (type x64\build.log & exit /b 1)
echo Built x64\WinVi64.exe




