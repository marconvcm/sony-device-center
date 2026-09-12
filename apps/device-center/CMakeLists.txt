cmake_minimum_required(VERSION 3.16)
project(sony-device-center LANGUAGES CXX)

# Version comes from the top-level project; keep a fallback so this
# directory can still be configured on its own.
if(NOT DEFINED SonyDeviceCenter_VERSION)
    set(SonyDeviceCenter_VERSION "0.0.0-dev")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(SONY_REQUIRE_QT
    "Fail configuration when Qt 6 is missing instead of skipping the desktop app" OFF)

find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QUIET)

if(NOT Qt6_FOUND)
    if(SONY_REQUIRE_QT)
        message(FATAL_ERROR
            "Qt 6 (Core, Gui, Qml, Quick, QuickControls2) not found, and "
            "SONY_REQUIRE_QT is ON. Release packages must contain the desktop app.")
    endif()
    message(WARNING "Qt 6 (Core, Gui, Qml, Quick, QuickControls2) not found — skipping sony-device-center target.")
    return()
endif()


add_executable(sony-device-center
    main.cpp
    src/DeviceCenterController.cpp
    src/DeviceBackend.cpp
    include/DeviceBackend.h
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

# The hero images are compiled in through qml.qrc. On macOS this loose copy
# would land inside the bundle at Contents/MacOS/resources, 21 MB the app
# never reads, so it is skipped there.
if(NOT APPLE)
    add_custom_command(TARGET sony-device-center POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../../Client/resources/devices"
            "$<TARGET_FILE_DIR:sony-device-center>/resources/devices"
        COMMENT "Copying device hero assets for device-center")
endif()

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

    # `cmake --build build --target dmg` produces the drag-to-Applications
    # disk image: macdeployqt, sonyd/sonyctl folded into the bundle, signing,
    # dmgbuild. See packaging/macos/build-dmg.sh for the knobs.
    add_custom_target(dmg
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/macos/build-dmg.sh" "${CMAKE_BINARY_DIR}"
        DEPENDS sony-device-center sonyd sonyctl
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Building the macOS disk image"
        VERBATIM)
endif()

install(TARGETS sony-device-center
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .)

if(WIN32)
    # A console-subsystem binary pops a terminal window next to the GUI on
    # every launch. The app has no console UI, so build it as a GUI-subsystem
    # executable; Qt6::Core supplies the WinMain shim.
    set_target_properties(sony-device-center PROPERTIES WIN32_EXECUTABLE ON)

    # A bare .exe cannot start without the Qt DLLs and QML module tree beside
    # it, so let windeployqt populate the install prefix at install time.
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin}")

    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                # The MSVC runtime comes from InstallRequiredSystemLibraries
                # in the top-level CMakeLists, not from windeployqt, which
                # would only drop a vc_redist.exe here.
                --release --no-translations --no-compiler-runtime
                --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/qml\"
                \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/sony-device-center.exe\")
        ")
    else()
        message(WARNING "windeployqt not found - the Windows package will not include the Qt runtime.")
    endif()
endif()


if(BUILD_TESTING)
    find_package(Qt6 COMPONENTS Test QUIET)
    if(TARGET Qt6::Test)
        add_executable(sony-ui-tests
            ../../tests/ui/DeviceControllerTests.cpp
            src/DeviceCenterController.cpp include/DeviceCenterController.h
            src/DeviceBackend.cpp include/DeviceBackend.h
            src/I18nManager.cpp include/I18nManager.h)
        target_include_directories(sony-ui-tests PRIVATE include)
        target_compile_definitions(sony-ui-tests PRIVATE SONY_DEVICE_CENTER_VERSION="${SonyDeviceCenter_VERSION}")
        target_link_libraries(sony-ui-tests PRIVATE Qt6::Test Qt6::Gui sony-core)
        add_test(NAME "device-controller-worker-integration" COMMAND sony-ui-tests)
        set_tests_properties("device-controller-worker-integration" PROPERTIES TIMEOUT 25)
    endif()
endif()

if(BUILD_TESTING AND UNIX AND NOT APPLE)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND)
        add_test(NAME simulated-app-smoke
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/smoke/simulated.py
                $<TARGET_FILE:sonyd> $<TARGET_FILE:sonyctl> $<TARGET_FILE:sony-device-center>)
        set_tests_properties(simulated-app-smoke PROPERTIES TIMEOUT 50)
    endif()
endif()
