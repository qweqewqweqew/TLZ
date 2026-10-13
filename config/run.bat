@echo off
setlocal EnableExtensions
set "APP_DIR=%~dp0"
if "%APP_DIR:~-1%"=="\" set "APP_DIR=%APP_DIR:~0,-1%"
set "QT_PREFIX=D:\Qt\5.15.2\msvc2019_64"
set "PATH=%QT_PREFIX%\bin;D:\opencv\build\x64\vc16\bin;C:\opt\ros\foxy\x64\bin;D:\TLZ\Camera_Node\install\mz_interfaces\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_PREFIX%\plugins"
set "QT_OPENGL=desktop"
if exist C:\opt\ros\foxy\x64\local_setup.bat call C:\opt\ros\foxy\x64\local_setup.bat
set "AMENT_PREFIX_PATH=D:\TLZ\Camera_Node\install\mz_interfaces;%AMENT_PREFIX_PATH%"
cd /d "%APP_DIR%"
if not exist "%APP_DIR%\MzTLZ.exe" (
  echo [error] MzTLZ.exe not found: %APP_DIR%\MzTLZ.exe
  exit /b 1
)
start "" "%APP_DIR%\MzTLZ.exe"
exit /b 0
