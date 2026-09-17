#pragma once

#include <QIcon>
#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWindow;

namespace sony::devicecenter {

class DeviceCenterController;

// System tray presence: a battery-ring icon, a quick-control menu and
// show/hide of the main window. Mirrors DeviceCenterController state; it never
// talks to the device itself.
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(DeviceCenterController& controller, QObject* parent = nullptr);
    ~TrayController() override;

    // The QML ApplicationWindow, once the engine has created it.
    void setWindow(QWindow* window);
    [[nodiscard]] bool isAvailable() const;

    // Rendered tray glyph: a ring showing charge level with the percentage
    // inside. Grey when disconnected. Exposed for tests and previews.
    static QIcon renderIcon(int level, bool charging, bool connected, int size = 64);

public slots:
    void toggleWindow();
    void showWindow();
    // Desktop notification through the tray (a toast on Windows). No-op
    // without a tray.
    void showMessage(const QString& title, const QString& body);

signals:
    void quitRequested();

private:
    void _buildMenu();
    void _update();

    DeviceCenterController& _controller;
    QSystemTrayIcon* _tray{nullptr};
    QMenu* _menu{nullptr};
    QAction* _status{nullptr};
    QAction* _noiseCancelling{nullptr};
    QAction* _ambient{nullptr};
    QAction* _off{nullptr};
    QAction* _speakToChat{nullptr};
    QAction* _powerOff{nullptr};
    QAction* _show{nullptr};
    QAction* _quit{nullptr};
    QPointer<QWindow> _window;
    // Last values the icon was drawn for; redraw only when they change.
    int _drawnLevel{-2};
    bool _drawnCharging{false};
    bool _drawnConnected{false};
};

} // namespace sony::devicecenter
