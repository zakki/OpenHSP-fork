@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0switch_chsp.ps1" -Mode Disable
set "chsp_result=%errorlevel%"
if /I not "%~1"=="/quiet" pause
exit /b %chsp_result%
