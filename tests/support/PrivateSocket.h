#pragma once
#include <filesystem>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif
struct PrivateSocket {
    std::filesystem::path directory;
    std::string path;
    PrivateSocket() {
        directory = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
            ("sony-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
#ifndef _WIN32
        ::chmod(directory.c_str(), 0700);
#endif
        path = (directory / "ipc.sock").string();
    }
    ~PrivateSocket() { std::error_code error; std::filesystem::remove_all(directory, error); }
};
