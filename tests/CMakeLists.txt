cmake_minimum_required(VERSION 3.14)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SonyBridgeTests LANGUAGES CXX)
    include(CTest)
endif()

if(NOT BUILD_TESTING)
    return()
endif()

find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(Catch2
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.8.1
        URL_HASH SHA256=18b3f70ac80fccc340d8c6ff0f339b2ae64944782f8d2fca2bd705cf47cadb79
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

find_package(Threads REQUIRED)
include(Catch)

set(SONYBRIDGE_CLIENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Client")
add_executable(sony-protocol-tests
    protocol/CommandSerializerTests.cpp
    protocol/BluetoothWrapperTests.cpp
    "${SONYBRIDGE_CLIENT_DIR}/CommandSerializer.cpp"
    "${SONYBRIDGE_CLIENT_DIR}/ByteMagic.cpp"
    "${SONYBRIDGE_CLIENT_DIR}/BluetoothWrapper.cpp"
)
target_compile_features(sony-protocol-tests PRIVATE cxx_std_20)
target_include_directories(sony-protocol-tests PRIVATE "${SONYBRIDGE_CLIENT_DIR}")
target_link_libraries(sony-protocol-tests PRIVATE Catch2::Catch2WithMain Threads::Threads)
catch_discover_tests(sony-protocol-tests PROPERTIES TIMEOUT 10)
