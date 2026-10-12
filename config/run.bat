@echo off
setlocal EnableExtensions
REM Same runtime environment as tmp_run_mztlz.bat, but starts the deploy exe.
REM ROS / mz_interfaces DLLs must come from the machine install, not this folder.

set "APP_DIR=%~dp0"
if "%APP_DIR:~-1%"=="\" set "APP_DIR=%APP_DIR:~0,-1%"

set "QT_PREFIX=C:\Qt\5.15.2\msvc2019_64"
set "PATH=%QT_PREFIX%\bin;C:\opencv\build\x64\vc15\bin;C:\opt\ros\foxy\x64\bin;D:\Camera_Node\install\mz_interfaces\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_PREFIX%\plugins"
set "QT_OPENGL=desktop"

if exist C:\opt\ros\foxy\x64\local_setup.bat (
  call C:\opt\ros\foxy\x64\local_setup.bat
)
set "AMENT_PREFIX_PATH=D:\Camera_Node\install\mz_interfaces;%AMENT_PREFIX_PATH%"

cd /d "%APP_DIR%"
if not exist "%APP_DIR%\MzTLZ.exe" (
  echo [error] MzTLZ.exe not found: %APP_DIR%\MzTLZ.exe
  exit /b 1
)
start "" "%APP_DIR%\MzTLZ.exe"
exit /b 0
