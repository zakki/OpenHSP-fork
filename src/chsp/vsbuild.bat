REM Batch build script for Visual Studio 2022
echo off
MSBuild win32/chsp.sln -t:Rebuild -p:Configuration=Release;Platform="x86"
MSBuild win32dll/hspcmp.sln -t:Rebuild -p:Configuration=Release;Platform="x86"

if not exist Release mkdir Release

copy /B /Y win32\Release\chsp.exe Release
copy /B /Y win32dll\Release\hspcmp.dll Release
copy /B /Y extlib\tcc\libtcc.dll Release
