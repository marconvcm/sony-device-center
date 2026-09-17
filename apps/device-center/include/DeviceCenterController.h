#pragma once

#include <QObject>
#include <QThread>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <memory>
#include <string>

#include "BatteryHistory.h"
#include "sony/core/IDeviceService.h"
#include "sony/core/IpcClient.h"

namespace sony::devicecenter {

class DeviceBackend;

class DeviceCenterController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY stateChanged)
    Q_PROPERTY(QString codec READ codec NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap featureStatus READ featureStatus NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY stateChanged)
    Q_PROPERTY(QString deviceAddress READ deviceAddress NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY stateChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY stateChanged)
    Q_PROPERTY(bool isCharging READ isCharging NOTIFY stateChanged)
    // Earbuds: per-side and case levels, -1 when the device does not report them.
    Q_PROPERTY(int batteryLeft READ batteryLeft NOTIFY stateChanged)
    Q_PROPERTY(int batteryRight READ batteryRight NOTIFY stateChanged)
    Q_PROPERTY(int batteryCase READ batteryCase NOTIFY stateChanged)
    Q_PROPERTY(bool hasDualBattery READ hasDualBattery NOTIFY stateChanged)
    // Time-left estimate from the current discharge session. Minutes are -1
    // and the text empty while there is not enough data (or while charging).
    Q_PROPERTY(int batteryMinutesLeft READ batteryMinutesLeft NOTIFY stateChanged)
    Q_PROPERTY(QString batteryTimeLeft READ batteryTimeLeft NOTIFY stateChanged)
    Q_PROPERTY(double batteryDischargeRate READ batteryDischargeRate NOTIFY stateChanged)
    Q_PROPERTY(double batterySessionStart READ batterySessionStart NOTIFY stateChanged)
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

    Q_PROPERTY(bool iconAntialiasing READ iconAntialiasing WRITE setIconAntialiasing NOTIFY appearanceChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY appearanceChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(bool systemReducedMotion READ systemReducedMotion NOTIFY appearanceChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(bool notifyLowBattery READ notifyLowBattery WRITE setNotifyLowBattery NOTIFY notificationSettingsChanged)
    Q_PROPERTY(bool notifyConnection READ notifyConnection WRITE setNotifyConnection NOTIFY notificationSettingsChanged)
    Q_PROPERTY(bool notifyCharged READ notifyCharged WRITE setNotifyCharged NOTIFY notificationSettingsChanged)
    Q_PROPERTY(int lowBatteryThreshold READ lowBatteryThreshold WRITE setLowBatteryThreshold NOTIFY notificationSettingsChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)

public:
    // historyDir: where per-device battery logs live; defaults to
    // AppLocalDataLocation. Tests point it at a temporary directory.
    explicit DeviceCenterController(QObject* parent = nullptr, std::shared_ptr<core::IDeviceService> service = {},
                                    const QString& historyDir = {});
    ~DeviceCenterController() override;

    bool busy() const { return _busy; }
    QString lastError() const { return _lastError; }
    QString connectionState() const { return _connectionState; }
    QString codec() const { return _codec; }
    QVariantMap featureStatus() const { return _features; }
    Q_INVOKABLE void clearError() { _lastError.clear(); emit stateChanged(); }
    [[nodiscard]] QString deviceName() const;
    [[nodiscard]] QString deviceAddress() const;
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] int batteryLevel() const;
    [[nodiscard]] bool isCharging() const;
    [[nodiscard]] int batteryLeft() const;
    [[nodiscard]] int batteryRight() const;
    [[nodiscard]] int batteryCase() const;
    [[nodiscard]] bool hasDualBattery() const;
    [[nodiscard]] int batteryMinutesLeft() const;
    [[nodiscard]] QString batteryTimeLeft() const;
    [[nodiscard]] double batteryDischargeRate() const;
    [[nodiscard]] double batterySessionStart() const;
    // Logged samples at or after sinceMs (Unix ms), oldest first.
    Q_INVOKABLE QVariantList batterySamples(double sinceMs) const;
    // "5 h 20 min" in the current language; minutes only under an hour.
    Q_INVOKABLE QString formatDuration(int minutes) const;
    BatteryHistory& batteryHistory() { return *_history; }
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

    [[nodiscard]] bool autostart() const;
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString currentLanguage() const;
    [[nodiscard]] QVariantList availableLanguages() const;

    Q_INVOKABLE void setAnc(bool enabled);
    Q_INVOKABLE void setAmbient(int level, bool focusOnVoice = false);
    Q_INVOKABLE void setNoiseControlOff();
    Q_INVOKABLE void setEqualizerPreset(int preset);
    Q_INVOKABLE void setEqualizerCustom(int clearBass, const QVariantList& bands);
    Q_INVOKABLE void setDsee(bool enabled);
    Q_INVOKABLE void setSpeakToChat(bool enabled);
    Q_INVOKABLE void setAdaptiveVolume(bool enabled);
    Q_INVOKABLE void setAutoPowerOff(int index);
    Q_INVOKABLE void powerOff();

    bool iconAntialiasing() const { return _iconAntialiasing; }
    Q_INVOKABLE void setIconAntialiasing(bool enabled);
    QString themeMode() const { return _themeMode; }
    bool animationsEnabled() const { return _animationsEnabled; }
    bool systemReducedMotion() const { return _systemReducedMotion; }
    Q_INVOKABLE void setThemeMode(const QString& mode);
    Q_INVOKABLE void setAnimationsEnabled(bool enabled);
    Q_INVOKABLE void setAutostart(bool enable);
    [[nodiscard]] bool minimizeToTray() const;
    Q_INVOKABLE void setMinimizeToTray(bool enable);
    [[nodiscard]] bool notifyLowBattery() const { return _notifyLowBattery; }
    [[nodiscard]] bool notifyConnection() const { return _notifyConnection; }
    [[nodiscard]] bool notifyCharged() const { return _notifyCharged; }
    [[nodiscard]] int lowBatteryThreshold() const { return _lowBatteryThreshold; }
    Q_INVOKABLE void setNotifyLowBattery(bool enable);
    Q_INVOKABLE void setNotifyConnection(bool enable);
    Q_INVOKABLE void setNotifyCharged(bool enable);
    Q_INVOKABLE void setLowBatteryThreshold(int percent);
    Q_INVOKABLE void setLanguage(const QString& langCode);
    Q_INVOKABLE QString t(const QString& key) const;
    Q_INVOKABLE void openUrl(const QString& url);

    Q_INVOKABLE void connectDevice(const QString& address, const QString& name = "");
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void refreshDiscoveredDevices();

signals:
    void stateChanged();
    void batteryHistoryChanged();
    void capabilitiesChanged();
    void pairedDevicesChanged();
    void autostartChanged();
    void minimizeToTrayChanged();
    void notificationSettingsChanged();
    void languageChanged();
    void appearanceChanged();

private:
    void _applySnapshot(const QByteArray& data);
    void _send(const QString& method, const QJsonObject& params = {});
    QList<QPair<QString, QJsonObject>> _pending;
    std::unique_ptr<BatteryHistory> _history;
    QThread _worker;
    DeviceBackend* _backend{nullptr};
    quint64 _generation{0};
    bool _busy{true};
    QString _lastError, _connectionState{"searching"}, _codec{"Unknown"};
    QVariantMap _features;
    QJsonObject _capabilities;

    // Cached UI state
    QString _deviceName{""};
    QString _deviceAddress{""};
    bool _connected{false};
    int _batteryLevel{-1};
    int _batteryLeft{-1}, _batteryRight{-1}, _batteryCase{-1};
    bool _isCharging{false};
    QString _noiseControlMode{"unknown"};
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
    QString _themeMode{"dark"};
    bool _animationsEnabled{true};
    bool _iconAntialiasing{true};
    bool _systemReducedMotion{false};
    QString _currentLanguage{"en"};
    bool _minimizeToTray{true};
    bool _notifyLowBattery{true}, _notifyConnection{true}, _notifyCharged{false};
    int _lowBatteryThreshold{20};

};

} // namespace sony::devicecenter
