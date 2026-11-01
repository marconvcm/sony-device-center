// Hardware-free native socket regression tests. Linked with --wrap for Bluetooth
// setup only; send/recv/close exercise real Unix sockets and the default SIGPIPE.
#include "linux/LinuxBluetoothConnector.h"
#include "linux/DBusHelper.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

namespace {
int controlFd = -1, peerFd = -1;
bool failConnect = false, failOptions = false;
uint8_t channel = 1;
void check(bool ok) { if (!ok) throw std::runtime_error("Linux socket regression failed"); }
void expectFailure(LinuxBluetoothConnector& connector) {
    bool failed = false;
    try { connector.connect("11:22:33:44:55:66"); }
    catch (const RecoverableException&) { failed = true; }
    check(failed && !connector.isConnected());
    check(fcntl(controlFd, F_GETFD) == -1 && errno == EBADF);
    close(peerFd);
}
}
extern "C" int __wrap_socket(int, int type, int) {
    check(type & SOCK_CLOEXEC);
    int pair[2];
    check(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) == 0);
    controlFd = pair[0]; peerFd = pair[1];
    return controlFd;
}
extern "C" int __wrap_connect(int, const sockaddr*, socklen_t) {
    if (failConnect) { errno = ECONNREFUSED; return -1; }
    return 0;
}
extern "C" int __real_setsockopt(int, int, int, const void*, socklen_t);
extern "C" int __wrap_setsockopt(int fd, int level, int name, const void* value, socklen_t size) {
    if (failOptions) { errno = EINVAL; return -1; }
    // RFCOMM security option has no Unix-socket equivalent.
    if (level == SOL_SOCKET) return __real_setsockopt(fd, level, name, value, size);
    return 0;
}
uint8_t sdp_getServiceChannel(const char*, uint8_t*) { return channel; }
DBusConnection* dbus_open_system_bus() { return nullptr; }
std::vector<std::string> dbus_list_adapters(DBusConnection*) { return {}; }
std::string dbus_get_property(DBusConnection*, const char*, const char*) { return {}; }
bool dbus_get_property_bool(DBusConnection*, const char*, const char*) { return false; }

int main() {
    LinuxBluetoothConnector connector;
    failConnect = true;
    expectFailure(connector);
    failConnect = false;
    channel = 0;
    expectFailure(connector);
    channel = 1;
    failOptions = true;
    expectFailure(connector);
    failOptions = false;

    connector.connect("11:22:33:44:55:66");
    int oldPeer = peerFd;
    connector.connect("11:22:33:44:55:66");
    char closedByte;
    check(read(oldPeer, &closedByte, 1) == 0);
    close(oldPeer);
    check(connector.isConnected());
    char byte = 'x';
    check(connector.send(&byte, 1) == 1);
    check(read(peerFd, &byte, 1) == 1);
    check(write(peerFd, &byte, 1) == 1);
    check(connector.recv(&byte, 1) == 1);
    // Timeout must preserve the control connection.
    timeval deadline{0, 1000};
    check(setsockopt(controlFd, SOL_SOCKET, SO_RCVTIMEO, &deadline, sizeof(deadline)) == 0);
    bool timeout = false;
    try { connector.recv(&byte, 1); }
    catch (const RecoverableException& ex) { timeout = !ex.shouldDisconnect; }
    check(timeout && connector.isConnected());
    close(peerFd);
    bool brokenPipe = false;
    try { connector.send(&byte, 1); }
    catch (const RecoverableException& ex) { brokenPipe = ex.shouldDisconnect; }
    check(brokenPipe); // process must survive with default SIGPIPE handling
    connector.disconnect();
    int replacement = open("/dev/null", O_RDONLY);
    check(replacement >= 0);
    connector.disconnect();
    check(fcntl(replacement, F_GETFD) >= 0);
    close(replacement);
    int destructorFd;
    {
        LinuxBluetoothConnector scoped;
        scoped.connect("11:22:33:44:55:66");
        destructorFd = controlFd;
    }
    check(fcntl(destructorFd, F_GETFD) == -1 && errno == EBADF);
    close(peerFd);
}
