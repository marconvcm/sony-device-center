#pragma once

#include "BluetoothConnectorTransport.h"

namespace sony::transport {

class LinuxBluetoothTransport : public BluetoothConnectorTransport {
public:
    using BluetoothConnectorTransport::BluetoothConnectorTransport;
};

using LinuxDeviceDiscovery = BluetoothConnectorDiscovery;

} // namespace sony::transport
