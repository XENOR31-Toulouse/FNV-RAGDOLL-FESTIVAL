@echo off
rem Compile et lance le programme de test : les vrais calculs du plugin sur des corps simules, hors du jeu
rem (src\tests.inl). Renvoie le nombre d'echecs.
setlocal
set ROOT=%~dp0
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo Visual Studio C++ introuvable & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist "%ROOT%build\test" mkdir "%ROOT%build\test"
cl /nologo /DRR_TEST /O1 /MT /EHa /std:c++17 /W4 /wd4100 /arch:SSE2 /utf-8 ^
   /Fo"%ROOT%build\test\\" /Fe"%ROOT%build\test\rr_test.exe" "%ROOT%src\main.cpp" user32.lib || exit /b 1
"%ROOT%build\test\rr_test.exe"
