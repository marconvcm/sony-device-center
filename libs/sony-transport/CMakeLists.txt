cmake_minimum_required(VERSION 3.14)
project(sony-transport LANGUAGES CXX)

set(SONYBRIDGE_CLIENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Client" CACHE PATH "Path to Client directory")

add_library(sony-transport
    src/FakeTransport.cpp
    src/BluetoothConnectorTransport.cpp
    src/TransportBluetoothConnector.cpp
    src/Logger.cpp
)

target_compile_features(sony-transport PUBLIC cxx_std_20)

target_include_directories(sony-transport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${SONYBRIDGE_CLIENT_DIR}"
)

find_package(Threads REQUIRED)
target_link_libraries(sony-transport PUBLIC Threads::Threads)
