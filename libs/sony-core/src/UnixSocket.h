#pragma once
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <cstring>
#include <cerrno>

namespace sony::core::unixsocket {
inline void validatePath(const std::string& path, bool create = false) {
    if (path.empty() || path[0] != '/' || path.size() >= sizeof(sockaddr_un::sun_path))
        throw std::runtime_error("IPC socket requires an absolute path within the Unix socket length limit");
    const auto parent = std::filesystem::path(path).parent_path();
    if (create && ::mkdir(parent.c_str(), 0700) < 0 && errno != EEXIST)
        throw std::runtime_error("Cannot create private IPC directory");
    // Reject symlinks anywhere in the parent path. Shared ancestors such as /tmp
    // are allowed; the immediate parent must be private and owned by this user.
    std::filesystem::path current;
    for (const auto& part : parent) {
        current /= part;
        struct stat st{};
        if (::lstat(current.c_str(), &st) < 0 || !S_ISDIR(st.st_mode))
            throw std::runtime_error("IPC directory is missing or contains a symlink");
    }
    struct stat st{};
    if (::lstat(parent.c_str(), &st) < 0 || st.st_uid != ::geteuid() || (st.st_mode & 0077))
        throw std::runtime_error("IPC directory must be user-owned with mode 0700");
    if (::lstat(path.c_str(), &st) == 0 && (!S_ISSOCK(st.st_mode) || st.st_uid != ::geteuid()))
        throw std::runtime_error("Refusing foreign or non-socket IPC endpoint");
}
inline sockaddr_un address(const std::string& path) {
    sockaddr_un addr{}; addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    return addr;
}
inline bool sameUser(int fd) {
#ifdef __linux__
    struct ucred cred{}; socklen_t len = sizeof(cred);
    return ::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0 && cred.uid == ::geteuid();
#else
    uid_t uid; gid_t gid;
    return ::getpeereid(fd, &uid, &gid) == 0 && uid == ::geteuid();
#endif
}
inline void nonblocking(int fd) {
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
    int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
}
inline ssize_t send(int fd, const char* data, size_t size) {
#ifdef MSG_NOSIGNAL
    return ::send(fd, data, size, MSG_NOSIGNAL);
#else
    return ::send(fd, data, size, 0);
#endif
}
struct Fd {
    int value{-1};
    explicit Fd(int fd = -1) : value(fd) {}
    ~Fd() { if (value >= 0) ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
}
#endif
