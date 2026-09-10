cmake_minimum_required(VERSION 3.16)
project(sony-device-center LANGUAGES CXX)

# Version comes from the top-level project; keep a fallback so this
# directory can still be configured on its own.
if(NOT DEFINED SonyDeviceCenter_VERSION)
    set(SonyDeviceCenter_VERSION "0.0.0-dev")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QUIET)

if(NOT Qt6_FOUND)
    message(WARNING "Qt 6 (Core, Gui, Qml, Quick, QuickControls2) not found — skipping sony-device-center target.")
    return()
endif()


add_executable(sony-device-center
    main.cpp
    src/DeviceCenterController.cpp
    include/DeviceCenterController.h
    src/I18nManager.cpp
    include/I18nManager.h
    qml.qrc
)

target_compile_features(sony-device-center PRIVATE cxx_std_20)

target_compile_definitions(sony-device-center
    PRIVATE
        SONY_DEVICE_CENTER_VERSION="${SonyDeviceCenter_VERSION}")

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

if(APPLE)
    # Ships the brand icon inside the .app and makes the previously unused
    # packaging/macos/Info.plist.in the bundle's real property list.
    set(SONY_APP_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/macos/AppIcon.icns")
    set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
    set_source_files_properties("${SONY_APP_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(sony-device-center PRIVATE "${SONY_APP_ICNS}")

    set_target_properties(sony-device-center PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME "Sony Device Center"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.sonybridge.sony-device-center"
        MACOSX_BUNDLE_BUNDLE_VERSION "${SonyDeviceCenter_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${SonyDeviceCenter_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/macos/Info.plist.in")
endif()

install(TARGETS sony-device-center
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .)

