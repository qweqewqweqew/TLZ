@echo off
setlocal EnableExtensions
set "APP_DIR=%~dp0"
if "%APP_DIR:~-1%"=="\" set "APP_DIR=%APP_DIR:~0,-1%"
set "QT_PREFIX=D:\Qt\5.15.2\msvc2019_64"
set "QT_PLUGIN_PATH=%QT_PREFIX%\plugins"
set "QT_OPENGL=desktop"
if exist C:\opt\ros\foxy\x64\local_setup.bat call C:\opt\ros\foxy\x64\local_setup.bat
REM ROS setup may reorder PATH; prepend the deployed SICK interface afterwards.
set "PATH=%APP_DIR%;D:\TLZ\MZTLZ\.deps\install\bin;%QT_PREFIX%\bin;D:\opencv\build\x64\vc16\bin;C:\opt\ros\foxy\x64\bin;%PATH%"
set "AMENT_PREFIX_PATH=D:\TLZ\MZTLZ\.deps\install;%AMENT_PREFIX_PATH%"
cd /d "%APP_DIR%"
if not exist "%APP_DIR%\MzTLZ.exe" (
  echo [error] MzTLZ.exe not found: %APP_DIR%\MzTLZ.exe
  exit /b 1
)
start "" "%APP_DIR%\MzTLZ.exe"
exit /b 0
