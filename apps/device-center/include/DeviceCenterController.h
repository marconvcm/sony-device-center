#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>
#include <string>

#include "sony/core/IDeviceService.h"
#include "sony/core/IpcClient.h"

namespace sony::devicecenter {

class DeviceCenterController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString deviceName READ deviceName NOTIFY stateChanged)
    Q_PROPERTY(QString deviceAddress READ deviceAddress NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY stateChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY stateChanged)
    Q_PROPERTY(bool isCharging READ isCharging NOTIFY stateChanged)
    Q_PROPERTY(QString noiseControlMode READ noiseControlMode NOTIFY stateChanged)
    Q_PROPERTY(int ambientLevel READ ambientLevel NOTIFY stateChanged)
    Q_PROPERTY(bool focusOnVoice READ focusOnVoice NOTIFY stateChanged)
    Q_PROPERTY(int equalizerPreset READ equalizerPreset NOTIFY stateChanged)
    Q_PROPERTY(QString equalizerPresetName READ equalizerPresetName NOTIFY stateChanged)
    Q_PROPERTY(int clearBass READ clearBass NOTIFY stateChanged)
    Q_PROPERTY(QVariantList equalizerBands READ equalizerBands NOTIFY stateChanged)
    Q_PROPERTY(bool dsee READ dsee NOTIFY stateChanged)
    Q_PROPERTY(bool speakToChat READ speakToChat NOTIFY stateChanged)
    Q_PROPERTY(bool adaptiveVolume READ adaptiveVolume NOTIFY stateChanged)
    Q_PROPERTY(int autoPowerOff READ autoPowerOff NOTIFY stateChanged)
    Q_PROPERTY(QString heroImagePath READ heroImagePath NOTIFY stateChanged)

    Q_PROPERTY(bool hasAnc READ hasAnc NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasAmbient READ hasAmbient NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasEqualizer READ hasEqualizer NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasClearBass READ hasClearBass NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasDsee READ hasDsee NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasSpeakToChat READ hasSpeakToChat NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasAdaptiveVolume READ hasAdaptiveVolume NOTIFY capabilitiesChanged)

    Q_PROPERTY(QVariantList pairedDevices READ pairedDevices NOTIFY pairedDevicesChanged)

public:
    explicit DeviceCenterController(QObject* parent = nullptr);
    ~DeviceCenterController() override;

    [[nodiscard]] QString deviceName() const;
    [[nodiscard]] QString deviceAddress() const;
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] int batteryLevel() const;
    [[nodiscard]] bool isCharging() const;
    [[nodiscard]] QString noiseControlMode() const;
    [[nodiscard]] int ambientLevel() const;
    [[nodiscard]] bool focusOnVoice() const;
    [[nodiscard]] int equalizerPreset() const;
    [[nodiscard]] QString equalizerPresetName() const;
    [[nodiscard]] int clearBass() const;
    [[nodiscard]] QVariantList equalizerBands() const;
    [[nodiscard]] bool dsee() const;
    [[nodiscard]] bool speakToChat() const;
    [[nodiscard]] bool adaptiveVolume() const;
    [[nodiscard]] int autoPowerOff() const;
    [[nodiscard]] QString heroImagePath() const;

    [[nodiscard]] bool hasAnc() const;
    [[nodiscard]] bool hasAmbient() const;
    [[nodiscard]] bool hasEqualizer() const;
    [[nodiscard]] bool hasClearBass() const;
    [[nodiscard]] bool hasDsee() const;
    [[nodiscard]] bool hasSpeakToChat() const;
    [[nodiscard]] bool hasAdaptiveVolume() const;

    [[nodiscard]] QVariantList pairedDevices() const;

    Q_INVOKABLE void setAnc(bool enabled);
    Q_INVOKABLE void setAmbient(int level, bool focusOnVoice = false);
    Q_INVOKABLE void setNoiseControlOff();
    Q_INVOKABLE void setEqualizerPreset(int preset);
    Q_INVOKABLE void setEqualizerCustom(int clearBass, const QVariantList& bands);
    Q_INVOKABLE void setDsee(bool enabled);
    Q_INVOKABLE void setSpeakToChat(bool enabled);
    Q_INVOKABLE void setAdaptiveVolume(bool enabled);
    Q_INVOKABLE void setAutoPowerOff(int index);

    Q_INVOKABLE void connectDevice(const QString& address, const QString& name = "");
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void refreshDiscoveredDevices();

signals:
    void stateChanged();
    void capabilitiesChanged();
    void pairedDevicesChanged();

private:
    void _initService();
    void _syncState();

    std::unique_ptr<core::IpcClient> _ipcClient;
    std::shared_ptr<core::IDeviceService> _directService;
    bool _usingIpc{false};

    // Cached UI state
    QString _deviceName{""};
    QString _deviceAddress{""};
    bool _connected{false};
    int _batteryLevel{0};
    bool _isCharging{false};
    QString _noiseControlMode{"off"};
    int _ambientLevel{10};
    bool _focusOnVoice{false};
    int _equalizerPreset{0x00};
    QString _equalizerPresetName{"Off"};
    int _clearBass{0};
    QVariantList _equalizerBands{0, 0, 0, 0, 0};
    bool _dsee{false};
    bool _speakToChat{false};
    bool _adaptiveVolume{false};
    int _autoPowerOff{0};
    QVariantList _pairedDevices;

};

} // namespace sony::devicecenter
