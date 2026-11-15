@echo off
setlocal
cd /d "%~dp0"
call ..\build-env.cmd legacy
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /DWIN32 /D_WIN32_WINNT=0x0601 /D_WIN32_IE=0x0800 /D_CRT_SECURE_NO_WARNINGS /Fecompatibility-job-test.exe compatibility-job-test.c /link user32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib > compatibility-job-build.log 2>&1
if errorlevel 1 (type compatibility-job-build.log & exit /b 1)
compatibility-job-test.exe
