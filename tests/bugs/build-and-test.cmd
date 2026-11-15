@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio C++ build tools are required. & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "WINVI_VS=%%i"
if not defined WINVI_VS (echo Visual Studio C++ x64 tools not found. & exit /b 1)
call "%WINVI_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
:ready
cd /d "%~dp0..\.."
if not exist tests\bugs\obj mkdir tests\bugs\obj
set "REVIEW_SOURCES="
for %%f in (*.c) do if /i not "%%~nxf"=="Tags.c" if /i not "%%~nxf"=="PathExp.c" if /i not "%%~nxf"=="Page.c" if /i not "%%~nxf"=="InterCom.c" if /i not "%%~nxf"=="Bmp.c" if /i not "%%~nxf"=="Search.c" set REVIEW_SOURCES=!REVIEW_SOURCES! %%f
cl /nologo /GS /guard:cf /MP4 /O1 /Zi /FS /fsanitize=address /MT /DWIN32 /D_WIN32_WINNT=0x0A00 /D_WIN32_IE=0x0A00 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fotests\bugs\obj\ /Fdtests\bugs\obj\compiler.pdb /Fetests\bugs\review-probes.exe !REVIEW_SOURCES! tests\bugs\review-probes.c tests\bugs\review-path.c tests\bugs\review-page.c tests\bugs\review-peer.c tests\bugs\review-bitmap.c tests\bugs\review-search.c /link /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /GUARD:CF /SUBSYSTEM:CONSOLE /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > tests\bugs\build.log 2>&1
if errorlevel 1 (type tests\bugs\build.log & exit /b 1)
copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" tests\bugs\ >nul
echo Built isolated security probes




cd /d "%~dp0"
pwsh -NoProfile -File "%~dp0run-bugs.ps1"
exit /b %errorlevel%

