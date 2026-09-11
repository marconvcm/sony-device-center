#include "sony/transport/PlatformTransport.h"
#include "sony/transport/BluetoothConnectorTransport.h"
#include "sony/transport/FakeTransport.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#if defined(SONY_HAS_LINUX_BLUETOOTH)
#include "LinuxBluetoothConnector.h"
#include "DBusHelper.h"

namespace sony::transport {

namespace {

class LinuxPlatformDiscovery : public IDeviceDiscovery {
public:
    std::vector<DiscoveredDevice> discover() override {
        std::vector<DiscoveredDevice> result;
        try {
            LinuxBluetoothConnector connector;
            auto devs = connector.getConnectedDevices();
            for (const auto& d : devs) {
                std::string upper = d.name;
                std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
                    return static_cast<char>(std::toupper(c));
                });

                bool isSony = (upper.find("WH-") != std::string::npos ||
                               upper.find("WF-") != std::string::npos ||
                               upper.find("WI-") != std::string::npos ||
                               upper.find("MDR-") != std::string::npos ||
                               upper.find("LINKBUDS") != std::string::npos ||
                               upper.find("ULT WEAR") != std::string::npos ||
                               upper.find("SONY") != std::string::npos);

                if (isSony) {
                    result.push_back(DiscoveredDevice{
                        .name = d.name,
                        .address = DeviceAddress(d.mac),
                        .paired = d.paired, .connected = d.connected
                    });
                }
            }
        } catch (...) {}
        return result;
    }
};

} // namespace

std::unique_ptr<ITransport> createPlatformTransport() {
    return std::make_unique<BluetoothConnectorTransport>(std::make_unique<LinuxBluetoothConnector>());
}

std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery() {
    return std::make_unique<LinuxPlatformDiscovery>();
}

} // namespace sony::transport

#elif defined(SONY_HAS_WINDOWS_BLUETOOTH)
#include "WindowsBluetoothConnector.h"

namespace sony::transport {

std::unique_ptr<ITransport> createPlatformTransport() {
    return std::make_unique<BluetoothConnectorTransport>(std::make_unique<WindowsBluetoothConnector>());
}

std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery() {
    return std::make_unique<BluetoothConnectorDiscovery>(std::make_unique<WindowsBluetoothConnector>());
}

} // namespace sony::transport

#elif defined(SONY_HAS_MACOS_BLUETOOTH)
#include "MacOSBluetoothConnector.h"

namespace sony::transport {

std::unique_ptr<ITransport> createPlatformTransport() {
    return std::make_unique<BluetoothConnectorTransport>(std::make_unique<MacOSBluetoothConnector>());
}

std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery() {
    return std::make_unique<BluetoothConnectorDiscovery>(std::make_unique<MacOSBluetoothConnector>());
}

} // namespace sony::transport

#else

namespace sony::transport {

std::unique_ptr<ITransport> createPlatformTransport() {
    return std::make_unique<FakeTransport>();
}

std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery() {
    return std::make_unique<FakeDeviceDiscovery>();
}

} // namespace sony::transport

#endif
