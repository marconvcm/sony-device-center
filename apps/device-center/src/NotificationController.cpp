#include "NotificationController.h"
#include "DeviceCenterController.h"
#include "TrayController.h"

namespace sony::devicecenter {

namespace {
constexpr int kCriticalBattery = 10;
}

NotificationController::NotificationController(DeviceCenterController& controller, TrayController& tray, QObject* parent)
    : QObject(parent), _controller(controller), _tray(tray) {
    connect(&_controller, &DeviceCenterController::stateChanged, this, &NotificationController::_evaluate);
}

void NotificationController::_evaluate() {
    const bool connected = _controller.isConnected();
    const int level = _controller.batteryLevel();
    const bool charging = _controller.isCharging();
    auto t = [this](const char* key) { return _controller.t(QString::fromLatin1(key)); };

    // Connection edges. The very first snapshot is a baseline, not an event:
    // starting the app next to already-connected headphones is not news.
    if (_seenState && connected != _wasConnected && _controller.notifyConnection()) {
        if (connected) _notify(_controller.deviceName(), t("notify_connected"));
        else _notify(t("notify_disconnected_title"), t("notify_disconnected"));
    }
    _seenState = true;
    _wasConnected = connected;

    if (!connected || level < 0) {
        // Losing the device resets the discharge bookkeeping so the next
        // session announces again.
        _lowStepAnnounced = -1;
        _chargedAnnounced = false;
        return;
    }

    if (charging) {
        _lowStepAnnounced = -1;
        if (level >= 100 && !_chargedAnnounced && _controller.notifyCharged()) {
            _chargedAnnounced = true;
            _notify(_controller.deviceName(), t("notify_charged"));
        }
        return;
    }
    _chargedAnnounced = false;

    const int threshold = _controller.lowBatteryThreshold();
    if (level > threshold) { _lowStepAnnounced = -1; return; }
    // Step 0 at the user's threshold, step 1 at the fixed critical level.
    const int step = level <= kCriticalBattery && kCriticalBattery < threshold ? 1 : 0;
    if (step > _lowStepAnnounced && _controller.notifyLowBattery()) {
        _lowStepAnnounced = step;
        _notify(_controller.deviceName(), t("notify_low_battery").arg(level));
    }
}

void NotificationController::_notify(const QString& title, const QString& body) {
    _lastMessage = title + ": " + body;
    ++_messageCount;
    _tray.showMessage(title, body);
}

} // namespace sony::devicecenter
