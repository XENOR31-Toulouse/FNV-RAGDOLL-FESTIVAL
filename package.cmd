@echo off
rem Compile puis construit le paquet a publier : dist\NVReactiveRagdoll-<version>.zip (voir package.ps1).
call "%~dp0build.cmd" || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" || exit /b 1
