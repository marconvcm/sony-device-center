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
add_subdirectory(apps/device-center)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Client/CMakeLists.txt")
    add_subdirectory(Client)
endif()

include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/sony-device-center.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/sony-device-center.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES packaging/linux/sony-device-center.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
    install(FILES packaging/linux/com.github.sonybridge.sony-device-center.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES packaging/linux/sonyd.service
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user OPTIONAL)
endif()

# Packaging via CPack
set(CPACK_PACKAGE_NAME "sony-device-center")
set(CPACK_PACKAGE_VENDOR "SonyBridge")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform Sony audio device companion SDK, daemon, CLI, and GUI")
set(CPACK_PACKAGE_VERSION_MAJOR 2)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_CONTACT "https://github.com/Plutoberth/SonyHeadphonesClient")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SonyBridge Team")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libbluetooth3, libdbus-1-3")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
endif()

include(CPack)

