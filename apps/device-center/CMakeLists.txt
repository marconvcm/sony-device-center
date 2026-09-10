cmake_minimum_required(VERSION 3.16)
project(sony-device-center LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

add_executable(sony-device-center
    main.cpp
    src/DeviceCenterController.cpp
    include/DeviceCenterController.h
    qml.qrc
)

target_compile_features(sony-device-center PRIVATE cxx_std_20)

target_include_directories(sony-device-center
    PRIVATE
        include
)

target_link_libraries(sony-device-center
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        sony-core
        sony-protocol
        sony-transport
        Threads::Threads
)

add_custom_command(TARGET sony-device-center POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Client/resources/devices"
        "$<TARGET_FILE_DIR:sony-device-center>/resources/devices"
    COMMENT "Copying device hero assets for device-center")

install(TARGETS sony-device-center RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

