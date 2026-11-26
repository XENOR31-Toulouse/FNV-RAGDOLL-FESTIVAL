@echo off
rem Compile NVReactiveRagdoll.dll (32 bits, comme FalloutNV.exe) avec Visual Studio 2022.
rem Resultat : build\NVReactiveRagdoll.dll, et dist\Data\ pret a installer :
rem   NVSE\Plugins\NVReactiveRagdoll.dll, Config\NVReactiveRagdoll.ini, MCM\NVReactiveRagdoll.json (menu en jeu)
setlocal
set ROOT=%~dp0
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo Visual Studio C++ introuvable & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1

if not exist "%ROOT%build" mkdir "%ROOT%build"
cl /nologo /LD /O2 /MT /EHa /std:c++17 /W4 /wd4100 /arch:SSE2 /utf-8 ^
   /Fo"%ROOT%build\\" /Fe"%ROOT%build\NVReactiveRagdoll.dll" "%ROOT%src\main.cpp" user32.lib || exit /b 1

set OUT=%ROOT%dist\Data
for %%d in (NVSE\Plugins Config MCM) do if not exist "%OUT%\%%d" mkdir "%OUT%\%%d"
copy /y "%ROOT%build\NVReactiveRagdoll.dll" "%OUT%\NVSE\Plugins\" >nul
copy /y "%ROOT%data\NVReactiveRagdoll.ini" "%OUT%\Config\" >nul
copy /y "%ROOT%data\MCM\NVReactiveRagdoll.json" "%OUT%\MCM\" >nul
if exist "%OUT%\NVSE\Plugins\NVReactiveRagdoll.ini" del "%OUT%\NVSE\Plugins\NVReactiveRagdoll.ini"
echo OK : %OUT%

rem Copie dans le mod Mod Organizer de developpement s'il existe (MO2_MOD peut pointer ailleurs).
rem Le .ini n'est jamais ecrase (tes reglages restent) ; un .ini de l'ancienne place (NVSE\Plugins) est deplace.
if not defined MO2_MOD set MO2_MOD=%LOCALAPPDATA%\ModOrganizer\New Vegas\mods\NV Reactive Ragdoll (dev)
if exist "%MO2_MOD%" (
  for %%d in (NVSE\Plugins Config MCM) do if not exist "%MO2_MOD%\%%d" mkdir "%MO2_MOD%\%%d"
  copy /y "%OUT%\NVSE\Plugins\NVReactiveRagdoll.dll" "%MO2_MOD%\NVSE\Plugins\" >nul
  copy /y "%OUT%\MCM\NVReactiveRagdoll.json" "%MO2_MOD%\MCM\" >nul
  if exist "%MO2_MOD%\NVSE\Plugins\NVReactiveRagdoll.ini" if not exist "%MO2_MOD%\Config\NVReactiveRagdoll.ini" move "%MO2_MOD%\NVSE\Plugins\NVReactiveRagdoll.ini" "%MO2_MOD%\Config\" >nul
  if not exist "%MO2_MOD%\Config\NVReactiveRagdoll.ini" copy /y "%OUT%\Config\NVReactiveRagdoll.ini" "%MO2_MOD%\Config\" >nul
  echo Installe dans : "%MO2_MOD%"
)
