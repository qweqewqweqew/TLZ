@echo off
setlocal EnableExtensions

REM Stop the three nodes started by start_all.bat.
REM Do not touch MzTLZ.exe.

set "NO_PAUSE=0"
if /I "%~1"=="--no-pause" set "NO_PAUSE=1"

echo ========================================
echo  Copper milling node stop
echo ========================================
echo.

echo stopping algorithm / camera / backend...
taskkill /IM copper_node_core.exe /T /F >nul 2>&1
taskkill /IM copper_node.exe /T /F >nul 2>&1
taskkill /IM camera_node.exe /T /F >nul 2>&1
taskkill /IM camera_mock.exe /T /F >nul 2>&1
taskkill /IM milling_controller.exe /T /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq *copper_node*" /T /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq *camera_node*" /T /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq *milling_controller*" /T /F >nul 2>&1
ping 127.0.0.1 -n 3 >nul
echo done
exit /b 0
