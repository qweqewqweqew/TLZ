@echo off
setlocal EnableExtensions

REM One-click start for the three nodes.
REM Order: kill old processes/windows -> algorithm -> camera -> backend
REM Each child gets its own ROS overlay. Do NOT set PATH/AMENT in this
REM launcher: leaking the algorithm prefix into camera/backend breaks DDS
REM matching so milling_controller never receives plc/feedback_ack.

set "NO_PAUSE=0"
if /I "%~1"=="--no-pause" set "NO_PAUSE=1"

REM Site paths: put overrides in start_all.local.bat next to this script
REM (see start_all.local.bat.example) or set the MZ_* environment variables.
if exist "%~dp0start_all.local.bat" call "%~dp0start_all.local.bat"
if not defined MZ_ALGO_ROOT set "MZ_ALGO_ROOT=D:\robot_client_move_test"
if not defined MZ_CAMERA_DIR set "MZ_CAMERA_DIR=D:\Camera_Node"
if not defined MZ_BACKEND_ROOT set "MZ_BACKEND_ROOT=D:\CopperMilling"
if not defined MZ_ROS_PREFIX set "MZ_ROS_PREFIX=C:\opt\ros\foxy\x64"
if not defined MZ_OPENCV_BIN set "MZ_OPENCV_BIN=C:\opencv\build\x64\vc15\bin"
if not defined MZ_PLC_IP set "MZ_PLC_IP=192.168.1.60"
if not defined MZ_MILLING_ARGS set "MZ_MILLING_ARGS=-p plc_ip:=%MZ_PLC_IP% -p plc_rack:=0 -p plc_slot:=1 -p task_id_base:=1 -p task_count:=1"

set "ALGO_ROOT=%MZ_ALGO_ROOT%"
set "ALGO_EXE=%ALGO_ROOT%\copper_node.exe"
set "CAMERA_DIR=%MZ_CAMERA_DIR%"
set "CAMERA_IFACES=%CAMERA_DIR%\install\mz_interfaces"
set "CAMERA_SENSOR=%CAMERA_DIR%\install\mz_sensor"
set "CAMERA_RUNTIME=%CAMERA_SENSOR%\lib\mz_sensor"
set "CAMERA_EXE=%CAMERA_RUNTIME%\camera_node.exe"
set "CAMERA_CONFIG=%CAMERA_SENSOR%\share\mz_sensor\config\camera_node.yaml"
set "CAMERA_SICK_CSV=%CAMERA_DIR%\mz_sensor\config\Configuration-io.csv"
set "BACKEND_ROOT=%MZ_BACKEND_ROOT%"
set "BACKEND_EXE=%BACKEND_ROOT%\install\copper_milling\lib\copper_milling\milling_controller.exe"
set "BACKEND_LOCAL_SETUP=%BACKEND_ROOT%\install\local_setup.bat"
set "ROS_PREFIX=%MZ_ROS_PREFIX%"
set "ROS_LOCAL_SETUP=%ROS_PREFIX%\local_setup.bat"
set "MILLING_ARGS=%MZ_MILLING_ARGS%"

echo ========================================
echo  Copper milling one-click start
echo ========================================
echo.

if not exist "%ALGO_EXE%" (
  echo [ERROR] algorithm not found: %ALGO_EXE%
  goto fail
)
if not exist "%CAMERA_EXE%" (
  echo [ERROR] camera node not found: %CAMERA_EXE%
  echo         build Camera_Node first
  goto fail
)
if not exist "%CAMERA_IFACES%\share\ament_index\resource_index\packages\mz_interfaces" (
  echo [ERROR] camera mz_interfaces overlay not found: %CAMERA_IFACES%
  goto fail
)
if not exist "%CAMERA_SENSOR%\share\ament_index\resource_index\packages\mz_sensor" (
  echo [ERROR] camera mz_sensor overlay not found: %CAMERA_SENSOR%
  goto fail
)
if not exist "%CAMERA_CONFIG%" (
  echo [ERROR] camera config not found: %CAMERA_CONFIG%
  goto fail
)
if not exist "%CAMERA_SICK_CSV%" (
  echo [ERROR] SICK configuration not found: %CAMERA_SICK_CSV%
  goto fail
)
if not exist "%BACKEND_EXE%" (
  echo [ERROR] backend not built: %BACKEND_EXE%
  echo         run scripts\build.bat in CopperMilling first
  goto fail
)
if not exist "%BACKEND_LOCAL_SETUP%" (
  echo [ERROR] backend overlay not found: %BACKEND_LOCAL_SETUP%
  goto fail
)
if not exist "%ROS_LOCAL_SETUP%" (
  echo [ERROR] ROS overlay not found: %ROS_LOCAL_SETUP%
  goto fail
)

echo [0/3] killing previous processes and cmd windows...
taskkill /IM copper_node_core.exe /T /F >nul 2>&1
taskkill /IM copper_node.exe /T /F >nul 2>&1
taskkill /IM camera_node.exe /T /F >nul 2>&1
taskkill /IM camera_mock.exe /T /F >nul 2>&1
taskkill /IM milling_controller.exe /T /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq *copper_node*" /T /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq *camera_node*" /T /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq *milling_controller*" /T /F >nul 2>&1
call :wait 2
echo       done
echo.

echo [1/3] starting algorithm node
echo       %ALGO_EXE%
start "copper_node" /D "%ALGO_ROOT%" cmd /c "title copper_node && set PATH=%ALGO_ROOT%;%ALGO_ROOT%\bin;%PATH% && set AMENT_PREFIX_PATH=%ALGO_ROOT%;%ROS_PREFIX% && set COLCON_PREFIX_PATH=%ALGO_ROOT% && copper_node.exe"
call :wait 8
echo.

echo [2/3] starting camera node
echo       %CAMERA_EXE%
start "camera_node" /D "%CAMERA_DIR%" cmd /c "title camera_node && set AMENT_PREFIX_PATH=& set COLCON_PREFIX_PATH=& set CMAKE_PREFIX_PATH=& set PATH=%CAMERA_RUNTIME%;%CAMERA_IFACES%\bin;%PATH% && set GENICAM_GENTL64_PATH=%CAMERA_RUNTIME% && call %ROS_LOCAL_SETUP% && set AMENT_PREFIX_PATH=%CAMERA_SENSOR%;%CAMERA_IFACES%;%ROS_PREFIX% && set COLCON_PREFIX_PATH=%CAMERA_SENSOR%;%CAMERA_IFACES% && %CAMERA_EXE% --ros-args --params-file %CAMERA_CONFIG% -p sick.configuration_file:=%CAMERA_SICK_CSV%"
call :wait 5
echo.

echo [3/3] starting backend node milling_controller
start "milling_controller" cmd /c "title milling_controller && cd /d %BACKEND_ROOT% && set AMENT_PREFIX_PATH=& set COLCON_PREFIX_PATH=& set CMAKE_PREFIX_PATH=& set PATH=%MZ_OPENCV_BIN%;%PATH% && call %ROS_LOCAL_SETUP% && call %BACKEND_LOCAL_SETUP% && %BACKEND_EXE% --ros-args %MILLING_ARGS%"

exit /b 0

:wait
set /a "_n=%~1+1"
ping 127.0.0.1 -n %_n% >nul
goto :eof

:fail
echo.
echo Paths can be changed in %~dp0start_all.local.bat (see start_all.local.bat.example).
if not "%NO_PAUSE%"=="1" pause
exit /b 1
