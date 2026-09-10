cmake_minimum_required(VERSION 3.14)
project(sonyctl LANGUAGES CXX)

add_executable(sonyctl
    main.cpp
)

target_compile_features(sonyctl PRIVATE cxx_std_20)
target_link_libraries(sonyctl PRIVATE sony-core sony-protocol sony-transport Threads::Threads)
