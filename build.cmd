@echo off
rem Compile NVReactiveRagdoll.dll (32 bits, comme FalloutNV.exe) avec Visual Studio 2022.
rem Resultat : build\NVReactiveRagdoll.dll, et une copie prete a installer dans dist\Data\NVSE\Plugins\.
setlocal
set ROOT=%~dp0
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo Visual Studio C++ introuvable & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1

if not exist "%ROOT%build" mkdir "%ROOT%build"
cl /nologo /LD /O2 /MT /EHa /std:c++17 /W4 /wd4100 /arch:SSE2 /utf-8 ^
   /Fo"%ROOT%build\\" /Fe"%ROOT%build\NVReactiveRagdoll.dll" "%ROOT%src\main.cpp" user32.lib || exit /b 1

set OUT=%ROOT%dist\Data\NVSE\Plugins
if not exist "%OUT%" mkdir "%OUT%"
copy /y "%ROOT%build\NVReactiveRagdoll.dll" "%OUT%\" >nul
copy /y "%ROOT%data\NVReactiveRagdoll.ini" "%OUT%\" >nul
echo OK : %OUT%\NVReactiveRagdoll.dll

rem Copie dans le mod Mod Organizer de developpement s'il existe (MO2_MOD peut pointer ailleurs)
if not defined MO2_MOD set MO2_MOD=%LOCALAPPDATA%\ModOrganizer\New Vegas\mods\NV Reactive Ragdoll (dev)
if exist "%MO2_MOD%" (
  if not exist "%MO2_MOD%\NVSE\Plugins" mkdir "%MO2_MOD%\NVSE\Plugins"
  copy /y "%OUT%\NVReactiveRagdoll.dll" "%MO2_MOD%\NVSE\Plugins\" >nul
  if not exist "%MO2_MOD%\NVSE\Plugins\NVReactiveRagdoll.ini" copy /y "%OUT%\NVReactiveRagdoll.ini" "%MO2_MOD%\NVSE\Plugins\" >nul
  echo Installe dans : "%MO2_MOD%"
)
