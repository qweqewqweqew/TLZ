cmake_minimum_required(VERSION 3.19)
project(MzTLZ LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets)

set(ELAWIDGETTOOLS_BUILD_STATIC_LIB OFF CACHE BOOL "Build ElaWidgetTools as a shared library." FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/ElaWidgetTools-main/ElaWidgetTools-main/ElaWidgetTools"
    "${CMAKE_CURRENT_BINARY_DIR}/ElaWidgetTools"
)

add_executable(MzTLZ
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(MzTLZ
    PRIVATE
        Qt5::Core
        Qt5::Widgets
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
