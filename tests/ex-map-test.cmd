@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
if /i "%~1"=="asan" (call build-env.cmd modern) else (call build-env.cmd legacy)
if errorlevel 1 exit /b 1
set "MAP_FLAGS=/O2 /MT"
if /i "%~1"=="asan" set "MAP_FLAGS=/O1 /Zi /fsanitize=address /MT"
set "MAP_SOURCES="
for %%f in (*.c) do if /i not "%%~nxf"=="Map.c" if /i not "%%~nxf"=="ExMap.c" set MAP_SOURCES=!MAP_SOURCES! %%f
if not exist tests\ex-map-obj mkdir tests\ex-map-obj
cl /nologo /MP4 %MAP_FLAGS% /DWIN32 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /DWinMain=WvOriginalWinMain /Fotests\ex-map-obj\ /Fdtests\ex-map-obj\compiler.pdb /Fetests\ex-map-test.exe !MAP_SOURCES! tests\ex-map-test.c /link /STACK:4194304 user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib mpr.lib ole32.lib uuid.lib shlwapi.lib bcrypt.lib > tests\ex-map-build.log 2>&1
if errorlevel 1 (type tests\ex-map-build.log & exit /b 1)
if /i "%~1"=="asan" copy /y "%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll" tests\ >nul
tests\ex-map-test.exe
