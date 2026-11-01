#include "sony/core/SimulatedDevice.h"
#include "sony/protocol/FrameCodec.h"

#include <algorithm>

namespace sony::core {

using protocol::DataType;
using protocol::FrameCodec;
using protocol::SonyFrame;

namespace {
constexpr const char* kDefaultName = "WH-1000XM5";
constexpr const char* kDefaultAddress = "CC:98:8B:00:11:22";
}

SimulatedDeviceTransport::SimulatedDeviceTransport(bool earbuds) : _earbuds(earbuds) {}

size_t SimulatedDeviceTransport::send(std::span<const std::byte> data) {
    const size_t written = FakeTransport::send(data);
    if (data.empty()) return written;

    std::vector<uint8_t> bytes(data.size());
    std::transform(data.begin(), data.end(), bytes.begin(), [](std::byte b) {
        return static_cast<uint8_t>(b);
    });

    try {
        const auto frame = FrameCodec::decode(bytes);
        if (frame.type != DataType::DataMdr) return written;

        // Stop-and-wait ARQ: every data frame is acknowledged with the flipped
        // sequence bit before any reply is produced, exactly as hardware does.
        queueIncoming(FrameCodec::encode(SonyFrame{
            .type = DataType::Ack,
            .sequence = static_cast<uint8_t>(1 - (frame.sequence & 1)),
            .payload = {}
        }));
        handle(frame.payload);
    } catch (...) {
        // Undecodable bytes are dropped, as a real headset would drop them.
    }
    return written;
}

void SimulatedDeviceTransport::setBattery(int level, bool charging) {
    std::lock_guard lock(_stateMutex);
    _battery = static_cast<uint8_t>(std::clamp(level, 0, 100));
    _charging = charging;
    if (!isConnected()) return;
    // POWER_NTFY 25 00 <level> <charging>
    reply({0x25, 0x00, _battery, static_cast<uint8_t>(_charging ? 1 : 0)});
}

void SimulatedDeviceTransport::reply(std::vector<uint8_t> payload) {
    queueIncoming(FrameCodec::encode(SonyFrame{
        .type = DataType::DataMdr,
        .sequence = static_cast<uint8_t>(_sequence++ & 1),
        .payload = std::move(payload)
    }));
}

void SimulatedDeviceTransport::handle(const std::vector<uint8_t>& p) {
    if (p.empty()) return;
    std::lock_guard lock(_stateMutex);
    const uint8_t op = p[0];
    const uint8_t type = p.size() > 1 ? p[1] : 0;

    switch (op) {
    case 0x00: // init handshake
        reply({0x01, 0x00});
        break;

    case 0x04: // firmware version: 05 02 <len> <ascii>
        if (type == 0x02) reply({0x05, 0x02, 0x05, '2', '.', '3', '.', '1'});
        break;

    case 0x12: // codec: 13 02 <code>, 0x10 = LDAC
        if (type == 0x02) reply({0x13, 0x02, 0x10});
        break;

    case 0x22: { // battery
        const auto charging = static_cast<uint8_t>(_charging ? 1 : 0);
        if (_earbuds) {
            // 22 09 -> 23 09 <L> <Lchg> <R> <Rchg>; 22 0a -> 23 0a <case> <chg>
            if (type == 0x09) reply({0x23, 0x09, _batteryLeft, charging, _batteryRight, charging});
            if (type == 0x0a) reply({0x23, 0x0a, _batteryCase, 0x00});
        } else if (type == 0x00) {
            reply({0x23, 0x00, _battery, charging});
        }
        break;
    }

    case 0x66: // noise control query
        if (type == 0x17) reply({0x67, 0x17, 0x01, _noise[0], _noise[1], _noise[2], _noise[3]});
        break;

    case 0x68: // noise control set: 68 17 01 <effect> <settingType> <voice> <level>
        if (type == 0x17 && p.size() >= 7) {
            std::copy(p.begin() + 3, p.begin() + 7, _noise.begin());
            reply({0x69, 0x17, 0x01, _noise[0], _noise[1], _noise[2], _noise[3]});
        }
        break;

    case 0x56: { // equalizer query
        std::vector<uint8_t> out{0x57, 0x00, _eqPreset, 0x06};
        out.insert(out.end(), _eqBands.begin(), _eqBands.end());
        reply(std::move(out));
        break;
    }

    case 0x58: { // equalizer set: preset (58 00 <preset> 00) or custom (58 00 a0 06 <6 values>)
        if (p.size() >= 3) _eqPreset = p[2];
        if (p.size() >= 10 && p[3] == 0x06) std::copy(p.begin() + 4, p.begin() + 10, _eqBands.begin());
        std::vector<uint8_t> out{0x59, 0x00, _eqPreset, 0x06};
        out.insert(out.end(), _eqBands.begin(), _eqBands.end());
        reply(std::move(out));
        break;
    }

    case 0xe6: // DSEE query
        if (type == 0x01) reply({0xe7, 0x01, static_cast<uint8_t>(_dsee ? 1 : 0)});
        break;

    case 0xe8: // DSEE set
        if (type == 0x01 && p.size() >= 3) _dsee = p[2] != 0;
        break;

    case 0x24: // power set; type 03 = power off
        if (type == 0x03 && p.size() >= 3 && p[2] == 0x01) {
            // A switched-off headset is gone until the process restarts:
            // drop the link and refuse reconnection so auto-connect keeps retrying.
            setFailConnect(true, SonyErrorCode::TransportFailure);
            simulateDisconnect();
        }
        break;

    case 0x26: // auto power-off query
        if (type == 0x05) reply({0x27, 0x05, _autoPowerOff[0], _autoPowerOff[1]});
        break;

    case 0x28: // auto power-off set
        if (type == 0x05 && p.size() >= 4) _autoPowerOff = {p[2], p[3]};
        break;

    case 0xf6: // system settings query; both flags are reported inverted on the wire
        if (type == 0x0c) reply({0xf7, 0x0c, static_cast<uint8_t>(_speakToChat ? 0 : 1)});
        if (type == 0x0a) reply({0xf7, 0x0a, static_cast<uint8_t>(_adaptiveVolume ? 0 : 1)});
        break;

    case 0xf8: // system settings set
        if (p.size() >= 3) {
            if (type == 0x0c) _speakToChat = p[2] == 0;
            if (type == 0x0a) _adaptiveVolume = p[2] == 0;
        }
        break;

    default:
        break;
    }
}

SimulatedDevice createSimulatedDevice(std::string name, std::string address) {
    SimulatedDevice device;
    device.name = name.empty() ? kDefaultName : std::move(name);
    device.address = address.empty() ? kDefaultAddress : std::move(address);
    const bool earbuds = device.name.rfind("WF-", 0) == 0 || device.name.rfind("LinkBuds", 0) == 0;
    device.transport = std::make_shared<SimulatedDeviceTransport>(earbuds);
    device.discovery = std::make_shared<transport::FakeDeviceDiscovery>();
    device.discovery->addDevice(transport::DiscoveredDevice{
        .name = device.name,
        .address = transport::DeviceAddress(device.address)
    });
    return device;
}

} // namespace sony::core
