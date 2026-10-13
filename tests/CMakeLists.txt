# Standalone Qt-only tests. Configured separately from the app so they build
# without ROS 2, mz_interfaces, OpenCV or HCNetSDK:
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.18)
project(MzTLZTests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Test)

set(MZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

enable_testing()

add_executable(tst_imageviewframe
    tst_imageviewframe.cpp
    ${MZ_ROOT}/src/vision/ImageViewFrame.cpp
    ${MZ_ROOT}/include/vision/ImageViewFrame.h
)
target_include_directories(tst_imageviewframe PRIVATE ${MZ_ROOT}/include)
target_link_libraries(tst_imageviewframe PRIVATE Qt5::Widgets Qt5::Test)

add_test(NAME tst_imageviewframe COMMAND tst_imageviewframe -platform offscreen)
