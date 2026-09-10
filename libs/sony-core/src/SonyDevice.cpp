#include "sony/core/SonyDevice.h"
#include "sony/protocol/DeviceProfileRegistry.h"
#include "sony/protocol/ProtocolV1.h"
#include "sony/protocol/ProtocolV2.h"
#include "sony/transport/Logger.h"

namespace sony::core {

SonyDevice::SonyDevice(
    std::shared_ptr<transport::ITransport> transport,
    SonyProtocolVersion version)
    : _transport(std::move(transport)), _version(version) {
    auto defaultProf = protocol::DeviceProfileRegistry::getProfile(protocol::SonyModel::Unknown);
    _profile = defaultProf.value_or(protocol::DeviceProfile{});
    _capabilities = _profile.capabilities;
    _name = std::string(to_string(_profile.model));
}

SonyDevice::~SonyDevice() {
    disconnect();
}

void SonyDevice::connect(const transport::DeviceAddress& address, std::string_view deviceName) {
    if (!_transport) {
        throw SonyException(SonyErrorCode::TransportFailure, "No transport configured");
    }

    if (isConnected()) {
        disconnect();
    }

    if (!deviceName.empty()) {
        _name = std::string(deviceName);
        _profile = protocol::DeviceProfileRegistry::getProfileForDevice(deviceName);
        _capabilities = _profile.capabilities;
        _version = _profile.protocol;
    } else {
        _name = std::string(to_string(_profile.model));
    }

    _transport->connect(address);
    _setupSession();

    if (_protocol) {
        try {
            _protocol->initDevice();
        } catch (const SonyException& ex) {
            Logger::warn(LogCategory::Device, "Device init handshake failed: " + std::string(ex.what()));
        }
    }

    refreshAll();

    _dispatcher.dispatch(protocol::ConnectionChanged{
        .connected = true,
        .deviceAddress = address.str()
    });
}

void SonyDevice::disconnect() noexcept {
    if (_session) {
        _session->disconnect();
    }
    if (_transport && _transport->isConnected()) {
        _transport->disconnect();
    }
    _dispatcher.dispatch(protocol::ConnectionChanged{
        .connected = false,
        .deviceAddress = ""
    });
}

bool SonyDevice::isConnected() const noexcept {
    return _transport && _transport->isConnected();
}

SonyProtocolVersion SonyDevice::protocolVersion() const noexcept {
    return _version;
}

const std::string& SonyDevice::name() const noexcept {
    return _name;
}

const protocol::DeviceProfile& SonyDevice::profile() const noexcept {
    return _profile;
}

const protocol::DeviceCapabilities& SonyDevice::capabilities() const noexcept {
    return _capabilities;
}

protocol::DeviceState SonyDevice::state() const {
    std::lock_guard lock(_stateMutex);
    return _state;
}

protocol::DeviceStateSnapshot SonyDevice::snapshot() const {
    std::lock_guard lock(_stateMutex);
    return std::make_shared<const protocol::DeviceState>(_state);
}

protocol::DeviceEventDispatcher& SonyDevice::events() noexcept {
    return _dispatcher;
}

void SonyDevice::_setupSession() {
    _session = std::make_unique<protocol::SonyProtocolSession>(_transport.get());
    if (_version == SonyProtocolVersion::V1) {
        _protocol = std::make_unique<protocol::ProtocolV1>(*_session);
    } else {
        _protocol = std::make_unique<protocol::ProtocolV2>(*_session);
    }

    _session->onNotification([this](const protocol::SonyFrame& frame) {
        _onNotification(frame);
    });

    _session->start();
}

void SonyDevice::_onNotification(const protocol::SonyFrame& frame) {
    std::lock_guard lock(_stateMutex);
    _dispatcher.parseNotification(frame, _state);
}

void SonyDevice::refreshAll() {
    refreshBattery();
    refreshNoiseControl();
    refreshEqualizer();
    refreshDsee();
}

void SonyDevice::refreshBattery() {
    if (!_protocol) return;
    try {
        auto bat = _protocol->getBattery();
        {
            std::lock_guard lock(_stateMutex);
            _state.battery = bat;
        }
        _dispatcher.dispatch(protocol::BatteryChanged{bat});
        _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
    } catch (const SonyException& ex) {
        Logger::debug(LogCategory::Device, "refreshBattery error: " + std::string(ex.what()));
    }
}

void SonyDevice::refreshNoiseControl() {
    if (!_protocol) return;
    try {
        auto nc = _protocol->getNoiseControl();
        {
            std::lock_guard lock(_stateMutex);
            _state.noiseControl = nc;
        }
        _dispatcher.dispatch(protocol::NoiseControlChanged{nc});
        _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
    } catch (const SonyException& ex) {
        Logger::debug(LogCategory::Device, "refreshNoiseControl error: " + std::string(ex.what()));
    }
}

void SonyDevice::refreshEqualizer() {
    if (!_protocol) return;
    try {
        auto eq = _protocol->getEqualizer();
        {
            std::lock_guard lock(_stateMutex);
            _state.equalizer = eq;
        }
        _dispatcher.dispatch(protocol::EqualizerChanged{eq});
        _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
    } catch (const SonyException& ex) {
        Logger::debug(LogCategory::Device, "refreshEqualizer error: " + std::string(ex.what()));
    }
}

void SonyDevice::refreshDsee() {
    if (!_protocol) return;
    try {
        bool dsee = _protocol->getDsee();
        {
            std::lock_guard lock(_stateMutex);
            _state.dsee = dsee;
        }
        _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
    } catch (const SonyException& ex) {
        Logger::debug(LogCategory::Device, "refreshDsee error: " + std::string(ex.what()));
    }
}

void SonyDevice::setNoiseControl(const protocol::NoiseControlState& nc) {
    if (!_protocol) return;
    _protocol->setNoiseControl(nc);
    {
        std::lock_guard lock(_stateMutex);
        _state.noiseControl = nc;
    }
    _dispatcher.dispatch(protocol::NoiseControlChanged{nc});
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

void SonyDevice::setAnc(bool enabled) {
    protocol::NoiseControlState nc;
    nc.mode = enabled ? protocol::NoiseControlMode::NoiseCancelling : protocol::NoiseControlMode::Off;
    nc.ambientLevel = 0;
    nc.focusOnVoice = false;
    setNoiseControl(nc);
}

void SonyDevice::setAmbient(int level, bool focusOnVoice) {
    protocol::NoiseControlState nc;
    nc.mode = protocol::NoiseControlMode::Ambient;
    nc.ambientLevel = level;
    nc.focusOnVoice = focusOnVoice;
    setNoiseControl(nc);
}

void SonyDevice::setEqualizerPreset(int preset) {
    if (!_protocol) return;
    _protocol->setEqualizerPreset(preset);
    {
        std::lock_guard lock(_stateMutex);
        _state.equalizer.preset = preset;
    }
    _dispatcher.dispatch(protocol::EqualizerChanged{_state.equalizer});
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

void SonyDevice::setEqualizerCustom(int clearBass, const std::array<int, 5>& bands) {
    if (!_protocol) return;
    _protocol->setEqualizerCustom(clearBass, bands);
    {
        std::lock_guard lock(_stateMutex);
        _state.equalizer.preset = 0xa0; // MANUAL
        _state.equalizer.clearBass = clearBass;
        _state.equalizer.bands = bands;
    }
    _dispatcher.dispatch(protocol::EqualizerChanged{_state.equalizer});
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

void SonyDevice::setDsee(bool enabled) {
    if (!_protocol) return;
    _protocol->setDsee(enabled);
    {
        std::lock_guard lock(_stateMutex);
        _state.dsee = enabled;
    }
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

void SonyDevice::setAutoPowerOff(int index) {
    if (!_protocol) return;
    _protocol->setAutoPowerOff(index);
    {
        std::lock_guard lock(_stateMutex);
        _state.autoPowerOff = index;
    }
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

void SonyDevice::setSpeakToChat(bool enabled) {
    if (!_protocol) return;
    _protocol->setSpeakToChat(enabled);
    {
        std::lock_guard lock(_stateMutex);
        _state.speakToChat = enabled;
    }
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

void SonyDevice::setAdaptiveVolume(bool enabled) {
    if (!_protocol) return;
    _protocol->setAdaptiveVolume(enabled);
    {
        std::lock_guard lock(_stateMutex);
        _state.adaptiveVolume = enabled;
    }
    _dispatcher.dispatch(protocol::DeviceStateChanged{snapshot()});
}

} // namespace sony::core
