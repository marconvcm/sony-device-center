#pragma once

#include "sony/transport/ITransport.h"
#include "sony/transport/IDeviceDiscovery.h"
#include <memory>

namespace sony::transport {

/// Creates the native Bluetooth transport for the current operating system
/// (Linux BlueZ/RFCOMM, Windows WinSock RFCOMM, macOS IOBluetooth),
/// or falls back to FakeTransport if platform Bluetooth is unavailable.
std::unique_ptr<ITransport> createPlatformTransport();

/// Creates the native Bluetooth device discovery service for the current OS,
/// discovering paired and connected Sony audio devices.
std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery();

} // namespace sony::transport
