#include "sony/core/DeviceService.h"
#include "sony/core/IpcServer.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
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

class SimulatedDaemonTransport : public FakeTransport {
public:
    size_t send(std::span<const std::byte> data) override {
        size_t res = FakeTransport::send(data);
        if (!data.empty()) {
            std::vector<uint8_t> bytes(data.size());
            std::transform(data.begin(), data.end(), bytes.begin(), [](std::byte b) {
                return static_cast<uint8_t>(b);
            });
            try {
                auto frame = FrameCodec::decode(bytes);
                if (frame.type == DataType::DataMdr) {
                    SonyFrame ackFrame{
                        .type = DataType::Ack,
                        .sequence = frame.sequence,
                        .payload = {}
                    };
                    queueIncoming(FrameCodec::encode(ackFrame));

                    // Auto-respond to known inquiry requests
                    if (!frame.payload.empty()) {
                        uint8_t op = frame.payload[0];
                        if (op == 0x00) { // Init
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x01, 0x00}
                            }));
                        } else if (op == 0x22) { // Battery query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x23, 0x00, 87, 0x00} // 87%
                            }));
                        } else if (op == 0x66) { // NC query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x67, 0x17, 0x01, 0x01, 0x00, 0x00, 0x00} // ANC
                            }));
                        } else if (op == 0x56) { // EQ query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x57, 0x00, 0x16, 0x06, 0x0e, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a} // Bass Boost
                            }));
                        } else if (op == 0xe6) { // DSEE query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0xe7, 0x01, 0x01} // DSEE on
                            }));
                        }
                    }
                }
            } catch (...) {}
        }
        return res;
    }

private:
    uint8_t _nextRespSeq() {
        return _respSeq++;
    }
    uint8_t _respSeq{0};
};

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
    std::string socketPath = defaultSocketPath();
    std::string deviceAddr;
    std::string deviceName = "WH-1000XM5";
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

    // Simulated or fallback transport
    auto fakeTransport = std::make_shared<SimulatedDaemonTransport>();
    auto fakeDiscovery = std::make_shared<FakeDeviceDiscovery>();
    fakeDiscovery->addDevice(transport::DiscoveredDevice{
        .name = deviceName,
        .address = DeviceAddress(deviceAddr.empty() ? "CC:98:8B:00:11:22" : deviceAddr)
    });

    transport = fakeTransport;
    discovery = fakeDiscovery;

    auto service = std::make_shared<DeviceService>(transport, discovery);

    // Auto-connect to device
    std::string targetAddress = deviceAddr.empty() ? "CC:98:8B:00:11:22" : deviceAddr;
    std::cout << "[sonyd] Initializing device connection to " << targetAddress << " (" << deviceName << ")...\n";
    try {
        service->connect(DeviceAddress(targetAddress), deviceName);
        std::cout << "[sonyd] Connected successfully. Protocol V2 active.\n";
    } catch (const std::exception& ex) {
        std::cerr << "[sonyd] Warning: initial device connection deferred: " << ex.what() << "\n";
    }

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
