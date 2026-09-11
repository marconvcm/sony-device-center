#include "sony/core/IpcServer.h"
#include "sony/transport/SonyError.h"
#include "UnixSocket.h"
#include <chrono>
#include <cstdlib>
#include <algorithm>
#ifndef _WIN32
#include <poll.h>
#include <sys/file.h>
#endif

namespace sony::core {
std::string defaultSocketPath() {
#ifndef _WIN32
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && *xdg) return std::string(xdg) + "/sony-device-center.sock";
#ifdef __APPLE__
    return "/private/tmp/sony-device-center-" + std::to_string(::geteuid()) + "/sony-device-center.sock";
#else
    return "/tmp/sony-device-center-" + std::to_string(::geteuid()) + "/sony-device-center.sock";
#endif
#else
    return "sony-device-center";
#endif
}
IpcServer::IpcServer(std::shared_ptr<IDeviceService> service, std::string path)
    : _service(std::move(service)), _socketPath(std::move(path)) {}
IpcServer::~IpcServer() { stop(); }
void IpcServer::start() {
    if (_running) return;
#ifndef _WIN32
    try {
        unixsocket::validatePath(_socketPath, true);
        _lockFd = ::open((_socketPath + ".lock").c_str(), O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
        struct stat lockStat{};
        if (_lockFd < 0 || ::fstat(_lockFd, &lockStat) < 0 || !S_ISREG(lockStat.st_mode) ||
            lockStat.st_uid != ::geteuid() || (lockStat.st_mode & 0077))
            throw std::runtime_error("Invalid IPC lock file");
        if (::flock(_lockFd, LOCK_EX | LOCK_NB) < 0)
            throw std::runtime_error("sonyd is already running at " + _socketPath);
        // Also detect older daemons which do not acquire the lock.
        unixsocket::Fd probe(::socket(AF_UNIX, SOCK_STREAM, 0));
        unixsocket::nonblocking(probe.value);
        auto addr = unixsocket::address(_socketPath);
        if (::connect(probe.value, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 ||
            (errno != ENOENT && errno != ECONNREFUSED))
            throw std::runtime_error("IPC endpoint is already in use");
        if (::unlink(_socketPath.c_str()) < 0 && errno != ENOENT)
            throw std::runtime_error("Cannot remove stale IPC socket");
        _listenFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (_listenFd < 0) throw std::runtime_error("Cannot create IPC socket");
        unixsocket::nonblocking(_listenFd);
        if (::bind(_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("Cannot bind IPC socket");
        struct stat st{};
        if (::lstat(_socketPath.c_str(), &st) == 0) _socketInode = st.st_ino;
        if (::chmod(_socketPath.c_str(), 0600) < 0 || ::listen(_listenFd, 16) < 0)
            throw std::runtime_error("Cannot secure or listen on IPC socket");
        _running = true;
        _executor = std::thread([this] { _executeLoop(); });
        _worker = std::thread([this] { _serverLoop(); });
    } catch (...) { stop(); throw; }
#else
    throw std::runtime_error("IPC not supported on Windows; use direct mode");
#endif
}
void IpcServer::stop() noexcept {
    _running = false;
    _queueCv.notify_all();
    if (_worker.joinable()) _worker.join();
    if (_executor.joinable()) _executor.join();
#ifndef _WIN32
    if (_listenFd >= 0) { ::close(_listenFd); _listenFd = -1; }
    struct stat st{};
    if (_socketInode && ::lstat(_socketPath.c_str(), &st) == 0 && st.st_ino == _socketInode && st.st_uid == ::geteuid())
        ::unlink(_socketPath.c_str());
    _socketInode = 0;
    if (_lockFd >= 0) { ::close(_lockFd); _lockFd = -1; }
#endif
    std::lock_guard lock(_queueMutex); _jobs.clear();
}
bool IpcServer::isRunning() const noexcept { return _running; }
const std::string& IpcServer::socketPath() const noexcept { return _socketPath; }
void IpcServer::_executeLoop() {
    while (_running) {
        std::shared_ptr<Job> job;
        {
            std::unique_lock lock(_queueMutex);
            _queueCv.wait_for(lock, std::chrono::milliseconds(100), [this] { return !_running || !_jobs.empty(); });
            if (!_running) break;
            if (!_jobs.empty()) { job = _jobs.front(); _jobs.pop_front(); }
        }
        if (job && !job->cancelled) {
            try {
                auto response = _service ? IpcProtocol::executeLine(job->line, *_service)
                    : IpcProtocol::serializeResponse({false, "No service available", {}});
                if (response.size() > 256 * 1024) response = "ERR|Response too large|\n";
                job->result.set_value(std::move(response));
            } catch (const std::exception&) { job->result.set_value("ERR|Internal service error|\n"); }
        }
        bool idle;
        { std::lock_guard lock(_queueMutex); idle = _jobs.empty(); }
        if (_running && idle && _service) { try { _service->tick(); } catch (...) {} }
    }
}
void IpcServer::_serverLoop() {
#ifndef _WIN32
    using Clock = std::chrono::steady_clock;
    struct Client {
        int fd; std::string input, output; size_t sent{0};
        Clock::time_point deadline{Clock::now() + std::chrono::seconds(5)};
        std::shared_ptr<Job> job; std::future<std::string> result;
    };
    std::vector<Client> clients;
    while (_running) {
        std::vector<pollfd> fds{{_listenFd, POLLIN, 0}};
        for (auto& c : clients) fds.push_back({c.fd, static_cast<short>(c.output.empty() ? POLLIN : POLLOUT), 0});
        int ret = ::poll(fds.data(), fds.size(), 25);
        if (ret < 0 && errno != EINTR) break;
        // Process existing clients before adding new ones (indices match pollfd).
        for (size_t i = clients.size(); i-- > 0;) {
            auto& c = clients[i]; auto events = fds[i + 1].revents;
            bool close = Clock::now() >= c.deadline || (events & (POLLERR | POLLHUP | POLLNVAL));
            if (!close && c.result.valid() && c.result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                c.output = c.result.get(); c.deadline = Clock::now() + std::chrono::seconds(5);
            }
            if (!close && (events & POLLIN)) {
                char buf[1024]; auto n = ::read(c.fd, buf, sizeof(buf));
                if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) close = true;
                if (n > 0) {
                    // Exactly one outstanding request per connection. Pipelining
                    // is rejected rather than allowing unbounded queued work.
                    if (c.job) close = true;
                    else {
                        c.input.append(buf, static_cast<size_t>(n));
                        auto end = c.input.find('\n');
                        if (c.input.size() > 16 * 1024) close = true;
                        else if (end != std::string::npos) {
                            if (end + 1 != c.input.size()) close = true;
                            else {
                                c.job = std::make_shared<Job>(); c.job->line = c.input.substr(0, end);
                                c.result = c.job->result.get_future();
                                std::lock_guard lock(_queueMutex);
                                if (_jobs.size() >= 16) c.job->result.set_value("ERR|Service busy|\n");
                                else { _jobs.push_back(c.job); _queueCv.notify_one(); }
                                c.deadline = Clock::now() + std::chrono::seconds(30);
                            }
                        }
                    }
                }
            }
            if (!close && !c.output.empty()) {
                auto n = unixsocket::send(c.fd, c.output.data() + c.sent, c.output.size() - c.sent);
                if (n > 0) c.sent += static_cast<size_t>(n);
                else if (n < 0 && errno != EAGAIN && errno != EINTR) close = true;
                if (c.sent == c.output.size()) close = true;
            }
            if (close) { if (c.job) c.job->cancelled = true; ::close(c.fd); clients.erase(clients.begin() + i); }
        }
        if (fds[0].revents & POLLIN) {
            int fd = ::accept(_listenFd, nullptr, nullptr);
            if (fd >= 0) {
                unixsocket::nonblocking(fd);
                if (clients.size() >= 16 || !unixsocket::sameUser(fd)) ::close(fd);
                else clients.push_back(Client{fd});
            }
        }
    }
    for (auto& c : clients) { if (c.job) c.job->cancelled = true; ::close(c.fd); }
#endif
}
}
