@echo off
setlocal
set "VERSION=0.1.0-beta"
set "APPDIR=%LOCALAPPDATA%\IkClutStudio\%VERSION%"
set "ZIP=%~dp0IkClutStudio-0.1.0-beta-win64-portable.zip"
set "EXE=%APPDIR%\IkClutStudio-0.1.0-beta-win64\IkClutStudio.exe"

if not exist "%ZIP%" (
    echo Missing runtime package: %ZIP%
    pause
    exit /b 1
)

if not exist "%EXE%" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "New-Item -ItemType Directory -Force -Path '%APPDIR%' | Out-Null; Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%APPDIR%' -Force"
)

if not exist "%EXE%" (
    echo Failed to prepare IKCLUT Studio runtime.
    echo Expected: %EXE%
    pause
    exit /b 1
)

start "" "%EXE%"
exit /b 0
