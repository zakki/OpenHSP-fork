@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "HSPCMP=%~1"
if "%HSPCMP%"=="" set "HSPCMP=%SCRIPT_DIR%..\..\src\hspcmp\Release64\hspcmp.exe"

python "%SCRIPT_DIR%run_tests.py" --hspcmp "%HSPCMP%" --common "%SCRIPT_DIR%..\..\common"
exit /b %ERRORLEVEL%
