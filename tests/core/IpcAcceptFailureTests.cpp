// Inject descriptor exhaustion at accept(), leaving a real connection queued.
// Verify bounded retries and recovery without changing the host's fd limits.
#include "sony/core/DeviceService.h"
#include "sony/core/IpcServer.h"
#include "sony/core/IpcClient.h"
#include "sony/transport/FakeTransport.h"
#include "../support/PrivateSocket.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <future>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>

namespace {
std::atomic<int> failure{0}, attempts{0};
void check(bool value) { if (!value) throw std::runtime_error("IPC accept backoff/recovery failed"); }
}
extern "C" int __real_accept(int, sockaddr*, socklen_t*);
extern "C" int __wrap_accept(int fd, sockaddr* address, socklen_t* length) {
    if (const int error = failure.load()) {
        ++attempts;
        errno = error;
        return -1;
    }
    return __real_accept(fd, address, length);
}
int main() {
    using namespace std::chrono_literals;
    for (int error : {EMFILE, ENFILE}) {
        PrivateSocket socket;
        auto service = std::make_shared<sony::core::DeviceService>(std::make_shared<sony::transport::FakeTransport>());
        sony::core::IpcServer server(service, socket.path);
        attempts = 0;
        failure = error;
        server.start();
        auto response = std::async(std::launch::async, [&] {
            return sony::core::IpcClient(socket.path).sendCommand("devices", 3s);
        });
        const auto until = std::chrono::steady_clock::now() + 1s;
        while (attempts == 0 && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
        std::this_thread::sleep_for(600ms);
        const int retries = attempts;
        failure = 0;
        check(response.get().success);
        server.stop();
        check(retries >= 1 && retries <= 4);
    }
}
