#include "sony/core/IpcClient.h"
#include "sony/core/IpcServer.h"
#include "UnixSocket.h"
#include <stdexcept>
#ifndef _WIN32
#include <poll.h>
#endif
namespace sony::core {
IpcClient::IpcClient(std::string path) : _socketPath(std::move(path)) {}
const std::string& IpcClient::socketPath() const noexcept { return _socketPath; }
#ifndef _WIN32
namespace {
using Clock = std::chrono::steady_clock;
void waitFor(int fd, short events, Clock::time_point deadline) {
    while (Clock::now() < deadline) {
        pollfd pfd{fd, events, 0};
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        int ret = ::poll(&pfd, 1, static_cast<int>(remaining + 1));
        if (ret > 0) {
            if (pfd.revents & events) return;
            throw std::runtime_error("Daemon disconnected");
        }
        if (ret < 0 && errno != EINTR) throw std::runtime_error("IPC polling failed");
    }
    throw std::runtime_error("Timed out waiting for daemon");
}
void connectTo(int fd, const std::string& path, Clock::time_point deadline) {
    unixsocket::validatePath(path);
    unixsocket::nonblocking(fd);
    auto addr = unixsocket::address(path);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        if (errno != EINPROGRESS && errno != EAGAIN) throw std::runtime_error("Daemon unavailable at " + path);
        waitFor(fd, POLLOUT, deadline);
        int error = 0; socklen_t len = sizeof(error);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error)
            throw std::runtime_error("Cannot connect to daemon");
    }
    if (!unixsocket::sameUser(fd)) throw std::runtime_error("IPC peer belongs to a different user");
}
}
#endif
bool IpcClient::isDaemonRunning(std::chrono::milliseconds timeout) {
#ifndef _WIN32
    try {
        unixsocket::Fd fd(::socket(AF_UNIX, SOCK_STREAM, 0));
        if (fd.value < 0) return false;
        connectTo(fd.value, _socketPath, Clock::now() + timeout);
        return true;
    } catch (...) { return false; }
#else
    (void)timeout; return false;
#endif
}
std::string IpcClient::request(std::string_view line, std::chrono::milliseconds timeout) {
#ifndef _WIN32
    if (line.size() + 1 > 16 * 1024 || line.find('\n') != std::string_view::npos)
        throw std::runtime_error("Invalid or oversized IPC request");
    unixsocket::Fd fd(::socket(AF_UNIX, SOCK_STREAM, 0));
    if (fd.value < 0) throw std::runtime_error("Cannot create IPC socket");
    auto deadline = Clock::now() + timeout;
    connectTo(fd.value, _socketPath, deadline);
    std::string data(line); data += '\n'; size_t sent = 0;
    while (sent < data.size()) {
        waitFor(fd.value, POLLOUT, deadline);
        auto n = unixsocket::send(fd.value, data.data() + sent, data.size() - sent);
        if (n > 0) sent += static_cast<size_t>(n);
        else if (n < 0 && errno != EINTR && errno != EAGAIN) throw std::runtime_error("Cannot send to daemon");
    }
    std::string response;
    while (true) {
        waitFor(fd.value, POLLIN, deadline);
        char buf[4096]; auto n = ::read(fd.value, buf, sizeof(buf));
        if (n == 0) throw std::runtime_error("Truncated daemon response");
        if (n < 0) { if (errno == EINTR || errno == EAGAIN) continue; throw std::runtime_error("Cannot read daemon response"); }
        response.append(buf, static_cast<size_t>(n));
        if (response.size() > 256 * 1024) throw std::runtime_error("Oversized daemon response");
        auto end = response.find('\n');
        if (end != std::string::npos) return response.substr(0, end);
    }
#else
    (void)line; (void)timeout; throw std::runtime_error("IPC not supported on this platform");
#endif
}
IpcResponse IpcClient::sendCommand(std::string_view line, std::chrono::milliseconds timeout) {
    try { return IpcProtocol::parseResponse(request(line, timeout)); }
    catch (const std::exception& ex) { return {false, ex.what(), {}}; }
}
}
