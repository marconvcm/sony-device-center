#include "sony/core/DeviceService.h"
#include "sony/core/IpcServer.h"
#include "sony/core/SimulatedDevice.h"
#include "sony/transport/PlatformTransport.h"
#include "sony/transport/Logger.h"


#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

std::atomic<bool> g_shutdown{false};

void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown.store(true);
    }
}

void printHelp() {
    std::cout << "Usage: sonyd [options]\n\n"
              << "Options:\n"
              << "  -s, --socket <path>   Path to IPC Unix domain socket\n"
              << "  -d, --device <addr>   Bluetooth address of Sony device to connect to\n"
              << "  --simulated, --fake   Run in simulation mode (ideal for headless or testing)\n"
              << "  -v, --verbose         Enable diagnostic protocol logging\n"
              << "  -h, --help            Show this help text\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::string socketPath = defaultSocketPath();
    std::string deviceAddr;
    std::string deviceName;  // resolved from discovery; never guessed
    bool simulated = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if ((arg == "-s" || arg == "--socket") && i + 1 < argc) {
            socketPath = argv[++i];
        } else if ((arg == "-d" || arg == "--device") && i + 1 < argc) {
            deviceAddr = argv[++i];
        } else if (arg == "--simulated" || arg == "--fake") {
            simulated = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
    }

    if (verbose) {
        Logger::setLogLevel(LogLevel::Debug);
        Logger::setDeveloperMode(true);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "===============================================\n"
              << "  sonyd — Sony Audio Device Background Daemon  \n"
              << "===============================================\n";

    std::shared_ptr<ITransport> transport;
    std::shared_ptr<IDeviceDiscovery> discovery;

    if (!simulated) {
        transport = transport::createPlatformTransport();
        discovery = transport::createPlatformDiscovery();
    }

    std::string targetAddress = deviceAddr;

    if (simulated) {
        auto simulatedDevice = createSimulatedDevice(deviceName, targetAddress);
        transport = simulatedDevice.transport;
        discovery = simulatedDevice.discovery;
        targetAddress = simulatedDevice.address;
        deviceName = simulatedDevice.name;
    }

    auto service = std::make_shared<DeviceService>(transport, discovery);

    service->startAutoConnect(targetAddress);

    // Start IPC Server
    auto server = std::make_unique<IpcServer>(service, socketPath);
    try {
        server->start();
        std::cout << "[sonyd] IPC socket listening at: " << socketPath << "\n"
                  << "[sonyd] Ready for sonyctl and UI connections.\n"
                  << "[sonyd] Press Ctrl+C to terminate daemon.\n";
    } catch (const std::exception& ex) {
        std::cerr << "[sonyd] Failed to start IPC server: " << ex.what() << "\n";
        return 1;
    }

    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n[sonyd] Shutdown signal received. Closing IPC server...\n";
    server->stop();
    service->disconnect();
    std::cout << "[sonyd] Daemon terminated cleanly.\n";

    return 0;
}
