@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
  if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\18\Community"
)

if not defined VSINSTALL (
  echo Visual Studio C++ Build Tools nicht gefunden.
  exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
set "OUT=%~1"
if not defined OUT set "OUT=%~dp0Weiks CS2 Autoaccept.exe"
cl /nologo /std:c++17 /O2 /EHsc /DUNICODE /D_UNICODE "%~dp0CS2AcceptWatcher.cpp" /Fe:"%OUT%" /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
echo Fertig: %OUT%
