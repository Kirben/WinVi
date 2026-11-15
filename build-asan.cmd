@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build-env.cmd" modern
if errorlevel 1 exit /b 1
if not exist asan mkdir asan
rc /nologo /d WIN32 /fo asan\WinVi.res WinVi.rc
cl /nologo /GS /guard:cf /MP4 /O1 /Zi /fsanitize=address /MT /DWIN32 /D_WIN32_WINNT=0x0A00 /D_WIN32_IE=0x0A00 /D_CRT_SECURE_NO_WARNINGS /Foasan\ /Fdasan\compiler.pdb /Feasan\WinVi64.exe *.c /link asan\WinVi.res /MACHINE:X64 /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /GUARD:CF /SUBSYSTEM:WINDOWS,6.02 /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > asan\build.log 2>&1
if errorlevel 1 (type asan\build.log & exit /b 1)
copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" asan\ >nul
copy /y WinVi-*.htm asan\ >nul
echo Built ASan executable

