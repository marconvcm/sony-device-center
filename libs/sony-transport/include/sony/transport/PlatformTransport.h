#pragma once

#include "sony/transport/ITransport.h"
#include "sony/transport/IDeviceDiscovery.h"
#include <memory>

namespace sony::transport {

/// Creates the native Bluetooth transport for the current operating system
/// (Linux BlueZ/RFCOMM, Windows WinSock RFCOMM, macOS IOBluetooth),
/// or falls back to FakeTransport if platform Bluetooth is unavailable.
// Experimental XM6 BLE control is opt-in; other platforms ignore this option.
std::unique_ptr<ITransport> createPlatformTransport(bool enableBleControl = false);

/// Creates the native Bluetooth device discovery service for the current OS.
/// It lists paired and connected devices, and SonyDeviceDiscovery keeps only
/// the devices with a Sony address prefix or a Sony name.
std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery();

} // namespace sony::transport
