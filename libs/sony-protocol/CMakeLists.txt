cmake_minimum_required(VERSION 3.14)
project(sony-protocol LANGUAGES CXX)

add_library(sony-protocol
    src/FrameCodec.cpp
    src/EqualizerPresets.cpp
    src/SonyProtocolSession.cpp
    src/ProtocolV1.cpp
    src/ProtocolV2.cpp
    src/DeviceProfileRegistry.cpp
    src/CapabilityCache.cpp
    src/CapabilityDiscovery.cpp
    src/DeviceEventDispatcher.cpp
)

target_compile_features(sony-protocol PUBLIC cxx_std_20)

target_include_directories(sony-protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sony-protocol PUBLIC sony-transport)
