#pragma once

#include "BluetoothConnectorTransport.h"

namespace sony::transport {

class MacOSBluetoothTransport : public BluetoothConnectorTransport {
public:
    using BluetoothConnectorTransport::BluetoothConnectorTransport;
};

using MacOSDeviceDiscovery = BluetoothConnectorDiscovery;

} // namespace sony::transport
