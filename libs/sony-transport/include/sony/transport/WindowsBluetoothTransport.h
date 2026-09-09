#pragma once

#include "BluetoothConnectorTransport.h"

namespace sony::transport {

class WindowsBluetoothTransport : public BluetoothConnectorTransport {
public:
    using BluetoothConnectorTransport::BluetoothConnectorTransport;
};

using WindowsDeviceDiscovery = BluetoothConnectorDiscovery;

} // namespace sony::transport
