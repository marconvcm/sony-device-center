cmake_minimum_required(VERSION 3.14)
project(sonyd LANGUAGES CXX)

add_executable(sonyd
    main.cpp
)

target_compile_features(sonyd PRIVATE cxx_std_20)
target_link_libraries(sonyd PRIVATE sony-core sony-protocol sony-transport Threads::Threads)

install(TARGETS sonyd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
