#include "DeviceCenterController.h"
#include "DeviceBackend.h"
#include <QJsonDocument>
#include <QJsonArray>
#include "I18nManager.h"
#include "sony/core/DeviceService.h"
#include "sony/core/IpcProtocol.h"
#include "sony/protocol/DeviceProfileRegistry.h"
#include "sony/protocol/EqualizerPresets.h"
#include "sony/transport/PlatformTransport.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>
#include <sstream>

namespace sony::devicecenter {

DeviceCenterController::DeviceCenterController(QObject* parent, std::shared_ptr<core::IDeviceService> service,
                                               const QString& historyDir)
    : QObject(parent) {
    _history = std::make_unique<BatteryHistory>(
        historyDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : historyDir);
    connect(_history.get(), &BatteryHistory::changed, this, &DeviceCenterController::batteryHistoryChanged);
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    _themeMode = settings.value("themeMode", "light").toString();
    if (_themeMode != "dark" && _themeMode != "light" && _themeMode != "system") _themeMode = "light";
    _iconAntialiasing = settings.value("iconAntialiasing", true).toBool();
    _animationsEnabled = settings.value("animationsEnabled", true).toBool();
#ifdef Q_OS_WIN
    // Qt 6.10 exposes color scheme, but not the Windows animation preference.
    const auto updateMotion = [this] {
        BOOL enabled = TRUE;
        if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
            const bool reduced = !enabled;
            if (_systemReducedMotion != reduced) {
                _systemReducedMotion = reduced;
                emit appearanceChanged();
            }
        }
    };
    updateMotion();
    auto* motionTimer = new QTimer(this);
    connect(motionTimer, &QTimer::timeout, this, updateMotion);
    motionTimer->start(2000);
#endif
    _currentLanguage = settings.value("language", "en").toString();
    _minimizeToTray = settings.value("minimizeToTray", true).toBool();
    _notifyLowBattery = settings.value("notifyLowBattery", true).toBool();
    _notifyConnection = settings.value("notifyConnection", true).toBool();
    _notifyCharged = settings.value("notifyCharged", false).toBool();
    _lowBatteryThreshold = settings.value("lowBatteryThreshold", 20).toInt();
    _ambientLevel = std::clamp(settings.value("ambientLevel", 10).toInt(), 1, 20);
    _backend = new DeviceBackend(std::move(service));
    _backend->moveToThread(&_worker);
    connect(&_worker, &QThread::started, _backend, &DeviceBackend::start);
    connect(&_worker, &QThread::finished, _backend, &QObject::deleteLater);
    connect(_backend, &DeviceBackend::snapshotReady, this, [this](const QByteArray& data, quint64 generation) {
        if (generation == _generation) _applySnapshot(data);
    });
    connect(_backend, &DeviceBackend::devicesReady, this, [this](const QByteArray& data) {
        _pairedDevices = QJsonDocument::fromJson(data).array().toVariantList();
        emit pairedDevicesChanged();
    });
    connect(_backend, &DeviceBackend::error, this, [this](const QString& error, quint64 generation) {
        if (generation == _generation) { _lastError = error; emit stateChanged(); }
    });
    connect(_backend, &DeviceBackend::completed, this, [this](quint64 generation) {
        if (generation != _generation) return;
        _busy = false;
        if (_pending.isEmpty()) { emit stateChanged(); return; }
        // Flush the value the user most recently asked for while we were busy.
        auto next = _pending.takeFirst();
        _send(next.first, next.second);
    });
    _worker.start();
}
DeviceCenterController::~DeviceCenterController() {
    QMetaObject::invokeMethod(_backend, &DeviceBackend::shutdown, Qt::BlockingQueuedConnection);
    _worker.quit(); _worker.wait();
}
void DeviceCenterController::_send(const QString& method, const QJsonObject& params) {
    if (_busy) {
        // One in-flight command at a time. A slider drag produces many values
        // per second; only the newest one per method is worth sending, so
        // replace an earlier queued request for the same method in place and
        // keep the order otherwise.
        for (auto& entry : _pending) {
            if (entry.first == method) { entry.second = params; return; }
        }
        _pending.append({method, params});
        return;
    }
    _busy = true; _lastError.clear(); const auto generation = ++_generation;
    emit stateChanged();
    const auto bytes = QJsonDocument(QJsonObject{{"method",method},{"params",params}}).toJson(QJsonDocument::Compact);
    QMetaObject::invokeMethod(_backend, [backend = _backend, bytes, generation] { backend->command(bytes, generation); }, Qt::QueuedConnection);
}
void DeviceCenterController::_applySnapshot(const QByteArray& data) {
    const auto s = QJsonDocument::fromJson(data).object();
    _connected = s.value("connected").toBool();
    _connectionState = s.value("connectionState").toString("disconnected");
    if (s.contains("name")) _deviceName = s.value("name").toString();
    if (s.contains("address")) _deviceAddress = s.value("address").toString();
    if (!s.contains("features")) {
        _batteryLevel = _batteryLeft = _batteryRight = _batteryCase = -1; _noiseControlMode = "unknown";
        _isCharging = false;
        _history->observe(QDateTime::currentMSecsSinceEpoch(), false, -1, false);
        emit stateChanged(); return;
    }
    _features = s.value("features").toObject().toVariantMap();
    _capabilities = s.value("capabilities").toObject();
    auto valid = [&s](const char* feature) {
        return s.value("features").toObject().value(feature).toObject().value("availability").toString() == "valid";
    };
    const auto battery = s.value("battery").toObject();
    _batteryLevel = _connected && valid("battery") ? battery.value("main").toInt(-1) : -1;
    _isCharging = _connected && battery.value("charging").toBool();
    // Optional fields arrive as null; toInt(-1) keeps "not reported" distinct from 0%.
    const bool batteryValid = _connected && valid("battery");
    _batteryLeft = batteryValid ? battery.value("left").toInt(-1) : -1;
    _batteryRight = batteryValid ? battery.value("right").toInt(-1) : -1;
    _batteryCase = batteryValid ? battery.value("case").toInt(-1) : -1;
    const auto nc = s.value("noiseControl").toObject();
    _noiseControlMode = _connected && valid("noiseControl") ? nc.value("mode").toString() : "unknown";
    _focusOnVoice = nc.value("focusOnVoice").toBool();
    // Outside ambient mode the protocol reports level 0. Keep the last real
    // level instead, so switching back to ambient restores it rather than
    // dropping to 1; it is persisted because the device does not keep it.
    if (const int reported = nc.value("ambientLevel").toInt(); reported > 0 && reported != _ambientLevel) {
        _ambientLevel = reported;
        QSettings("SonyBridge", "SonyDeviceCenter").setValue("ambientLevel", reported);
    }
    const auto eq = s.value("equalizer").toObject();
    _equalizerPreset = valid("equalizer") ? eq.value("preset").toInt() : -1;
    _equalizerPresetName = valid("equalizer") ? eq.value("presetName").toString() : "Unknown";
    _clearBass = eq.value("clearBass").toInt(); _equalizerBands = eq.value("bands").toArray().toVariantList();
    _dsee = s.value("dsee").toBool(); _speakToChat = s.value("speakToChat").toBool();
    _adaptiveVolume = s.value("adaptiveVolume").toBool(); _autoPowerOff = s.value("autoPowerOff").toInt();
    _codec = _connected && valid("codec") ? s.value("codec").toString("Unknown") : "Unknown";
    // The log follows the selected device and only writes on actual changes.
    // Early snapshots carry no address yet; the log keeps its device rather
    // than closing and reopening (which would restart the discharge session).
    if (!_deviceAddress.isEmpty() && _history->device() != _deviceAddress) _history->setDevice(_deviceAddress);
    _history->observe(QDateTime::currentMSecsSinceEpoch(), _connected, _batteryLevel, _isCharging);
    emit stateChanged(); emit capabilitiesChanged();
}

QString DeviceCenterController::deviceName() const { return _deviceName; }
QString DeviceCenterController::deviceAddress() const { return _deviceAddress; }
bool DeviceCenterController::isConnected() const { return _connected; }
int DeviceCenterController::batteryLevel() const { return _batteryLevel; }
bool DeviceCenterController::isCharging() const { return _isCharging; }
int DeviceCenterController::batteryLeft() const { return _batteryLeft; }
int DeviceCenterController::batteryRight() const { return _batteryRight; }
int DeviceCenterController::batteryCase() const { return _batteryCase; }
bool DeviceCenterController::hasDualBattery() const { return _batteryLeft >= 0 || _batteryRight >= 0; }
int DeviceCenterController::batteryMinutesLeft() const {
    if (!_connected || _isCharging) return -1;
    const auto e = _history->estimate(QDateTime::currentMSecsSinceEpoch());
    return e.valid ? static_cast<int>(e.remainingMs / 60000) : -1;
}
QString DeviceCenterController::batteryTimeLeft() const {
    const int minutes = batteryMinutesLeft();
    return minutes < 0 ? QString() : formatDuration(minutes);
}
double DeviceCenterController::batteryDischargeRate() const {
    if (!_connected || _isCharging) return 0.0;
    const auto e = _history->estimate(QDateTime::currentMSecsSinceEpoch());
    return e.valid ? e.percentPerHour : 0.0;
}
double DeviceCenterController::batterySessionStart() const {
    if (!_connected || _isCharging) return 0.0;
    return static_cast<double>(_history->estimate(QDateTime::currentMSecsSinceEpoch()).sessionStartMs);
}
QVariantList DeviceCenterController::batterySamples(double sinceMs) const {
    return _history->samplesSince(static_cast<qint64>(sinceMs));
}
QString DeviceCenterController::formatDuration(int minutes) const {
    minutes = std::max(0, minutes);
    if (minutes < 60) return t("duration_minutes").arg(minutes);
    return t("duration_hours_minutes").arg(minutes / 60).arg(minutes % 60);
}
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
    if (lower.contains("wf") && lower.contains("xm6")) {
        return "resources/devices/wf-1000xm6.png";
    } else if (lower.contains("wf") && lower.contains("xm5")) {
        return "resources/devices/wf-1000xm5.png";
    } else if (lower.contains("wf") && lower.contains("xm4")) {
        return "resources/devices/wf-1000xm4.png";
    } else if (lower.contains("wf") && lower.contains("xm3")) {
        return "resources/devices/wf-1000xm3.png";
    } else if (lower.contains("xm6")) {
        return "resources/devices/wh-1000xm6.png";
    } else if (lower.contains("xm5")) {
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

bool DeviceCenterController::hasAnc() const { return _capabilities.value("anc").toBool(); }

bool DeviceCenterController::hasAmbient() const { return _capabilities.value("ambient").toBool(); }

bool DeviceCenterController::hasEqualizer() const { return _capabilities.value("equalizer").toBool(); }

bool DeviceCenterController::hasClearBass() const { return _capabilities.value("clearBass").toBool(); }

bool DeviceCenterController::hasDsee() const { return _capabilities.value("dsee").toBool(); }

bool DeviceCenterController::hasSpeakToChat() const { return _capabilities.value("speakToChat").toBool(); }

bool DeviceCenterController::hasAdaptiveVolume() const { return _capabilities.value("adaptiveVolume").toBool(); }

QVariantList DeviceCenterController::pairedDevices() const { return _pairedDevices; }

void DeviceCenterController::setAnc(bool enabled) { _send("anc", {{"enabled",enabled}}); }
void DeviceCenterController::setAmbient(int level, bool voice) {
    // 0 means "whatever it was": the remembered level. 20 is the maximum on
    // every model.
    level = std::clamp(level > 0 ? level : _ambientLevel, 1, 20);
    _send("ambient", {{"level",level},{"focusOnVoice",voice}});
}
void DeviceCenterController::setNoiseControlOff() { setAnc(false); }
void DeviceCenterController::setEqualizerPreset(int preset) { _send("eqPreset", {{"preset",preset}}); }
void DeviceCenterController::setEqualizerCustom(int bass, const QVariantList& bands) {
    _send("eqCustom", {{"clearBass",bass},{"bands",QJsonArray::fromVariantList(bands)}});
}
void DeviceCenterController::setDsee(bool enabled) { _send("dsee", {{"enabled",enabled}}); }
void DeviceCenterController::setSpeakToChat(bool enabled) { _send("speakToChat", {{"enabled",enabled}}); }
void DeviceCenterController::setAdaptiveVolume(bool enabled) { _send("adaptiveVolume", {{"enabled",enabled}}); }
void DeviceCenterController::setAutoPowerOff(int index) { _send("autoPowerOff", {{"index",index}}); }
void DeviceCenterController::powerOff() { _send("powerOff"); }
void DeviceCenterController::connectDevice(const QString& address, const QString& name) { _send("connect", {{"address",address},{"name",name}}); }
void DeviceCenterController::disconnectDevice() { _send("disconnect"); }
void DeviceCenterController::refreshDiscoveredDevices() {
    QMetaObject::invokeMethod(_backend, &DeviceBackend::discover, Qt::QueuedConnection);
}

bool DeviceCenterController::autostart() const {
#if defined(Q_OS_LINUX)
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString desktopFile = autostartDir + "/sony-device-center.desktop";
    return QFileInfo::exists(desktopFile);
#elif defined(Q_OS_WIN)
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return bootSettings.contains("SonyDeviceCenter");
#else
    return false;
#endif
}

void DeviceCenterController::setAutostart(bool enable) {
#if defined(Q_OS_LINUX)
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString desktopFile = autostartDir + "/sony-device-center.desktop";
    if (enable) {
        QDir().mkpath(autostartDir);
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=Sony Device Center\n";
            out << "Comment=Unofficial open-source companion for Sony WH/WF/LinkBuds audio devices\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << " --minimized\n";
            out << "Icon=sony-device-center\n";
            out << "Terminal=false\n";
            out << "Categories=Audio;AudioVideo;Settings;\n";
            out << "X-GNOME-Autostart-enabled=true\n";
        }
    } else {
        QFile::remove(desktopFile);
    }
#elif defined(Q_OS_WIN)
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enable) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        bootSettings.setValue("SonyDeviceCenter", "\"" + appPath + "\" --minimized");
    } else {
        bootSettings.remove("SonyDeviceCenter");
    }
#endif
    emit autostartChanged();
}

void DeviceCenterController::setIconAntialiasing(bool enabled) {
    if (_iconAntialiasing == enabled) return;
    _iconAntialiasing = enabled;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("iconAntialiasing", enabled);
    emit appearanceChanged();
}

void DeviceCenterController::setThemeMode(const QString& mode) {
    if (mode != "dark" && mode != "light" && mode != "system") return;
    if (_themeMode == mode) return;
    _themeMode = mode;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("themeMode", mode);
    emit appearanceChanged();
}

void DeviceCenterController::setAnimationsEnabled(bool enabled) {
    if (_animationsEnabled == enabled) return;
    _animationsEnabled = enabled;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("animationsEnabled", enabled);
    emit appearanceChanged();
}

bool DeviceCenterController::minimizeToTray() const { return _minimizeToTray; }
void DeviceCenterController::setMinimizeToTray(bool enable) {
    if (_minimizeToTray == enable) return;
    _minimizeToTray = enable;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("minimizeToTray", enable);
    emit minimizeToTrayChanged();
}

void DeviceCenterController::setNotifyLowBattery(bool enable) {
    if (_notifyLowBattery == enable) return;
    _notifyLowBattery = enable;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("notifyLowBattery", enable);
    emit notificationSettingsChanged();
}
void DeviceCenterController::setNotifyConnection(bool enable) {
    if (_notifyConnection == enable) return;
    _notifyConnection = enable;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("notifyConnection", enable);
    emit notificationSettingsChanged();
}
void DeviceCenterController::setNotifyCharged(bool enable) {
    if (_notifyCharged == enable) return;
    _notifyCharged = enable;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("notifyCharged", enable);
    emit notificationSettingsChanged();
}
void DeviceCenterController::setLowBatteryThreshold(int percent) {
    percent = std::clamp(percent, 5, 50);
    if (_lowBatteryThreshold == percent) return;
    _lowBatteryThreshold = percent;
    QSettings("SonyBridge", "SonyDeviceCenter").setValue("lowBatteryThreshold", percent);
    emit notificationSettingsChanged();
}

QString DeviceCenterController::appVersion() const {
    return QCoreApplication::applicationVersion();
}

QString DeviceCenterController::currentLanguage() const {
    return _currentLanguage;
}

QVariantList DeviceCenterController::availableLanguages() const {
    return I18nManager::instance().availableLanguages();
}

void DeviceCenterController::setLanguage(const QString& langCode) {
    if (_currentLanguage != langCode) {
        _currentLanguage = langCode;
        QSettings settings("SonyBridge", "SonyDeviceCenter");
        settings.setValue("language", langCode);
        emit languageChanged();
    }
}

QString DeviceCenterController::t(const QString& key) const {
    return I18nManager::instance().translate(key, _currentLanguage);
}

void DeviceCenterController::openUrl(const QString& url) {
    QDesktopServices::openUrl(QUrl(url));
}

} // namespace sony::devicecenter
