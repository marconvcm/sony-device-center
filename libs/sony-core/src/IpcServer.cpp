#include "sony/core/IpcServer.h"
#include "sony/transport/Logger.h"
#include "sony/transport/SonyError.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef _WIN32
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace sony::core {

std::string defaultSocketPath() {
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && *xdg) {
        return std::string(xdg) + "/sony-device-center.sock";
    }
    return "/tmp/sony-device-center.sock";
}

IpcServer::IpcServer(std::shared_ptr<IDeviceService> service, std::string socketPath)
    : _service(std::move(service)), _socketPath(std::move(socketPath)) {}

IpcServer::~IpcServer() {
    stop();
}

void IpcServer::start() {
    if (_running.load()) {
        return;
    }

#ifndef _WIN32
    _listenFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (_listenFd < 0) {
        throw SonyException(SonyErrorCode::TransportFailure, "Failed to create IPC domain socket");
    }

    ::unlink(_socketPath.c_str());

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, _socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(_listenFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(_listenFd);
        _listenFd = -1;
        throw SonyException(SonyErrorCode::TransportFailure, "Failed to bind IPC domain socket: " + _socketPath);
    }

    if (::listen(_listenFd, 16) < 0) {
        ::close(_listenFd);
        _listenFd = -1;
        throw SonyException(SonyErrorCode::TransportFailure, "Failed to listen on IPC domain socket");
    }

    _running.store(true);
    _worker = std::thread([this] { _serverLoop(); });
#else
    _running.store(true);
#endif
}

void IpcServer::stop() noexcept {
    if (!_running.exchange(false)) {
        return;
    }

#ifndef _WIN32
    if (_worker.joinable()) {
        _worker.join();
    }
    if (_listenFd >= 0) {
        ::close(_listenFd);
        _listenFd = -1;
    }
    ::unlink(_socketPath.c_str());
#endif
}

bool IpcServer::isRunning() const noexcept {
    return _running.load();
}

const std::string& IpcServer::socketPath() const noexcept {
    return _socketPath;
}

void IpcServer::_serverLoop() {
#ifndef _WIN32
    while (_running.load()) {
        struct pollfd pfd{};
        pfd.fd = _listenFd;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) {
            continue; // Poll timeout, loop to check _running
        }

        if (pfd.revents & POLLIN) {
            int clientFd = ::accept(_listenFd, nullptr, nullptr);
            if (clientFd >= 0) {
                _handleClient(clientFd);
                ::close(clientFd);
            }
        }
    }
#endif
}

void IpcServer::_handleClient(int clientFd) {
#ifndef _WIN32
    std::string line;
    char buf[512];

    while (_running.load()) {
        struct pollfd pfd{};
        pfd.fd = clientFd;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 500);
        if (ret <= 0) {
            break;
        }

        ssize_t n = ::read(clientFd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }

        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                if (!line.empty()) {
                    auto cmd = IpcProtocol::parseCommand(line);
                    IpcResponse resp;
                    if (_service) {
                        resp = IpcProtocol::execute(cmd, *_service);
                    } else {
                        resp.success = false;
                        resp.message = "No service available";
                    }
                    auto serialized = IpcProtocol::serializeResponse(resp);
                    ::write(clientFd, serialized.data(), serialized.size());
                    line.clear();
                }
            } else if (buf[i] != '\r') {
                line.push_back(buf[i]);
            }
        }
    }
#else
    (void)clientFd;
#endif
}

} // namespace sony::core
