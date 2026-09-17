#pragma once

#include <QObject>
#include <QString>

namespace sony::devicecenter {

class DeviceCenterController;
class TrayController;

// Turns state transitions into desktop notifications: low battery, link
// up/down, charge complete. Edge-triggered so a state that persists across
// the controller's polling never repeats a message.
class NotificationController : public QObject {
    Q_OBJECT
public:
    NotificationController(DeviceCenterController& controller, TrayController& tray, QObject* parent = nullptr);

    // The most recent message and how many were shown in total. Lets tests
    // check decisions without a tray.
    [[nodiscard]] QString lastMessage() const { return _lastMessage; }
    [[nodiscard]] int messageCount() const { return _messageCount; }

private:
    void _evaluate();
    void _notify(const QString& title, const QString& body);

    DeviceCenterController& _controller;
    TrayController& _tray;
    QString _lastMessage;
    int _messageCount{0};

    bool _wasConnected{false};
    bool _seenState{false};
    // Which low-battery step (threshold, then 10%) has been announced for
    // this discharge; reset once the level climbs back above the threshold.
    int _lowStepAnnounced{-1};
    bool _chargedAnnounced{false};
};

} // namespace sony::devicecenter
