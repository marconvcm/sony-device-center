#include "DeviceCenterController.h"
#include "sony/core/DeviceService.h"
#include "sony/core/IpcProtocol.h"
#include "sony/transport/FakeTransport.h"
#include "sony/protocol/FrameCodec.h"

#include <QVariantMap>

namespace sony::devicecenter {

namespace {

class LocalSimulatedTransport : public transport::FakeTransport {
public:
    size_t send(std::span<const std::byte> data) override {
        size_t res = transport::FakeTransport::send(data);
        if (!data.empty()) {
            std::vector<uint8_t> bytes(data.size());
            std::transform(data.begin(), data.end(), bytes.begin(), [](std::byte b) {
                return static_cast<uint8_t>(b);
            });
            try {
                auto frame = protocol::FrameCodec::decode(bytes);
                if (frame.type == protocol::DataType::DataMdr) {
                    protocol::SonyFrame ackFrame{
                        .type = protocol::DataType::Ack,
                        .sequence = frame.sequence,
                        .payload = {}
                    };
                    queueIncoming(protocol::FrameCodec::encode(ackFrame));

                    if (!frame.payload.empty()) {
                        uint8_t op = frame.payload[0];
                        if (op == 0x00) {
                            queueIncoming(protocol::FrameCodec::encode(protocol::SonyFrame{
                                .type = protocol::DataType::DataMdr,
                                .sequence = _seq++,
                                .payload = {0x01, 0x00}
                            }));
                        } else if (op == 0x22) {
                            queueIncoming(protocol::FrameCodec::encode(protocol::SonyFrame{
                                .type = protocol::DataType::DataMdr,
                                .sequence = _seq++,
                                .payload = {0x23, 0x00, 87, 0x00}
                            }));
                        } else if (op == 0x66) {
                            queueIncoming(protocol::FrameCodec::encode(protocol::SonyFrame{
                                .type = protocol::DataType::DataMdr,
                                .sequence = _seq++,
                                .payload = {0x67, 0x17, 0x01, 0x01, 0x00, 0x00, 0x00}
                            }));
                        } else if (op == 0x56) {
                            queueIncoming(protocol::FrameCodec::encode(protocol::SonyFrame{
                                .type = protocol::DataType::DataMdr,
                                .sequence = _seq++,
                                .payload = {0x57, 0x00, 0x16, 0x06, 0x0e, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a}
                            }));
                        } else if (op == 0xe6) {
                            queueIncoming(protocol::FrameCodec::encode(protocol::SonyFrame{
                                .type = protocol::DataType::DataMdr,
                                .sequence = _seq++,
                                .payload = {0xe7, 0x01, 0x01}
                            }));
                        }
                    }
                }
            } catch (...) {}
        }
        return res;
    }
private:
    uint8_t _seq{0};
};

} // namespace

DeviceCenterController::DeviceCenterController(QObject* parent)
    : QObject(parent), _ipcClient(std::make_unique<core::IpcClient>()) {
    _initService();
    refreshDiscoveredDevices();
}

DeviceCenterController::~DeviceCenterController() = default;

void DeviceCenterController::_initService() {
    if (_ipcClient && _ipcClient->isDaemonRunning()) {
        _usingIpc = true;
        auto infoResp = _ipcClient->sendCommand("info");
        if (infoResp.success && !infoResp.message.empty()) {
            _deviceName = QString::fromStdString(infoResp.message);
        }
        _syncState();
        return;
    }

    _usingIpc = false;
    auto transport = std::make_shared<LocalSimulatedTransport>();
    auto discovery = std::make_shared<transport::FakeDeviceDiscovery>();
    discovery->addDevice(transport::DiscoveredDevice{
        .name = "WH-1000XM5",
        .address = transport::DeviceAddress("CC:98:8B:00:11:22")
    });
    discovery->addDevice(transport::DiscoveredDevice{
        .name = "WF-1000XM4",
        .address = transport::DeviceAddress("CC:98:8B:22:33:44")
    });

    _directService = std::make_shared<core::DeviceService>(transport, discovery);
    _directService->connect(transport::DeviceAddress("CC:98:8B:00:11:22"), "WH-1000XM5");
    _syncState();
}

void DeviceCenterController::_syncState() {
    if (_usingIpc) {
        auto batResp = _ipcClient->sendCommand("battery");
        if (batResp.success) {
            // "Battery: 87%"
            int pct = 85;
            auto s = batResp.data;
            auto idx = s.find('%');
            if (idx != std::string::npos && idx >= 2) {
                try {
                    pct = std::stoi(s.substr(idx - 2, 2));
                } catch (...) {}
            }
            _batteryLevel = pct;
        }
    } else if (_directService && _directService->activeDevice()) {
        auto snap = _directService->snapshot();
        _batteryLevel = snap->battery.main.value_or(87);
        _isCharging = snap->battery.charging;
        if (snap->noiseControl.mode == protocol::NoiseControlMode::NoiseCancelling) {
            _noiseControlMode = "cancelling";
        } else if (snap->noiseControl.mode == protocol::NoiseControlMode::Ambient) {
            _noiseControlMode = "ambient";
        } else {
            _noiseControlMode = "off";
        }
        _ambientLevel = snap->noiseControl.ambientLevel > 0 ? snap->noiseControl.ambientLevel : 10;
        _focusOnVoice = snap->noiseControl.focusOnVoice;
        _equalizerPreset = snap->equalizer.preset;
        _clearBass = snap->equalizer.clearBass;
        _dsee = snap->dsee;
        _speakToChat = snap->speakToChat;
        _adaptiveVolume = snap->adaptiveVolume;
        _autoPowerOff = snap->autoPowerOff;
    }
    emit stateChanged();
    emit capabilitiesChanged();
}

QString DeviceCenterController::deviceName() const { return _deviceName; }
QString DeviceCenterController::deviceAddress() const { return _deviceAddress; }
bool DeviceCenterController::isConnected() const { return _connected; }
int DeviceCenterController::batteryLevel() const { return _batteryLevel; }
bool DeviceCenterController::isCharging() const { return _isCharging; }
QString DeviceCenterController::noiseControlMode() const { return _noiseControlMode; }
int DeviceCenterController::ambientLevel() const { return _ambientLevel; }
bool DeviceCenterController::focusOnVoice() const { return _focusOnVoice; }
int DeviceCenterController::equalizerPreset() const { return _equalizerPreset; }
QString DeviceCenterController::equalizerPresetName() const { return _equalizerPresetName; }
int DeviceCenterController::clearBass() const { return _clearBass; }
QVariantList DeviceCenterController::equalizerBands() const { return _equalizerBands; }
bool DeviceCenterController::dsee() const { return _dsee; }
bool DeviceCenterController::speakToChat() const { return _speakToChat; }
bool DeviceCenterController::adaptiveVolume() const { return _adaptiveVolume; }
int DeviceCenterController::autoPowerOff() const { return _autoPowerOff; }

QString DeviceCenterController::heroImagePath() const {
    QString lower = _deviceName.toLower();
    if (lower.contains("xm5")) {
        return "resources/devices/wh-1000xm5.png";
    } else if (lower.contains("xm4")) {
        return "resources/devices/wh-1000xm4.png";
    } else if (lower.contains("xm3")) {
        return "resources/devices/wh-1000xm3.png";
    } else if (lower.contains("ch720")) {
        return "resources/devices/wh-ch720n.png";
    } else if (lower.contains("linkbuds")) {
        return "resources/devices/linkbuds-s.png";
    }
    return "resources/devices/wh-1000xm5.png";
}

bool DeviceCenterController::hasAnc() const { return true; }
bool DeviceCenterController::hasAmbient() const { return true; }
bool DeviceCenterController::hasEqualizer() const { return true; }
bool DeviceCenterController::hasClearBass() const { return true; }
bool DeviceCenterController::hasDsee() const { return true; }
bool DeviceCenterController::hasSpeakToChat() const { return true; }
bool DeviceCenterController::hasAdaptiveVolume() const { return true; }

QVariantList DeviceCenterController::pairedDevices() const { return _pairedDevices; }

void DeviceCenterController::setAnc(bool enabled) {
    _noiseControlMode = enabled ? "cancelling" : "off";
    if (_usingIpc) {
        _ipcClient->sendCommand(enabled ? "anc on" : "anc off");
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setAnc(enabled);
    }
    emit stateChanged();
}

void DeviceCenterController::setAmbient(int level, bool focusOnVoice) {
    _noiseControlMode = "ambient";
    _ambientLevel = std::clamp(level, 1, 20);
    _focusOnVoice = focusOnVoice;
    if (_usingIpc) {
        _ipcClient->sendCommand("ambient " + std::to_string(_ambientLevel));
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setAmbient(_ambientLevel, _focusOnVoice);
    }
    emit stateChanged();
}

void DeviceCenterController::setNoiseControlOff() {
    _noiseControlMode = "off";
    if (_usingIpc) {
        _ipcClient->sendCommand("anc off");
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setAnc(false);
    }
    emit stateChanged();
}

void DeviceCenterController::setEqualizerPreset(int preset) {
    _equalizerPreset = preset;
    std::string presetStr = "off";
    switch (preset) {
        case 0x10: presetStr = "bright"; _equalizerPresetName = "Bright"; break;
        case 0x11: presetStr = "excited"; _equalizerPresetName = "Excited"; break;
        case 0x12: presetStr = "mellow"; _equalizerPresetName = "Mellow"; break;
        case 0x13: presetStr = "relaxed"; _equalizerPresetName = "Relaxed"; break;
        case 0x14: presetStr = "vocal"; _equalizerPresetName = "Vocal"; break;
        case 0x15: presetStr = "treble-boost"; _equalizerPresetName = "Treble Boost"; break;
        case 0x16: presetStr = "bass-boost"; _equalizerPresetName = "Bass Boost"; break;
        case 0x17: presetStr = "speech"; _equalizerPresetName = "Speech"; break;
        case 0xa0: presetStr = "manual"; _equalizerPresetName = "Custom"; break;
        default:   presetStr = "off"; _equalizerPresetName = "Off"; break;
    }
    if (_usingIpc) {
        _ipcClient->sendCommand("eq preset " + presetStr);
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setEqualizerPreset(preset);
    }
    emit stateChanged();
}

void DeviceCenterController::setEqualizerCustom(int clearBass, const QVariantList& bands) {
    _equalizerPreset = 0xa0;
    _equalizerPresetName = "Custom";
    _clearBass = std::clamp(clearBass, -10, 10);
    _equalizerBands = bands;
    std::array<int, 5> rawBands = {0, 0, 0, 0, 0};
    for (int i = 0; i < 5 && i < bands.size(); ++i) {
        rawBands[i] = bands[i].toInt();
    }
    if (_usingIpc) {
        std::string cmd = "eq custom " + std::to_string(_clearBass);
        for (int b : rawBands) cmd += " " + std::to_string(b);
        _ipcClient->sendCommand(cmd);
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setEqualizerCustom(_clearBass, rawBands);
    }
    emit stateChanged();
}

void DeviceCenterController::setDsee(bool enabled) {
    _dsee = enabled;
    if (_usingIpc) {
        _ipcClient->sendCommand(enabled ? "dsee on" : "dsee off");
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setDsee(enabled);
    }
    emit stateChanged();
}

void DeviceCenterController::setSpeakToChat(bool enabled) {
    _speakToChat = enabled;
    if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setSpeakToChat(enabled);
    }
    emit stateChanged();
}

void DeviceCenterController::setAdaptiveVolume(bool enabled) {
    _adaptiveVolume = enabled;
    if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setAdaptiveVolume(enabled);
    }
    emit stateChanged();
}

void DeviceCenterController::setAutoPowerOff(int index) {
    _autoPowerOff = index;
    if (_usingIpc) {
        _ipcClient->sendCommand("apo " + std::to_string(index));
    } else if (_directService && _directService->activeDevice()) {
        _directService->activeDevice()->setAutoPowerOff(index);
    }
    emit stateChanged();
}

void DeviceCenterController::connectDevice(const QString& address, const QString& name) {
    _deviceAddress = address;
    if (!name.isEmpty()) _deviceName = name;
    _connected = true;
    if (_directService) {
        _directService->connect(transport::DeviceAddress(address.toStdString()), _deviceName.toStdString());
    }
    _syncState();
}

void DeviceCenterController::disconnectDevice() {
    _connected = false;
    if (_directService) {
        _directService->disconnect();
    }
    emit stateChanged();
}

void DeviceCenterController::refreshDiscoveredDevices() {
    _pairedDevices.clear();
    QVariantMap dev1;
    dev1["name"] = "WH-1000XM5";
    dev1["address"] = "CC:98:8B:00:11:22";
    dev1["connected"] = true;
    _pairedDevices.append(dev1);

    QVariantMap dev2;
    dev2["name"] = "WF-1000XM4";
    dev2["address"] = "CC:98:8B:22:33:44";
    dev2["connected"] = false;
    _pairedDevices.append(dev2);

    emit pairedDevicesChanged();
}

} // namespace sony::devicecenter
