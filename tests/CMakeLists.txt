find_package(Qt5 5.15 REQUIRED COMPONENTS Core Sql Test)

add_executable(MzTLZ_auth_tests
    test_auth.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/PasswordHasher.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/PasswordPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AccountSchema.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthService.cpp
    ${CMAKE_SOURCE_DIR}/include/auth/AuthService.h
)

target_include_directories(MzTLZ_auth_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(MzTLZ_auth_tests PRIVATE Qt5::Core Qt5::Sql Qt5::Test)

add_test(NAME auth COMMAND MzTLZ_auth_tests)
