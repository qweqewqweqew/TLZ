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
if not defined MZ_HIK_SDK set "MZ_HIK_SDK=C:\Users\28651\Desktop\CH-HCNetSDKV6.1.9.48_build20230410_win64(1)"
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

set "QT_PLUGIN_PATH=%MZ_QT_PREFIX%\plugins"
set "QT_OPENGL=desktop"
if exist "%MZ_ROS_PREFIX%\local_setup.bat" call "%MZ_ROS_PREFIX%\local_setup.bat"
REM After local_setup, so that script cannot drop the Camera_Node bin from PATH.
set "PATH=%MZ_QT_PREFIX%\bin;%OPENCV_BIN%;%MZ_ROS_PREFIX%\bin;%MZ_INTERFACES_PREFIX%\bin;%MZ_INTERFACES_PREFIX%\lib;%PATH%"
set "AMENT_PREFIX_PATH=%MZ_INTERFACES_PREFIX%;%AMENT_PREFIX_PATH%"
set "MISSING=0"
call :copy_interface_dlls "%MZ_INTERFACES_PREFIX%" "%APP_DIR%"
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
if not exist "%APP_DIR%\mz_interfaces__rosidl_typesupport_cpp.dll" (
    echo [error] mz_interfaces__rosidl_typesupport_cpp.dll is still missing next to MzTLZ.exe.
    set "MISSING=1"
)
call :copy_hik_runtime "%MZ_HIK_SDK%" "%APP_DIR%"
set "REPO_SDK=%HERE%\..\third_party\HCNetSDK"
if not exist "%REPO_SDK%" set "REPO_SDK=%HERE%\..\..\third_party\HCNetSDK"
if exist "%REPO_SDK%" call :stage_hik_into_tree "%MZ_HIK_SDK%" "%REPO_SDK%"
if not exist "%APP_DIR%\HCNetSDK.dll" if exist "%REPO_SDK%" call :copy_hik_runtime "%REPO_SDK%" "%APP_DIR%"
if not exist "%APP_DIR%\HCNetSDK.dll" (
    echo [error] HCNetSDK.dll was not copied next to MzTLZ.exe.
    echo        Looked in %MZ_HIK_SDK%
    set "MISSING=1"
)
if "%MISSING%"=="1" goto fail

echo [info] starting %APP_DIR%\MzTLZ.exe
cd /d "%APP_DIR%"
start "" "%APP_DIR%\MzTLZ.exe"
exit /b 0

:copy_hik_runtime
set "HIK_DLL="
set "HIK_LIB="
if "%~1"=="" goto :eof
if "%~2"=="" goto :eof
if not exist "%~1" (
    echo [warn] Hikvision SDK folder not found: %~1
    goto :eof
)
if exist "%~1\HCNetSDK.dll" (
    set "HIK_LIB=%~1"
    goto copy_hik_files
)
pushd "%~1" >nul || goto :eof
for /f "delims=" %%D in ('dir /s /b HCNetSDK.dll 2^>nul') do (
    if not defined HIK_DLL set "HIK_DLL=%%D"
)
popd
if not defined HIK_DLL (
    echo [warn] HCNetSDK.dll not found under %~1
    goto :eof
)
call :hik_lib_from_dll "%HIK_DLL%"
:copy_hik_files
if not defined HIK_LIB goto :eof
echo [info] Copying Hikvision runtime from %HIK_LIB%
copy /Y "%HIK_LIB%\*.dll" "%~2\" >nul
if exist "%HIK_LIB%\HCNetSDKCom" xcopy /E /I /Y "%HIK_LIB%\HCNetSDKCom" "%~2\HCNetSDKCom\" >nul
goto :eof

:stage_hik_into_tree
set "HIK_IMPLIB="
if not exist "%~2" goto :eof
pushd "%~2" >nul || goto :eof
for /f "delims=" %%L in ('dir /s /b HCNetSDK.lib 2^>nul') do (
    if not defined HIK_IMPLIB set "HIK_IMPLIB=%%L"
)
popd
if not defined HIK_IMPLIB goto :eof
call :hik_lib_from_dll "%HIK_IMPLIB%"
set "HIK_DEST=%HIK_LIB%"
call :copy_hik_runtime "%~1" "%HIK_DEST%"
goto :eof

:hik_lib_from_dll
set "HIK_LIB=%~dp1"
goto :eof

:copy_interface_dlls
set "IFACE_DLL="
if "%~1"=="" exit /b 1
if "%~2"=="" exit /b 1
if not exist "%~1" (
    echo [error] mz_interfaces prefix not found: %~1
    exit /b 1
)
pushd "%~1" >nul || exit /b 1
for /f "delims=" %%D in ('dir /s /b mz_interfaces__rosidl_typesupport_cpp.dll 2^>nul') do (
    if not defined IFACE_DLL set "IFACE_DLL=%%D"
)
popd
if not defined IFACE_DLL (
    echo [error] mz_interfaces__rosidl_typesupport_cpp.dll not found under %~1
    echo        Build Camera_Node so that install\mz_interfaces contains this DLL.
    exit /b 1
)
call :hik_lib_from_dll "%IFACE_DLL%"
echo [info] Copying mz_interfaces DLLs from %HIK_LIB%
copy /Y "%HIK_LIB%mz_interfaces*.dll" "%~2\" >nul
if not exist "%~2\mz_interfaces__rosidl_typesupport_cpp.dll" (
    echo [error] Failed to copy mz_interfaces DLLs into %~2
    exit /b 1
)
exit /b 0

:fail
echo.
echo [fail] MzTLZ was not started.
if not "%NO_PAUSE%"=="1" pause
exit /b 1
