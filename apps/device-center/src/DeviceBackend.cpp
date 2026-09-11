#include "DeviceBackend.h"
#include "sony/core/DeviceService.h"
#include "sony/transport/PlatformTransport.h"
#include <QMetaObject>
namespace sony::devicecenter {
DeviceBackend::DeviceBackend(std::shared_ptr<core::IDeviceService> service) : _service(std::move(service)) {}
DeviceBackend::~DeviceBackend() { shutdown(); }
void DeviceBackend::shutdown() {
    _stopped = true;
    if (_timer) _timer->stop();
    if (_subscribed) { _subscribed->events().removeListener(_subscription); _subscribed = nullptr; }
    // Join the reader before QObject destruction; any callback already in flight
    // can still safely post to this context and will then be discarded by Qt.
    if (_service) _service->disconnect();
}
void DeviceBackend::start() {
    try {
        if (!_service) {
            _usingIpc = _ipc.isDaemonRunning();
            if (!_usingIpc) {
                _service = std::make_shared<core::DeviceService>(transport::createPlatformTransport(), transport::createPlatformDiscovery());
                _service->startAutoConnect();
            }
        }
        publish();
        discover();
        _timer = new QTimer(this);
        _timer->setInterval(_usingIpc ? 1000 : 500);
        connect(_timer, &QTimer::timeout, this, &DeviceBackend::poll);
        _timer->start();
        poll();
    } catch (const std::exception& ex) { emit error(QString::fromUtf8(ex.what()), _generation); }
    emit completed(_generation);
}
DeviceBackend::Json DeviceBackend::request(const std::string& method, const Json& params) {
    if (_incompatible) throw std::runtime_error("Update sonyd and Sony Device Center to matching versions");
    Json req{{"version",core::JsonProtocol::Version},{"id",++_requestId},{"method",method},{"params",params}};
    Json response;
    if (_usingIpc) {
        try { response = Json::parse(_ipc.request(req.dump())); }
        catch (const Json::exception&) { _incompatible = true; throw std::runtime_error("Daemon does not support structured IPC; update sonyd"); }
    } else response = core::JsonProtocol::execute(req, *_service);
    if (response.value("version",0) != core::JsonProtocol::Version || response.value("id",Json{}) != req["id"])
        throw std::runtime_error("Incompatible or mismatched daemon response; update sonyd");
    if (!response.value("ok",false)) {
        const auto e = response.value("error",Json::object());
        if (e.value("code",std::string{}) == "VersionMismatch") _incompatible = true;
        throw std::runtime_error(e.value("message",std::string("Device command failed")));
    }
    return response.at("data");
}
void DeviceBackend::publish() { emit snapshotReady(QByteArray::fromStdString(request("snapshot").dump()), _generation); }
void DeviceBackend::subscribe() {
    if (!_service) return;
    auto* device = _service->activeDevice();
    if (device == _subscribed) return;
    if (_subscribed) _subscribed->events().removeListener(_subscription);
    _subscribed = device;
    if (device) _subscription = device->events().onStateChanged([this](const auto&) {
        if (_notificationPending.exchange(true)) return;
        QMetaObject::invokeMethod(this, [this] {
            _notificationPending = false;
            if (!_stopped) { try { publish(); } catch (...) {} }
        }, Qt::QueuedConnection);
    });
}
void DeviceBackend::poll() {
    if (_stopped || _incompatible) return;
    try {
        if (_service) { _service->tick(); subscribe(); }
        publish();
    } catch (const std::exception& ex) {
        emit error(QString::fromUtf8(ex.what()), _generation);
        // In IPC mode a dead daemon must not leave the GUI displaying live data.
        emit snapshotReady(QByteArray("{\"connected\":false,\"connectionState\":\"disconnected\"}"), _generation);
    }
}
void DeviceBackend::command(QByteArray bytes, quint64 generation) {
    _generation = generation;
    if (_stopped) return;
    try {
        const auto cmd = Json::parse(bytes.toStdString());
        const auto data = request(cmd.at("method").get<std::string>(), cmd.value("params",Json::object()));
        subscribe();
        emit snapshotReady(QByteArray::fromStdString(data.dump()), _generation);
    } catch (const std::exception& ex) { emit error(QString::fromUtf8(ex.what()), _generation); }
    emit completed(_generation);
}
void DeviceBackend::discover() {
    try { emit devicesReady(QByteArray::fromStdString(request("devices").dump())); }
    catch (const std::exception& ex) { emit error(QString::fromUtf8(ex.what()), _generation); }
}
}
