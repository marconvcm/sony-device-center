cmake_minimum_required(VERSION 3.14)
project(SonyDeviceCenter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
find_package(Threads REQUIRED)

add_subdirectory(libs/sony-transport)
add_subdirectory(libs/sony-protocol)
add_subdirectory(libs/sony-core)

add_subdirectory(apps/sonyd)
add_subdirectory(apps/sonyctl)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Client/CMakeLists.txt")
    add_subdirectory(Client)
endif()
