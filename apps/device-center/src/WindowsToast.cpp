#include "WindowsToast.h"

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include <windows.h>
#include <shobjidl.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

namespace sony::devicecenter::WindowsToast {

namespace {

constexpr const wchar_t* kAppUserModelId = L"SonyBridge.SonyDeviceCenter";

// Windows reads the toast icon from a file path, not from Qt resources, so
// the brand PNG is written once to the app data folder.
QString iconPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString path = dir + "/toast-icon.png";
    if (!QFile::exists(path)) {
        QDir().mkpath(dir);
        QFile::copy(":/resources/brand/app-icon.png", path);
    }
    return QDir::toNativeSeparators(path);
}

QString escapeXml(QString text) {
    return text.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
}

} // namespace

void registerApplication() {
    // Tie this process to the ID so the shell attributes windows and toasts to it.
    SetCurrentProcessExplicitAppUserModelID(kAppUserModelId);

    // The registry entry is what makes the ID show up as "Sony Device Center"
    // with our icon in the notification centre and in Settings > Notifications.
    QSettings registry(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\AppUserModelId\\SonyBridge.SonyDeviceCenter"),
                       QSettings::NativeFormat);
    registry.setValue("DisplayName", "Sony Device Center");
    registry.setValue("IconUri", iconPath());
    registry.setValue("IconBackgroundColor", "0");
}

bool show(const QString& title, const QString& body) {
    try {
        using namespace winrt::Windows::UI::Notifications;
        using namespace winrt::Windows::Data::Xml::Dom;

        const QString xml = "<toast><visual><binding template='ToastGeneric'>"
                            "<text>" + escapeXml(title) + "</text>"
                            "<text>" + escapeXml(body) + "</text>"
                            "</binding></visual></toast>";
        XmlDocument document;
        document.LoadXml(winrt::hstring(xml.toStdWString()));
        ToastNotificationManager::CreateToastNotifier(kAppUserModelId).Show(ToastNotification(document));
        return true;
    } catch (const winrt::hresult_error&) {
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace sony::devicecenter::WindowsToast

#else

namespace sony::devicecenter::WindowsToast {
void registerApplication() {}
bool show(const QString&, const QString&) { return false; }
}

#endif
