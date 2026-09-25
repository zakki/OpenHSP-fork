REM Batch build script for Visual Studio 2022
echo off
MSBuild win32/chsp.sln -t:Clean -p:Configuration=Release;Platform="x86"
MSBuild win32dll/hspcmp.sln -t:Clean -p:Configuration=Release;Platform="x86"
