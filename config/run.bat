@echo off
setlocal EnableExtensions
REM Starts deploy\MzTLZ\MzTLZ.exe with Qt / OpenCV / ROS / mz_interfaces on PATH.
REM Double-clicking build\Release\MzTLZ.exe skips this and fails on missing DLLs.
REM ROS DLLs are intentionally not copied into deploy; they load from MZ_ROS_PREFIX.

set "NO_PAUSE=0"
if /I "%~1"=="--no-pause" set "NO_PAUSE=1"

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

if exist "%HERE%\build.local.bat" call "%HERE%\build.local.bat"
if exist "%HERE%\..\build.local.bat" call "%HERE%\..\build.local.bat"
if exist "%HERE%\..\..\build.local.bat" call "%HERE%\..\..\build.local.bat"

if not defined MZ_QT_PREFIX set "MZ_QT_PREFIX=C:\Qt\5.15.2\msvc2019_64"
if not defined MZ_OPENCV_DIR set "MZ_OPENCV_DIR=C:\opencv\build"
if not defined MZ_ROS_PREFIX set "MZ_ROS_PREFIX=C:\opt\ros\foxy\x64"
if not defined MZ_INTERFACES_PREFIX set "MZ_INTERFACES_PREFIX=D:\Camera_Node\install\mz_interfaces"
set "OPENCV_BIN=%MZ_OPENCV_DIR%\x64\vc15\bin"

set "APP_DIR="
if exist "%HERE%\MzTLZ.exe" set "APP_DIR=%HERE%"
if not defined APP_DIR if exist "%HERE%\..\deploy\MzTLZ\MzTLZ.exe" set "APP_DIR=%HERE%\..\deploy\MzTLZ"
if not defined APP_DIR if exist "%HERE%\deploy\MzTLZ\MzTLZ.exe" set "APP_DIR=%HERE%\deploy\MzTLZ"
if not defined APP_DIR (
    echo [error] MzTLZ.exe not found.
    echo        Use deploy\MzTLZ\MzTLZ.exe after build.bat, or run this script from config\ or deploy\MzTLZ.
    echo        build\Release\MzTLZ.exe has no Qt/OpenCV/ROS DLLs beside it.
    goto fail
)
for %%I in ("%APP_DIR%") do set "APP_DIR=%%~fI"

set "PATH=%MZ_QT_PREFIX%\bin;%OPENCV_BIN%;%MZ_ROS_PREFIX%\bin;%MZ_INTERFACES_PREFIX%\bin;%PATH%"
set "QT_PLUGIN_PATH=%MZ_QT_PREFIX%\plugins"
set "QT_OPENGL=desktop"
if exist "%MZ_ROS_PREFIX%\local_setup.bat" call "%MZ_ROS_PREFIX%\local_setup.bat"
set "AMENT_PREFIX_PATH=%MZ_INTERFACES_PREFIX%;%AMENT_PREFIX_PATH%"

set "MISSING=0"
if not exist "%APP_DIR%\Qt5Core.dll" if not exist "%MZ_QT_PREFIX%\bin\Qt5Core.dll" (
    echo [error] Qt5Core.dll missing. Expected %MZ_QT_PREFIX%\bin or next to MzTLZ.exe
    set "MISSING=1"
)
if not exist "%APP_DIR%\opencv_world346.dll" if not exist "%OPENCV_BIN%\opencv_world346.dll" (
    echo [error] opencv_world346.dll missing. Expected %OPENCV_BIN%
    set "MISSING=1"
)
if not exist "%MZ_ROS_PREFIX%\bin\rclcpp.dll" (
    echo [error] rclcpp.dll missing. Expected %MZ_ROS_PREFIX%\bin
    echo        build.bat removes ROS DLLs from deploy so they load from this install.
    set "MISSING=1"
)
if not exist "%MZ_INTERFACES_PREFIX%\bin" (
    echo [error] mz_interfaces bin missing: %MZ_INTERFACES_PREFIX%\bin
    set "MISSING=1"
)
if not exist "%APP_DIR%\HCNetSDK.dll" (
    echo [warn] HCNetSDK.dll is not next to MzTLZ.exe. The process will not start until that DLL is in deploy\MzTLZ.
)
if "%MISSING%"=="1" goto fail

echo [info] starting %APP_DIR%\MzTLZ.exe
cd /d "%APP_DIR%"
start "" "%APP_DIR%\MzTLZ.exe"
exit /b 0

:fail
echo.
echo [fail] MzTLZ was not started.
if not "%NO_PAUSE%"=="1" pause
exit /b 1
