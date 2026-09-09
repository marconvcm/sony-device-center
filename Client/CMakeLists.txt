cmake_minimum_required(VERSION 3.5)
project(SonyHeadphonesClient)

set(CMAKE_CXX_STANDARD 20)

include(CTest)
if(BUILD_TESTING AND NOT TARGET sony-protocol-tests)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../tests" "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()

find_package(Threads REQUIRED)

if(NOT TARGET sony-transport)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../libs/sony-transport" "${CMAKE_CURRENT_BINARY_DIR}/libs/sony-transport")
endif()

add_executable(SonyHeadphonesClient)

add_definitions(-D__HEADPHONES_APP_VERSION__="0.4.0-beta2")

target_sources(SonyHeadphonesClient
    PRIVATE BluetoothWrapper.cpp
    ByteMagic.cpp
    CommandSerializer.cpp
    TimedMessageQueue.cpp
    CrossPlatformGUI.cpp "Headphones.cpp")

target_include_directories (SonyHeadphonesClient
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if (WIN32 OR LINUX)
    set (DEAR_IMGUI TRUE)
endif()

if (DEAR_IMGUI)
    target_sources(SonyHeadphonesClient
        PRIVATE CrossPlatformGUI.cpp
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        CascadiaCodeFont.cpp
    )

    target_include_directories (SonyHeadphonesClient
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    )
endif ()

if (LINUX)
	target_sources(SonyHeadphonesClient
		PRIVATE linux/DBusHelper.cpp
		linux/LinuxBluetoothConnector.cpp
		linux/LinuxGUI.cpp
        linux/main.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
    )

    list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/linux")

    set (OpenGL_GL_PREFERENCE GLVND)
	
    find_package(DBus REQUIRED)

    find_package(glfw3 REQUIRED)

    find_package(OpenGL REQUIRED)

    if ((NOT DBUS_FOUND) OR (NOT glfw3_FOUND) OR (NOT OPENGL_FOUND))
        message ( FATAL_ERROR 
        "Didn't find one of the packages. For Debian based systems, use:"
        "sudo apt install libbluetooth-dev libglfw3-dev libdbus-1-dev" )
    endif ()

    target_include_directories(SonyHeadphonesClient
        PRIVATE ${DBUS_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
    )

    target_link_libraries(SonyHeadphonesClient
        PRIVATE ${DBUS_LIBRARIES}
        glfw
        bluetooth
        OpenGL::GL
    )

endif ()

if (WIN32)
    ADD_DEFINITIONS(-DUNICODE)

    target_sources(SonyHeadphonesClient
        PRIVATE windows/WindowsGUI.cpp
        windows/main.cpp
        windows/WindowsBluetoothConnector.cpp
        imgui/backends/imgui_impl_dx11.cpp
        imgui/backends/imgui_impl_win32.cpp
    )

    target_link_libraries(SonyHeadphonesClient
        PRIVATE d3d11
    )
endif ()

target_link_libraries(SonyHeadphonesClient
    PRIVATE Threads::Threads
    sony-transport
)

if (WIN32 OR LINUX)
    add_custom_command(TARGET SonyHeadphonesClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/devices"
            "$<TARGET_FILE_DIR:SonyHeadphonesClient>/resources/devices"
        COMMENT "Copying device hero images next to the binary")
endif()
