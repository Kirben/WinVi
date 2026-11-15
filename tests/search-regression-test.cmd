@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
set "SEARCH_FLAGS=/O2 /MT"
if /i "%~1"=="asan" (call build-env.cmd modern) else (call build-env.cmd legacy)
if errorlevel 1 exit /b 1
if /i "%~1"=="asan" set "SEARCH_FLAGS=/O1 /Zi /fsanitize=address /MT"
set "SEARCH_SOURCES="
for %%f in (*.c) do if /i not "%%~nxf"=="Search.c" set SEARCH_SOURCES=!SEARCH_SOURCES! %%f
if not exist tests\search-regression-obj mkdir tests\search-regression-obj
cl /nologo /MP4 %SEARCH_FLAGS% /DWIN32 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fotests\search-regression-obj\ /Fdtests\search-regression-obj\compiler.pdb /Fetests\search-regression-test.exe !SEARCH_SOURCES! tests\search-regression-test.c /link /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > tests\search-regression-build.log 2>&1
if errorlevel 1 (type tests\search-regression-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" tests\ >nul
tests\search-regression-test.exe %~2
