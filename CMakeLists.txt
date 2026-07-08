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

set(PYTHON_EXECUTABLE "C:/opt/ros/foxy/x64/python.exe" CACHE FILEPATH "" FORCE)
set(Python3_EXECUTABLE "C:/opt/ros/foxy/x64/python.exe" CACHE FILEPATH "" FORCE)

list(APPEND CMAKE_PREFIX_PATH
    "C:/opt/ros/foxy/x64"
    "D:/QtProj/tongliiz/install/mz_interfaces"
)
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release RelWithDebInfo "")
set(ENV{AMENT_PREFIX_PATH} "C:/opt/ros/foxy/x64;D:/QtProj/tongliiz/install/mz_interfaces")

if(MSVC)
    add_compile_options(/utf-8 /Zm500 /bigobj)
    add_definitions(-DNOMINMAX)
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets PrintSupport Sql)
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(mz_interfaces REQUIRED)
find_package(std_msgs REQUIRED)

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
