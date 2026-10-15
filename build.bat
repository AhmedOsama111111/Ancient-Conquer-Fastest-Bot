@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
echo [*] Building svchost (Release x64)...

set MSBUILD="E:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD% (
    echo [!] MSBuild not found at %MSBUILD%
    exit /b 1
)

%MSBUILD% ConquerBot.vcxproj /p:Configuration=Release /p:Platform=x64 /p:TargetName=svchost /nologo
if !errorlevel! neq 0 (
    echo [!] Build Failed!
    exit /b 1
)

echo.
echo [+] Build Succeeded! svchost.exe compiled and ready!
echo.
