#include "sony/core/IpcClient.h"
#include "sony/core/IpcServer.h"

#include <cstring>
#include <string>

#ifndef _WIN32
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace sony::core {

IpcClient::IpcClient(std::string socketPath)
    : _socketPath(std::move(socketPath)) {}

const std::string& IpcClient::socketPath() const noexcept {
    return _socketPath;
}

bool IpcClient::isDaemonRunning(std::chrono::milliseconds timeout) {
#ifndef _WIN32
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, _socketPath.c_str(), sizeof(addr.sun_path) - 1);

    int res = ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    return res == 0;
#else
    (void)timeout;
    return false;
#endif
}

IpcResponse IpcClient::sendCommand(std::string_view commandLine, std::chrono::milliseconds timeout) {
#ifndef _WIN32
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return IpcResponse{
            .success = false,
            .message = "Failed to create IPC socket"
        };
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, _socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return IpcResponse{
            .success = false,
            .message = "Daemon is not running at " + _socketPath
        };
    }

    std::string toSend = std::string(commandLine) + "\n";
    ssize_t written = ::write(fd, toSend.data(), toSend.size());
    if (written < 0 || static_cast<size_t>(written) != toSend.size()) {
        ::close(fd);
        return IpcResponse{
            .success = false,
            .message = "Failed to send command to daemon"
        };
    }

    // Read response line with timeout
    std::string responseLine;
    char buf[256];
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remainingMs <= 0) break;

        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;

        int pret = ::poll(&pfd, 1, static_cast<int>(remainingMs));
        if (pret <= 0) {
            break;
        }

        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }

        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                ::close(fd);
                return IpcProtocol::parseResponse(responseLine);
            }
            if (buf[i] != '\r') {
                responseLine.push_back(buf[i]);
            }
        }
    }

    ::close(fd);
    if (!responseLine.empty()) {
        return IpcProtocol::parseResponse(responseLine);
    }

    return IpcResponse{
        .success = false,
        .message = "Timed out waiting for response from daemon"
    };
#else
    (void)commandLine;
    (void)timeout;
    return IpcResponse{
        .success = false,
        .message = "IPC not supported on this platform"
    };
#endif
}

} // namespace sony::core
