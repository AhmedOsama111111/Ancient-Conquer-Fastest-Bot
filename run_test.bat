@echo off
set MSBUILD="E:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
%MSBUILD% test_coords.vcxproj /p:Configuration=Release /p:Platform=x64 /nologo
if %errorlevel% equ 0 (
    test_coords.exe
)
