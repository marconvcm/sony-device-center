cmake_minimum_required(VERSION 3.14)
project(sony-core LANGUAGES CXX)

add_library(sony-core
    src/SonyDevice.cpp
    src/DeviceService.cpp
    src/IpcProtocol.cpp
)

target_compile_features(sony-core PUBLIC cxx_std_20)

target_include_directories(sony-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(sony-core
    PUBLIC
        sony-protocol
        sony-transport
        Threads::Threads
)
