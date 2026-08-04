cmake_minimum_required(VERSION 3.19)
project(MzTLZ LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_UNITY_BUILD OFF)
set(CMAKE_AUTOGEN_PARALLEL 1)
set(CMAKE_BUILD_PARALLEL_LEVEL 1)
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=1)
set(CMAKE_JOB_POOL_COMPILE compile_pool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    list(PREPEND CMAKE_PREFIX_PATH "${MZ_ROS_PREFIX}")
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
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(mz_interfaces REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Boost REQUIRED)

set(ELAWIDGETTOOLS_BUILD_STATIC_LIB OFF CACHE BOOL "Build ElaWidgetTools as a shared library." FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/ElaWidgetTools-main/ElaWidgetTools-main/ElaWidgetTools"
    "${CMAKE_CURRENT_BINARY_DIR}/ElaWidgetTools"
)

add_executable(MzTLZ
    WIN32 MACOSX_BUNDLE
    AppStyle.cpp
    AppStyle.h
    Database.cpp
    Database.h
    EventLogPanel.cpp
    EventLogPanel.h
    HistoryDialog.cpp
    HistoryDialog.h
    ImageDisplayPanel.cpp
    ImageDisplayPanel.h
    ImageViewFrame.cpp
    ImageViewFrame.h
    InspectionRepository.cpp
    InspectionRepository.h
    main.cpp
    Logger.h
    mainwindow.cpp
    mainwindow.h
    MillingPathVM.h
    PlcFeedbackVM.h
    PlcPathCommandParamsVM.h
    Ros2Bridge.cpp
    Ros2Bridge.h
    ShmImageReader.cpp
    ShmImageReader.h
    TeachingDialog.cpp
    TeachingDialog.h
    RunDataPanel.cpp
    RunDataPanel.h
    RunningStatusPanel.cpp
    RunningStatusPanel.h
    TaskFlowPanel.cpp
    TaskFlowPanel.h
    TelemetryPlotWidget.cpp
    TelemetryPlotWidget.h
    TitleBar.cpp
    TitleBar.h
    UiHelpers.cpp
    UiHelpers.h
    mainwindow.ui
    qcustomplot.cpp
    qcustomplot.h
    resources.qrc
)

set_property(TARGET MzTLZ PROPERTY JOB_POOL_COMPILE compile_pool)

target_link_libraries(MzTLZ
    PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::PrintSupport
        Qt5::Sql
        ElaWidgetTools
        ${rclcpp_LIBRARIES}
        ${mz_interfaces_LIBRARIES}
        ${std_msgs_LIBRARIES}
)

target_include_directories(MzTLZ PRIVATE
    ${rclcpp_INCLUDE_DIRS}
    ${mz_interfaces_INCLUDE_DIRS}
    ${std_msgs_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

add_custom_command(TARGET MzTLZ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ElaWidgetTools>
        $<TARGET_FILE_DIR:MzTLZ>
)

include(GNUInstallDirs)

install(TARGETS MzTLZ
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
