#pragma once

#include "IpcProtocol.h"
#include <chrono>
#include <string>
#include <string_view>

namespace sony::core {

std::string defaultSocketPath();

class IpcClient {
public:
    explicit IpcClient(std::string socketPath = defaultSocketPath());
    ~IpcClient() = default;

    [[nodiscard]] bool isDaemonRunning(std::chrono::milliseconds timeout = std::chrono::milliseconds(200));
    IpcResponse sendCommand(std::string_view commandLine, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    std::string request(std::string_view line, std::chrono::milliseconds timeout = std::chrono::seconds(30));

    [[nodiscard]] const std::string& socketPath() const noexcept;

private:
    std::string _socketPath;
};

} // namespace sony::core
