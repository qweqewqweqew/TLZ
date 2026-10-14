cmake_minimum_required(VERSION 3.18)
project(MzTLZ LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_UNITY_BUILD OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/database.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/database.ini"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/hik_camera.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/hik_camera.ini"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/node_launcher.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/node_launcher.ini"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/display.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/display.ini"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/backend_params.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/backend_params.ini"
    COPYONLY
)

set(MZ_ROS_PREFIX "$ENV{MZ_ROS_PREFIX}" CACHE PATH "ROS 2 installation prefix")
if(NOT MZ_ROS_PREFIX)
    foreach(_ros_env_var ROS2_ROOT ROS_FOXY_ROOT ROS_ROOT)
        file(TO_CMAKE_PATH "$ENV{${_ros_env_var}}" _ros_prefix_candidate)
        if(EXISTS "${_ros_prefix_candidate}/share/ament_cmake/cmake/ament_cmakeConfig.cmake")
            set(MZ_ROS_PREFIX "${_ros_prefix_candidate}" CACHE PATH "ROS 2 installation prefix" FORCE)
            break()
        endif()
    endforeach()
endif()
if(NOT MZ_ROS_PREFIX)
    find_program(_ROS2_EXECUTABLE NAMES ros2.exe ros2)
    if(_ROS2_EXECUTABLE)
        get_filename_component(_ros_scripts_dir "${_ROS2_EXECUTABLE}" DIRECTORY)
        get_filename_component(_ros_prefix_candidate "${_ros_scripts_dir}" DIRECTORY)
        if(EXISTS "${_ros_prefix_candidate}/share/ament_cmake/cmake/ament_cmakeConfig.cmake")
            set(MZ_ROS_PREFIX "${_ros_prefix_candidate}" CACHE PATH "ROS 2 installation prefix" FORCE)
        endif()
    endif()
endif()

set(MZ_INTERFACES_PREFIX "$ENV{MZ_INTERFACES_PREFIX}" CACHE PATH "mz_interfaces installation prefix")
if(NOT MZ_INTERFACES_PREFIX)
    get_filename_component(
        _interfaces_prefix_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/../Camera_Node/install/mz_interfaces"
        ABSOLUTE
    )
    if(EXISTS "${_interfaces_prefix_candidate}/share/mz_interfaces/cmake/mz_interfacesConfig.cmake")
        set(MZ_INTERFACES_PREFIX "${_interfaces_prefix_candidate}"
            CACHE PATH "mz_interfaces installation prefix" FORCE)
    endif()
endif()

if(MZ_ROS_PREFIX)
    # Append so official Qt 5.15 stays ahead of ROS's bundled Qt headers.
    list(APPEND CMAKE_PREFIX_PATH "${MZ_ROS_PREFIX}")
    if(EXISTS "${MZ_ROS_PREFIX}/python.exe")
        set(PYTHON_EXECUTABLE "${MZ_ROS_PREFIX}/python.exe" CACHE FILEPATH "" FORCE)
        set(Python3_EXECUTABLE "${MZ_ROS_PREFIX}/python.exe" CACHE FILEPATH "" FORCE)
    endif()
endif()

if(NOT EXISTS "${MZ_INTERFACES_PREFIX}/share/mz_interfaces/cmake/mz_interfacesConfig.cmake")
    message(FATAL_ERROR
        "未找到 mz_interfaces。请先构建 Camera_Node，或设置 "
        "MZ_INTERFACES_PREFIX 指向 mz_interfaces 安装前缀。")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${MZ_INTERFACES_PREFIX}")

set(_ament_prefixes "${MZ_INTERFACES_PREFIX}")
if(MZ_ROS_PREFIX)
    list(APPEND _ament_prefixes "${MZ_ROS_PREFIX}")
endif()
if(NOT "$ENV{AMENT_PREFIX_PATH}" STREQUAL "")
    list(APPEND _ament_prefixes "$ENV{AMENT_PREFIX_PATH}")
endif()
list(JOIN _ament_prefixes ";" _ament_prefix_path)
set(ENV{AMENT_PREFIX_PATH} "${_ament_prefix_path}")

set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release RelWithDebInfo "")
set(mz_interfaces_DIR
    "${MZ_INTERFACES_PREFIX}/share/mz_interfaces/cmake"
    CACHE PATH "mz_interfaces package directory" FORCE
)

if(MSVC)
    add_compile_options(/utf-8 /Zm500 /bigobj)
    add_definitions(-DNOMINMAX)
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets PrintSupport Sql)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(mz_interfaces REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Boost REQUIRED)

# 随工程携带的 Windows HCNetSDK（头文件 + 库文件）。存在时优先使用，
# 避免沿用本机缓存或环境变量里的外部路径。
get_filename_component(
    _hikvision_sdk_vendored
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/HCNetSDK"
    ABSOLUTE
)
set(HIKVISION_HCNETSDK_ROOT "$ENV{HIKVISION_HCNETSDK_ROOT}"
    CACHE PATH "Hikvision HCNetSDK root directory")
if(EXISTS "${_hikvision_sdk_vendored}/头文件/HCNetSDK.h")
    set(HIKVISION_HCNETSDK_ROOT "${_hikvision_sdk_vendored}"
        CACHE PATH "Hikvision HCNetSDK root directory" FORCE)
elseif(NOT HIKVISION_HCNETSDK_ROOT)
    get_filename_component(
        _hikvision_sdk_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Darkroom/CH-HCNetSDKV6.1.9.48_build20230410_win64_20250312151005/CH-HCNetSDKV6.1.9.48_build20230410_win64"
        ABSOLUTE
    )
    if(EXISTS "${_hikvision_sdk_candidate}/头文件/HCNetSDK.h")
        set(HIKVISION_HCNETSDK_ROOT "${_hikvision_sdk_candidate}"
            CACHE PATH "Hikvision HCNetSDK root directory" FORCE)
    endif()
endif()

find_path(HIKVISION_HCNETSDK_INCLUDE_DIR
    NAMES HCNetSDK.h
    PATHS
        "${HIKVISION_HCNETSDK_ROOT}/头文件"
        "${HIKVISION_HCNETSDK_ROOT}/include"
    NO_DEFAULT_PATH
)
find_library(HIKVISION_HCNETSDK_LIBRARY
    NAMES HCNetSDK
    PATHS
        "${HIKVISION_HCNETSDK_ROOT}/库文件"
        "${HIKVISION_HCNETSDK_ROOT}/lib"
    NO_DEFAULT_PATH
)
if(NOT HIKVISION_HCNETSDK_INCLUDE_DIR OR NOT HIKVISION_HCNETSDK_LIBRARY)
    message(FATAL_ERROR
        "未找到海康 HCNetSDK。请设置 HIKVISION_HCNETSDK_ROOT 缓存变量或同名环境变量。")
endif()

set(ELAWIDGETTOOLS_BUILD_STATIC_LIB OFF CACHE BOOL "Build ElaWidgetTools as a shared library." FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/ElaWidgetTools-main/ElaWidgetTools-main/ElaWidgetTools"
    "${CMAKE_CURRENT_BINARY_DIR}/ElaWidgetTools"
    EXCLUDE_FROM_ALL
)

set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/app")

add_executable(MzTLZ
    WIN32 MACOSX_BUNDLE
    src/app/main.cpp
    src/app/mainwindow.cpp
    include/app/mainwindow.h
    src/app/mainwindow.ui
    src/app/AppStyle.cpp
    include/app/AppStyle.h
    src/app/TitleBar.cpp
    include/app/TitleBar.h
    src/app/UiHelpers.cpp
    include/app/UiHelpers.h
    src/app/RuntimeIniPaths.cpp
    include/app/RuntimeIniPaths.h
    include/app/Logger.h
    include/app/Theme.h
    src/auth/AuthTypes.cpp
    include/auth/AuthTypes.h
    src/auth/PasswordHasher.cpp
    include/auth/PasswordHasher.h
    src/auth/PasswordPolicy.cpp
    include/auth/PasswordPolicy.h
    src/auth/AccountSchema.cpp
    include/auth/AccountSchema.h
    src/auth/AuthService.cpp
    include/auth/AuthService.h
    src/ui/panels/DataStatisticsPanel.cpp
    include/ui/panels/DataStatisticsPanel.h
    src/ui/panels/DeviceStatusPanel.cpp
    include/ui/panels/DeviceStatusPanel.h
    src/ui/panels/EventLogPanel.cpp
    include/ui/panels/EventLogPanel.h
    src/ui/panels/ImageDisplayPanel.cpp
    include/ui/panels/ImageDisplayPanel.h
    src/ui/panels/RunDataPanel.cpp
    include/ui/panels/RunDataPanel.h
    src/ui/panels/RunningStatusPanel.cpp
    include/ui/panels/RunningStatusPanel.h
    src/ui/panels/TaskFlowPanel.cpp
    include/ui/panels/TaskFlowPanel.h
    src/ui/dialogs/HistoryDialog.cpp
    include/ui/dialogs/HistoryDialog.h
    src/ui/dialogs/BackendParamsDialog.cpp
    include/ui/dialogs/BackendParamsDialog.h
    src/ui/dialogs/TeachingDialog.cpp
    include/ui/dialogs/TeachingDialog.h
    src/ui/dialogs/LoginDialog.cpp
    include/ui/dialogs/LoginDialog.h
    src/ui/settings/SettingsSubPage.cpp
    include/ui/settings/SettingsSubPage.h
    src/ui/settings/SettingsFormWidgets.cpp
    include/ui/settings/SettingsFormWidgets.h
    src/ui/settings/SettingsPage.cpp
    include/ui/settings/SettingsPage.h
    src/ui/settings/ProcessSettingsPage.cpp
    include/ui/settings/ProcessSettingsPage.h
    src/ui/settings/VisionSettingsPage.cpp
    include/ui/settings/VisionSettingsPage.h
    src/ui/settings/DisplaySettingsPage.cpp
    include/ui/settings/DisplaySettingsPage.h
    src/ui/settings/NodeCommSettingsPage.cpp
    include/ui/settings/NodeCommSettingsPage.h
    src/ui/settings/AlarmSettingsPage.cpp
    include/ui/settings/AlarmSettingsPage.h
    src/ui/settings/AccountSettingsPage.cpp
    include/ui/settings/AccountSettingsPage.h
    src/ui/settings/AboutSettingsPage.cpp
    include/ui/settings/AboutSettingsPage.h
    src/ui/widgets/TelemetryPlotWidget.cpp
    include/ui/widgets/TelemetryPlotWidget.h
    src/ui/widgets/VerticalGauge.cpp
    include/ui/widgets/VerticalGauge.h
    src/vision/HeightMap3DFrame.cpp
    include/vision/HeightMap3DFrame.h
    src/vision/ImageViewFrame.cpp
    include/vision/ImageViewFrame.h
    include/vision/PointCloudFrame.h
    src/camera/HikCameraClient.cpp
    include/camera/HikCameraClient.h
    src/camera/HikCameraConfig.cpp
    include/camera/HikCameraConfig.h
    src/camera/HikLiveView.cpp
    include/camera/HikLiveView.h
    src/bridge/Ros2Bridge.cpp
    include/bridge/Ros2Bridge.h
    src/bridge/ShmImageReader.cpp
    include/bridge/ShmImageReader.h
    src/bridge/NodeLifecycleMonitor.cpp
    include/bridge/NodeLifecycleMonitor.h
    include/bridge/MillingPathVM.h
    include/bridge/PlcFeedbackVM.h
    include/bridge/PlcStatusPresentation.h
    src/data/Database.cpp
    include/data/Database.h
    src/data/DatabaseBootstrap.cpp
    include/data/DatabaseBootstrap.h
    src/data/InspectionRepository.cpp
    include/data/InspectionRepository.h
    config/database.ini
    config/node_launcher.ini
    config/display.ini
    config/backend_params.ini
    config/start_all.bat
    config/stop_all.bat
    third_party/qcustomplot/qcustomplot.cpp
    third_party/qcustomplot/qcustomplot.h
    resources.qrc
)

target_link_libraries(MzTLZ
    PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::PrintSupport
        Qt5::Sql
        ElaWidgetTools
        ${OpenCV_LIBS}
        ${rclcpp_LIBRARIES}
        ${mz_interfaces_LIBRARIES}
        ${std_msgs_LIBRARIES}
        ${HIKVISION_HCNETSDK_LIBRARY}
)

get_target_property(_qt5_core_includes Qt5::Core INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(MzTLZ BEFORE PRIVATE ${_qt5_core_includes})
target_include_directories(MzTLZ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot
    ${OpenCV_INCLUDE_DIRS}
    ${rclcpp_INCLUDE_DIRS}
    ${mz_interfaces_INCLUDE_DIRS}
    ${std_msgs_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${HIKVISION_HCNETSDK_INCLUDE_DIR}
)

add_custom_command(TARGET MzTLZ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ElaWidgetTools>
        $<TARGET_FILE_DIR:MzTLZ>
)
add_custom_command(TARGET MzTLZ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/node_launcher.ini"
        $<TARGET_FILE_DIR:MzTLZ>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/display.ini"
        $<TARGET_FILE_DIR:MzTLZ>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/database.ini"
        $<TARGET_FILE_DIR:MzTLZ>
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/config/backend_params.ini
        -DDST=$<TARGET_FILE_DIR:MzTLZ>/backend_params.ini
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyIfMissing.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/start_all.bat"
        $<TARGET_FILE_DIR:MzTLZ>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/stop_all.bat"
        $<TARGET_FILE_DIR:MzTLZ>
)

foreach(_opencv_target opencv_core opencv_imgproc opencv_imgcodecs)
    add_custom_command(TARGET MzTLZ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${_opencv_target}>
            $<TARGET_FILE_DIR:MzTLZ>
    )
endforeach()

get_filename_component(_hikvision_library_dir "${HIKVISION_HCNETSDK_LIBRARY}" DIRECTORY)
file(GLOB _hikvision_runtime_dlls "${_hikvision_library_dir}/*.dll")
# ninja 生成的 post-build.bat 按系统代码页解析，中文「库文件」会乱码导致
# copy_if_different 失败。拷贝改走 cmake -P，路径只出现在 UTF-8 脚本里。
set(_hikvision_copy_script "${CMAKE_CURRENT_BINARY_DIR}/copy_hikvision_runtime.cmake")
file(WRITE "${_hikvision_copy_script}"
"set(_hik_src \"${_hikvision_library_dir}\")
if(NOT DEST)
  message(FATAL_ERROR \"DEST not set\")
endif()
file(GLOB _hik_dlls \"\${_hik_src}/*.dll\")
foreach(_hik_dll IN LISTS _hik_dlls)
  execute_process(
    COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"\${_hik_dll}\" \"\${DEST}\"
    RESULT_VARIABLE _hik_copy_rc)
  if(_hik_copy_rc)
    message(FATAL_ERROR \"Failed to copy \${_hik_dll}\")
  endif()
endforeach()
if(EXISTS \"\${_hik_src}/HCNetSDKCom\")
  execute_process(
    COMMAND \"${CMAKE_COMMAND}\" -E copy_directory \"\${_hik_src}/HCNetSDKCom\" \"\${DEST}/HCNetSDKCom\"
    RESULT_VARIABLE _hik_copy_rc)
  if(_hik_copy_rc)
    message(FATAL_ERROR \"Failed to copy HCNetSDKCom\")
  endif()
endif()
")
add_custom_command(TARGET MzTLZ POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DDEST=$<TARGET_FILE_DIR:MzTLZ>
        -P "${_hikvision_copy_script}"
)

option(MZTLZ_BUILD_TESTS "Build MzTLZ unit tests" ON)
if(MZTLZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS MzTLZ
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(TARGETS ElaWidgetTools
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES config/hik_camera.ini DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/node_launcher.ini config/display.ini config/database.ini config/backend_params.ini config/start_all.bat config/stop_all.bat DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${_hikvision_runtime_dlls}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(EXISTS "${_hikvision_library_dir}/HCNetSDKCom")
    install(DIRECTORY "${_hikvision_library_dir}/HCNetSDKCom"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING PATTERN "*.dll"
    )
endif()
