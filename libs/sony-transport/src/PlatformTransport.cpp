#include "sony/transport/PlatformTransport.h"
#include "sony/transport/BluetoothConnectorTransport.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/SonyDeviceFilter.h"

#include <memory>
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
            for (const auto& d : connector.getConnectedDevices()) {
                result.push_back(DiscoveredDevice{
                    .name = d.name,
                    .address = DeviceAddress(d.mac),
                    .paired = d.paired, .connected = d.connected
                });
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
    return std::make_unique<SonyDeviceDiscovery>(std::make_unique<LinuxPlatformDiscovery>());
}

} // namespace sony::transport

#elif defined(SONY_HAS_WINDOWS_BLUETOOTH)
#include "WindowsBluetoothConnector.h"

namespace sony::transport {

std::unique_ptr<ITransport> createPlatformTransport() {
    return std::make_unique<BluetoothConnectorTransport>(std::make_unique<WindowsBluetoothConnector>());
}

std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery() {
    return std::make_unique<SonyDeviceDiscovery>(
        std::make_unique<BluetoothConnectorDiscovery>(std::make_unique<WindowsBluetoothConnector>()));
}

} // namespace sony::transport

#elif defined(SONY_HAS_MACOS_BLUETOOTH)
#include "MacOSBluetoothConnector.h"

namespace sony::transport {

std::unique_ptr<ITransport> createPlatformTransport() {
    return std::make_unique<BluetoothConnectorTransport>(std::make_unique<MacOSBluetoothConnector>());
}

std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery() {
    return std::make_unique<SonyDeviceDiscovery>(
        std::make_unique<BluetoothConnectorDiscovery>(std::make_unique<MacOSBluetoothConnector>()));
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
