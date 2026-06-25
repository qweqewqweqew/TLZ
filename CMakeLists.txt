cmake_minimum_required(VERSION 3.19)
project(MzTLZ LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_UNITY_BUILD OFF)
set(CMAKE_AUTOGEN_PARALLEL 1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /Zm500 /bigobj)
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets PrintSupport WebSockets)

set(ELAWIDGETTOOLS_BUILD_STATIC_LIB OFF CACHE BOOL "Build ElaWidgetTools as a shared library." FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/ElaWidgetTools-main/ElaWidgetTools-main/ElaWidgetTools"
    "${CMAKE_CURRENT_BINARY_DIR}/ElaWidgetTools"
)

add_executable(MzTLZ
    WIN32 MACOSX_BUNDLE
    AppStyle.cpp
    AppStyle.h
    EventLogPanel.cpp
    EventLogPanel.h
    ImageDisplayPanel.cpp
    ImageDisplayPanel.h
    ImageViewFrame.cpp
    ImageViewFrame.h
    main.cpp
    Logger.h
    mainwindow.cpp
    mainwindow.h
    RunDataPanel.cpp
    RunDataPanel.h
    RunningStatusPanel.cpp
    RunningStatusPanel.h
    TelemetryPlotWidget.cpp
    TelemetryPlotWidget.h
    TelemetryWebSocketServer.cpp
    TelemetryWebSocketServer.h
    TitleBar.cpp
    TitleBar.h
    UiHelpers.cpp
    UiHelpers.h
    mainwindow.ui
    qcustomplot.cpp
    qcustomplot.h
    resources.qrc
)

target_link_libraries(MzTLZ
    PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::PrintSupport
        Qt5::WebSockets
        ElaWidgetTools
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
