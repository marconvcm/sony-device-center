#pragma once

#include "sony/protocol/DeviceProfile.h"
#include "sony/protocol/DeviceState.h"
#include "sony/protocol/DeviceEventDispatcher.h"
#include "sony/protocol/IProtocol.h"
#include "sony/protocol/SonyProtocolSession.h"
#include "sony/transport/ITransport.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace sony::core {

using protocol::SonyProtocolVersion;

class SonyDevice {
public:
    explicit SonyDevice(
        std::shared_ptr<transport::ITransport> transport,
        SonyProtocolVersion version = SonyProtocolVersion::V1);
    ~SonyDevice();

    SonyDevice(const SonyDevice&) = delete;
    SonyDevice& operator=(const SonyDevice&) = delete;

    void connect(const transport::DeviceAddress& address, std::string_view deviceName = "");
    void disconnect() noexcept;

    [[nodiscard]] bool isConnected() const noexcept;
    [[nodiscard]] SonyProtocolVersion protocolVersion() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;

    [[nodiscard]] const protocol::DeviceProfile& profile() const noexcept;
    [[nodiscard]] const protocol::DeviceCapabilities& capabilities() const noexcept;

    [[nodiscard]] protocol::DeviceState state() const;
    [[nodiscard]] protocol::DeviceStateSnapshot snapshot() const;

    [[nodiscard]] protocol::DeviceEventDispatcher& events() noexcept;

    // Refresh state from device
    void refreshAll();
    void refreshBattery();
    void refreshNoiseControl();
    void refreshEqualizer();
    void refreshDsee();
    void refreshSettingsStep();

    // Control operations
    void setNoiseControl(const protocol::NoiseControlState& nc);
    void setAnc(bool enabled);
    void setAmbient(int level, bool focusOnVoice = false);
    void setEqualizerPreset(int preset);
    void setEqualizerCustom(int clearBass, const std::array<int, 5>& bands);
    void setDsee(bool enabled);
    void setAutoPowerOff(int index);
    void setSpeakToChat(bool enabled);
    void setAdaptiveVolume(bool enabled);

private:
    unsigned _refreshStep{0};
    void _markSuccess(const std::string& feature);
    void _markError(const std::string& feature, const SonyException& ex);
    void _setupSession();
    void _onNotification(const protocol::SonyFrame& frame);

    std::shared_ptr<transport::ITransport> _transport;
    SonyProtocolVersion _version;
    std::string _name;

    std::unique_ptr<protocol::SonyProtocolSession> _session;
    std::unique_ptr<protocol::IProtocol> _protocol;

    protocol::DeviceProfile _profile;
    protocol::DeviceCapabilities _capabilities;

    mutable std::recursive_mutex _stateMutex;
    protocol::DeviceState _state;
    protocol::DeviceEventDispatcher _dispatcher;
};

} // namespace sony::core
