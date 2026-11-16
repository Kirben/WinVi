@echo off
setlocal
cd /d "%~dp0.."
call build-env.cmd legacy
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /Fetests\shell-fixture.exe /Fotests\shell-fixture.obj tests\shell-fixture.c /link /SUBSYSTEM:CONSOLE,6.01 > tests\shell-fixture-build.log 2>&1
if errorlevel 1 (type tests\shell-fixture-build.log & exit /b 1)
