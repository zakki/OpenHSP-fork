@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "HSPCMP=%~1"
if "%HSPCMP%"=="" set "HSPCMP=%SCRIPT_DIR%..\..\src\hspcmp\Release64\hspcmp.exe"
set "HSPCMP_DLL=%~2"
if "%HSPCMP_DLL%"=="" if exist "%SCRIPT_DIR%..\..\src\hspcmp\Release64\hspcmp_64.dll" set "HSPCMP_DLL=%SCRIPT_DIR%..\..\src\hspcmp\Release64\hspcmp_64.dll"
if "%HSPCMP_DLL%"=="" if exist "%SCRIPT_DIR%..\..\src\hspcmp\Release\hspcmp.dll" set "HSPCMP_DLL=%SCRIPT_DIR%..\..\src\hspcmp\Release\hspcmp.dll"

set "DLL_ARGS="
if not "%HSPCMP_DLL%"=="" set "DLL_ARGS=--hspcmp-dll "%HSPCMP_DLL%""
python "%SCRIPT_DIR%run_tests.py" --hspcmp "%HSPCMP%" --common "%SCRIPT_DIR%..\..\common" %DLL_ARGS%
exit /b %ERRORLEVEL%
