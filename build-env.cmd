@echo off
rem Called from a build script's setlocal scope; deliberately preserves its environment.
if /i "%~1"=="modern" goto modern
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" if "%VCToolsVersion:~0,5%"=="14.44" exit /b 0
set "WINVI_COMPONENT=Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64"
set "WINVI_VC_VERSION=-vcvars_ver=14.44"
goto locate
:modern
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" exit /b 0
set "WINVI_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
set "WINVI_VC_VERSION="
:locate
set "WINVI_VS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio C++ build tools are required. & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires %WINVI_COMPONENT% -property installationPath`) do set "WINVI_VS=%%i"
if not defined WINVI_VS (echo Required compiler component not found: %WINVI_COMPONENT% & exit /b 1)
call "%WINVI_VS%\VC\Auxiliary\Build\vcvars64.bat" %WINVI_VC_VERSION%
if errorlevel 1 exit /b 1
if /i "%~1"=="modern" exit /b 0
if not "%VCToolsVersion:~0,5%"=="14.44" (echo MSVC v143 14.44 is required for Windows 7 targeting. & exit /b 1)
exit /b 0
