@echo off
setlocal EnableExtensions
REM MzTLZ Release build script for the target PC.
REM Do NOT call ROS setup.bat here: it prepends older Qt headers and breaks Qt 5.15.
REM Close MzTLZ.exe before building if the linker cannot overwrite the exe.

set "NO_PAUSE=0"
if /I "%~1"=="--no-pause" set "NO_PAUSE=1"

set "SRC=%~dp0"
REM %~dp0 always ends with \, strip it.
set "SRC=%SRC:~0,-1%"
set "BUILD=%SRC%\build"
set "DEPLOY=%SRC%\deploy\MzTLZ"

echo ========================================
echo  MzTLZ Release build
echo  source: %SRC%
echo  deploy: %DEPLOY%
echo ========================================
echo.

tasklist | findstr /I /C:"MzTLZ.exe" >nul
if not errorlevel 1 (
  echo [warn] MzTLZ.exe is running. Close it if linking or deploy overwrite fails.
  echo.
)

set "VS_SETUP="
for %%P in (
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat"
) do (
    if not defined VS_SETUP if exist "%%~P" set "VS_SETUP=%%~P"
)
if not defined VS_SETUP (
    echo [error] Visual Studio 2019 was not found.
    goto fail
)

echo [info] VS: %VS_SETUP%
call "%VS_SETUP%" -arch=x64
if errorlevel 1 (
    echo [error] Failed to load VS environment.
    goto fail
)

if exist C:\opt\ros\foxy\x64\python.exe (
    set "PYTHON_EXECUTABLE=C:\opt\ros\foxy\x64\python.exe"
    set "Python3_EXECUTABLE=C:\opt\ros\foxy\x64\python.exe"
    set "PATH=C:\opt\ros\foxy\x64;C:\opt\ros\foxy\x64\Scripts;%PATH%"
)

set "QT_PREFIX=D:\Qt\5.15.2\msvc2019_64"
if not exist "%QT_PREFIX%\lib\cmake\Qt5\Qt5Config.cmake" (
    echo [error] Qt5 not found: %QT_PREFIX%
    goto fail
)
set "INCLUDE=%QT_PREFIX%\include;%INCLUDE%"

set "CMAKE_EXE="
if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
if not defined CMAKE_EXE if exist "C:\Program Files\CMake\bin\cmake.exe" set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
if not defined CMAKE_EXE if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined CMAKE_EXE if exist "C:\opt\ros\foxy\x64\Scripts\cmake.exe" set "CMAKE_EXE=C:\opt\ros\foxy\x64\Scripts\cmake.exe"
if not defined CMAKE_EXE (
    echo [error] cmake.exe was not found.
    goto fail
)
echo [info] cmake: %CMAKE_EXE%

if not exist "%SRC%\CMakeLists.txt" (
    echo [error] CMakeLists.txt not found: %SRC%
    goto fail
)
if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%"
if errorlevel 1 goto fail

echo.
echo [info] Configuring...
"%CMAKE_EXE%" -G "Visual Studio 16 2019" -A x64 ^
  -DCMAKE_PREFIX_PATH="D:/Qt/5.15.2/msvc2019_64;D:/opencv/build;C:/opt/ros/foxy/x64;D:/TLZ/Camera_Node/install/mz_interfaces" ^
  -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" ^
  -DOpenCV_DIR="D:/opencv/build" ^
  -DMZ_ROS_PREFIX="C:/opt/ros/foxy/x64" ^
  -DMZ_INTERFACES_PREFIX="D:/TLZ/Camera_Node/install/mz_interfaces" ^
  -DHIKVISION_HCNETSDK_ROOT="%SRC:\=/%/third_party/HCNetSDK" ^
  -DPYTHON_EXECUTABLE="C:/opt/ros/foxy/x64/python.exe" ^
  -DPython3_EXECUTABLE="C:/opt/ros/foxy/x64/python.exe" ^
  -UCMAKE_AUTOGEN_PARALLEL ^
  "%SRC%"
if errorlevel 1 (
    echo [error] cmake configure failed
    goto fail
)

echo.
echo [info] Building Release...
"%CMAKE_EXE%" --build "%BUILD%" --config Release --target MzTLZ --parallel
if errorlevel 1 (
    echo [error] cmake build failed
    goto fail
)

if not exist "%BUILD%\Release\MzTLZ.exe" (
    echo [error] MzTLZ.exe was not produced.
    dir /s /b "%BUILD%\MzTLZ.exe" 2>nul
    goto fail
)

echo.
echo [success] %BUILD%\Release\MzTLZ.exe
dir "%BUILD%\Release\MzTLZ.exe"

set "WINDEPLOYQT=%QT_PREFIX%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
    echo [error] windeployqt not found: %WINDEPLOYQT%
    goto fail
)

echo.
echo [info] Packaging deploy overlay: %DEPLOY%
if not exist "%SRC%\deploy" mkdir "%SRC%\deploy"
if not exist "%DEPLOY%" mkdir "%DEPLOY%"

copy /Y "%BUILD%\Release\MzTLZ.exe" "%DEPLOY%\MzTLZ.exe" >nul
if errorlevel 1 (
    echo [error] Failed to copy MzTLZ.exe into deploy. Close MzTLZ.exe and retry.
    goto fail
)

if exist "%BUILD%\Release\*.dll" copy /Y "%BUILD%\Release\*.dll" "%DEPLOY%\" >nul
if exist "%BUILD%\Release\HCNetSDKCom" (
    xcopy /E /I /Y "%BUILD%\Release\HCNetSDKCom" "%DEPLOY%\HCNetSDKCom" >nul
)

copy /Y "%SRC%\config\database.ini" "%DEPLOY%\" >nul
copy /Y "%SRC%\config\hik_camera.ini" "%DEPLOY%\" >nul
copy /Y "%SRC%\config\node_launcher.ini" "%DEPLOY%\" >nul
copy /Y "%SRC%\config\display.ini" "%DEPLOY%\" >nul
copy /Y "%SRC%\config\start_all.bat" "%DEPLOY%\" >nul
copy /Y "%SRC%\config\stop_all.bat" "%DEPLOY%\" >nul
copy /Y "%SRC%\config\run.bat" "%DEPLOY%\" >nul

echo [info] windeployqt %DEPLOY%\MzTLZ.exe
set "PATH=%QT_PREFIX%\bin;%PATH%"
"%WINDEPLOYQT%" --release --force --compiler-runtime --no-translations --no-angle --no-opengl-sw "%DEPLOY%\MzTLZ.exe"
if errorlevel 1 (
    echo [error] windeployqt failed
    goto fail
)

if exist "%QT_PREFIX%\bin\Qt5PrintSupport.dll" copy /Y "%QT_PREFIX%\bin\Qt5PrintSupport.dll" "%DEPLOY%\" >nul
del /f /q "%DEPLOY%\opengl32sw.dll" "%DEPLOY%\libEGL.dll" "%DEPLOY%\libGLESv2.dll" >nul 2>nul

echo [info] Copying OpenCV / CRT DLLs
set "ROS_BIN=C:\opt\ros\foxy\x64\bin"
set "OPENCV_BIN=D:\opencv\build\x64\vc16\bin"

call :copy_glob "%OPENCV_BIN%\opencv_world490.dll"
call :copy_glob "%OPENCV_BIN%\jpeg62.dll"
call :copy_glob "%OPENCV_BIN%\libpng16.dll"
call :copy_glob "%OPENCV_BIN%\lzma.dll"
call :copy_glob "%OPENCV_BIN%\tiff.dll"
call :copy_glob "%OPENCV_BIN%\webp.dll"

for %%F in (
    msvcp140.dll
    msvcp140_1.dll
    msvcp140_2.dll
    msvcp140_atomic_wait.dll
    msvcp140_codecvt_ids.dll
    concrt140.dll
    vccorlib140.dll
    vcruntime140.dll
    vcruntime140_1.dll
) do (
    if exist "%ROS_BIN%\%%F" (
        copy /Y "%ROS_BIN%\%%F" "%DEPLOY%\" >nul
    ) else if exist "%SystemRoot%\System32\%%F" (
        copy /Y "%SystemRoot%\System32\%%F" "%DEPLOY%\" >nul
    )
)

echo [info] Removing ROS DLLs from deploy so they load from C:\opt\ros
del /f /q "%DEPLOY%\rcl.dll" "%DEPLOY%\rclcpp.dll" "%DEPLOY%\rcutils.dll" "%DEPLOY%\rcpputils.dll" >nul 2>nul
del /f /q "%DEPLOY%\rmw.dll" "%DEPLOY%\rmw_implementation.dll" "%DEPLOY%\rmw_fastrtps_cpp.dll" "%DEPLOY%\rmw_fastrtps_shared_cpp.dll" >nul 2>nul
del /f /q "%DEPLOY%\rcl_logging_spdlog.dll" "%DEPLOY%\rcl_yaml_param_parser.dll" >nul 2>nul
del /f /q "%DEPLOY%\rosidl_runtime_c.dll" "%DEPLOY%\rosidl_typesupport_c.dll" "%DEPLOY%\rosidl_typesupport_cpp.dll" >nul 2>nul
del /f /q "%DEPLOY%\rosidl_typesupport_fastrtps_c.dll" "%DEPLOY%\rosidl_typesupport_fastrtps_cpp.dll" >nul 2>nul
del /f /q "%DEPLOY%\fastcdr-1.0.dll" "%DEPLOY%\fastrtps-2.0.dll" "%DEPLOY%\tinyxml2.dll" "%DEPLOY%\yaml.dll" >nul 2>nul
del /f /q "%DEPLOY%\libstatistics_collector.dll" "%DEPLOY%\ament_index_cpp.dll" "%DEPLOY%\tracetools.dll" >nul 2>nul
del /f /q "%DEPLOY%\builtin_interfaces*.dll" "%DEPLOY%\rcl_interfaces*.dll" "%DEPLOY%\rmw_dds_common*.dll" >nul 2>nul
del /f /q "%DEPLOY%\rosgraph_msgs*.dll" "%DEPLOY%\statistics_msgs*.dll" "%DEPLOY%\std_msgs*.dll" >nul 2>nul
del /f /q "%DEPLOY%\unique_identifier_msgs*.dll" "%DEPLOY%\action_msgs*.dll" "%DEPLOY%\foonathan_memory*.dll" >nul 2>nul
del /f /q "%DEPLOY%\mz_interfaces*.dll" >nul 2>nul
if exist "%DEPLOY%\share" rd /s /q "%DEPLOY%\share"

echo.
echo [success] deploy package: %DEPLOY%\MzTLZ.exe
dir "%DEPLOY%\MzTLZ.exe"
if "%NO_PAUSE%"=="0" pause
exit /b 0

:copy_glob
if exist %1 copy /Y %1 "%DEPLOY%\" >nul
goto :eof

:fail
echo.
echo [fail] MzTLZ build did not succeed.
if "%NO_PAUSE%"=="0" pause
exit /b 1
