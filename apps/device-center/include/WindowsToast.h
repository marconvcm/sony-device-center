#pragma once

#include <QString>

namespace sony::devicecenter {

// Native Windows toast notifications (WinRT ToastNotificationManager) for
// this unpackaged app. Unlike QSystemTrayIcon::showMessage, which rides on
// legacy balloon tips that Windows 11 often drops, these reach the
// notification centre under the app's own name and icon.
//
// No-ops (returning false) on other platforms and when WinRT is unavailable.
namespace WindowsToast {

// Registers the AppUserModelID Windows needs to attribute toasts to an
// unpackaged executable. Call once at startup, before any window is shown.
void registerApplication();

// Shows a two-line toast. Returns false if it could not be shown, so the
// caller can fall back to the tray balloon.
bool show(const QString& title, const QString& body);

} // namespace WindowsToast

} // namespace sony::devicecenter
