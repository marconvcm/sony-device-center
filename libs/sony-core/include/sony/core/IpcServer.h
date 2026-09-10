#pragma once

#include "IDeviceService.h"
#include "IpcProtocol.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace sony::core {

std::string defaultSocketPath();

class IpcServer {
public:
    explicit IpcServer(std::shared_ptr<IDeviceService> service, std::string socketPath = defaultSocketPath());
    ~IpcServer();

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void start();
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] const std::string& socketPath() const noexcept;

private:
    void _serverLoop();
    void _handleClient(int clientFd);

    std::shared_ptr<IDeviceService> _service;
    std::string _socketPath;
    std::atomic<bool> _running{false};
    int _listenFd{-1};
    std::thread _worker;
};

} // namespace sony::core
