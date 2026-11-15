@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build-env.cmd" legacy
if errorlevel 1 exit /b 1
if not defined WINVI_OUTPUT_NAME set "WINVI_OUTPUT_NAME=WinVi64.exe"
if not exist x64 mkdir x64
rc /nologo /d WIN32 /fo x64\WinVi.res WinVi.rc > x64\resources.log 2>&1
if errorlevel 1 (type x64\resources.log & exit /b 1)
cl /nologo /GS /guard:cf /MP4 /O2 /MT %WINVI_BUILD_FLAGS% /W3 /DWIN32 /DWINVI_LEGACY_TARGET /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /Fox64\ /Fex64\%WINVI_OUTPUT_NAME% *.c /link x64\WinVi.res /MACHINE:X64 /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /GUARD:CF /SUBSYSTEM:WINDOWS,6.01 /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > x64\build.log 2>&1
if errorlevel 1 (type x64\build.log & exit /b 1)
copy /y WinVi-*.htm x64\ >nul
echo Built x64\%WINVI_OUTPUT_NAME%




