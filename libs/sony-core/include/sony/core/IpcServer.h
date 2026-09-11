#pragma once

#include "IDeviceService.h"
#include "IpcProtocol.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <future>
#include <functional>

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
    void _executeLoop();
    struct Job { std::string line; std::promise<std::string> result; std::atomic<bool> cancelled{false}; };

    std::shared_ptr<IDeviceService> _service;
    std::string _socketPath;
    std::atomic<bool> _running{false};
    int _listenFd{-1};
    std::thread _worker;
    std::thread _executor;
    std::mutex _queueMutex;
    std::condition_variable _queueCv;
    std::deque<std::shared_ptr<Job>> _jobs;
    int _lockFd{-1};
    unsigned long long _socketInode{0};
};

} // namespace sony::core
