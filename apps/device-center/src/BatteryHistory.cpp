#include "BatteryHistory.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariantMap>

#include <algorithm>

namespace sony::devicecenter {

namespace {
constexpr int kFileVersion = 1;
// A Bluetooth drop shorter than this, with the link coming straight back,
// is a hiccup rather than a break in use; the discharge session survives it.
constexpr qint64 kShortGapMs = 2LL * 60 * 1000;
// While the app is closed the log sees nothing. A "connected" marker that
// follows plain samples is such a gap; the headset kept draining on its own
// unless the level says otherwise, so the session carries on across it as
// long as the gap is not so long that it could hide a night on the shelf.
constexpr qint64 kMaxUnseenGapMs = 6LL * 3600 * 1000;
// No Sony headset drains faster than this. A bigger drop across an unseen
// gap means the charge was spent elsewhere (or on another cycle).
constexpr double kMaxPlausiblePercentPerHour = 15.0;
// ...and none lasts longer than a few days: a slower rate means the session
// swallowed hours the headset spent switched off, and the estimate would
// promise days.
constexpr double kMinPlausiblePercentPerHour = 1.0;

constexpr double hoursBetween(qint64 fromMs, qint64 toMs) {
    return static_cast<double>(toMs - fromMs) / (3600.0 * 1000.0);
}

// Whether a "connected" marker continues the discharge session the previous
// sample belongs to rather than starting a new one.
bool resumesSession(const BatteryHistory::Sample& previous, const BatteryHistory::Sample& connected) {
    const qint64 gap = connected.timeMs - previous.timeMs;
    // A higher level means the charger was involved in between.
    if (connected.level > previous.level) return false;
    // The link was seen dropping: the headset went off or away, and only a
    // hiccup keeps the session.
    if (previous.event == BatteryHistory::Event::Disconnected) return gap <= kShortGapMs;
    // Unseen gap (app restart): the same charge as long as the drop across
    // it could have happened in use.
    if (gap > kMaxUnseenGapMs) return false;
    const int drop = previous.level - connected.level;
    return drop == 0 || drop / hoursBetween(previous.timeMs, connected.timeMs) <= kMaxPlausiblePercentPerHour;
}

const char* eventName(BatteryHistory::Event event) {
    switch (event) {
    case BatteryHistory::Event::Connected: return "connected";
    case BatteryHistory::Event::Disconnected: return "disconnected";
    case BatteryHistory::Event::Level: break;
    }
    return "level";
}

BatteryHistory::Event eventFromName(const QString& name) {
    if (name == "connected") return BatteryHistory::Event::Connected;
    if (name == "disconnected") return BatteryHistory::Event::Disconnected;
    return BatteryHistory::Event::Level;
}
}

BatteryHistory::BatteryHistory(QString storageDir, QObject* parent)
    : QObject(parent), _storageDir(std::move(storageDir)) {}

BatteryHistory::~BatteryHistory() = default;

QString BatteryHistory::sanitizeAddress(const QString& address) {
    // "CC:98:8B:00:11:22" -> "CC-98-8B-00-11-22"; anything else that is not
    // a file-name character is dropped too.
    QString out;
    for (const QChar c : address) {
        if (c.isLetterOrNumber() || c == '-' || c == '_') out += c;
        else if (c == ':') out += '-';
    }
    return out;
}

QString BatteryHistory::filePath() const {
    if (_address.isEmpty()) return {};
    return _storageDir + "/battery-history/" + sanitizeAddress(_address) + ".json";
}

void BatteryHistory::setDevice(const QString& address) {
    if (address == _address) return;
    _address = address;
    _samples.clear();
    _wasConnected = false;
    _load();
    emit changed();
}

void BatteryHistory::observe(qint64 nowMs, bool connected, int level, bool charging) {
    if (_address.isEmpty()) return;
    if (!connected) {
        if (_wasConnected) {
            const int lastLevel = _samples.isEmpty() ? -1 : _samples.last().level;
            _append({nowMs, lastLevel, false, Event::Disconnected});
            _wasConnected = false;
        }
        return;
    }
    // Connected but no battery reading yet: the marker waits for a level so
    // the chart has a value to start the line from.
    if (level < 0) return;
    if (!_wasConnected) {
        _wasConnected = true;
        _append({nowMs, level, charging, Event::Connected});
        return;
    }
    const auto& last = _samples.last();
    if (last.level != level || last.charging != charging) _append({nowMs, level, charging, Event::Level});
}

void BatteryHistory::_append(Sample sample) {
    _samples.push_back(sample);
    _prune(sample.timeMs);
    _save();
    emit changed();
}

void BatteryHistory::_prune(qint64 nowMs) {
    const qint64 cutoff = nowMs - kRetentionMs;
    const auto first = std::find_if(_samples.begin(), _samples.end(), [cutoff](const Sample& s) { return s.timeMs >= cutoff; });
    if (first != _samples.begin()) _samples.erase(_samples.begin(), first);
}

QVariantList BatteryHistory::samplesSince(qint64 sinceMs) const {
    QVariantList out;
    auto it = std::lower_bound(_samples.begin(), _samples.end(), sinceMs,
                               [](const Sample& s, qint64 t) { return s.timeMs < t; });
    // One sample before the window lets the chart draw the line entering it.
    if (it != _samples.begin()) --it;
    for (; it != _samples.end(); ++it) {
        out.push_back(QVariantMap{
            {"t", it->timeMs}, {"level", it->level}, {"charging", it->charging}, {"event", QString::fromLatin1(eventName(it->event))}});
    }
    return out;
}

BatteryHistory::Estimate BatteryHistory::estimate(qint64 nowMs) const {
    Estimate result;
    if (_samples.isEmpty()) return result;
    const auto& end = _samples.last();
    if (end.charging || end.event == Event::Disconnected || end.level < 0) return result;

    // Walk back to the start of the current discharge session: the last
    // sample on the charger, or the last connect that did not merely resume.
    int start = 0;
    for (int i = _samples.size() - 1; i >= 0; --i) {
        const auto& s = _samples[i];
        if (s.charging || s.event == Event::Disconnected) { start = i + 1; break; }
        if (s.event == Event::Connected) {
            if (i == 0 || !resumesSession(_samples[i - 1], s)) { start = i; break; }
            // Skip the matching drop-out too; it would read as a boundary.
            if (_samples[i - 1].event == Event::Disconnected) --i;
        }
    }
    result.sessionStartMs = _samples[start].timeMs;

    // Connect markers sit at an unknown fraction of a percent (whenever the
    // charger came off, the link came up or the app started), so the rate is
    // measured between level changes only: the first one after the session
    // began and the last one seen.
    int anchor = -1, tail = -1;
    for (int i = start + 1; i < _samples.size(); ++i) {
        if (_samples[i].event != Event::Level) continue;
        if (anchor < 0) anchor = i;
        tail = i;
    }
    if (anchor < 0) return result;
    const int drop = _samples[anchor].level - _samples[tail].level;
    const qint64 span = _samples[tail].timeMs - _samples[anchor].timeMs;
    if (drop < kMinDropPercent || span < kMinSessionSpanMs) return result;

    const double perMs = static_cast<double>(drop) / static_cast<double>(span);
    result.percentPerHour = perMs * 3600.0 * 1000.0;
    if (result.percentPerHour < kMinPlausiblePercentPerHour) return result;
    result.valid = true;
    // The level has kept falling since it was last seen changing (or, after
    // an unseen gap, since the marker that reported it); count that time off.
    const qint64 reachedMs = end.level == _samples[tail].level ? _samples[tail].timeMs : end.timeMs;
    const double sinceEnd = static_cast<double>(std::max<qint64>(0, nowMs - reachedMs));
    result.remainingMs = static_cast<qint64>(std::max(0.0, end.level / perMs - sinceEnd));
    return result;
}

void BatteryHistory::seedDemoData(qint64 nowMs) {
    if (_address.isEmpty()) return;
    _samples.clear();
    constexpr qint64 kHour = 3600LL * 1000;
    constexpr qint64 kDay = 24 * kHour;
    // Six full days of "work": on at 09:00, off at 18:00, charged 20:00-21:30.
    // The last day ends two hours into a discharge that reaches the level the
    // simulator starts from, so the estimate has something to say at once.
    const qint64 today = QDateTime::fromMSecsSinceEpoch(nowMs).date().startOfDay().toMSecsSinceEpoch();
    for (int day = 7; day >= 1; --day) {
        const qint64 base = today - day * kDay;
        const qint64 on = base + 9 * kHour;
        const int startLevel = 96 - day;   // a little variety between days
        _samples.push_back({on, startLevel, false, Event::Connected});
        // ~6.5 %/h, one point per percent.
        const int dropped = 58;
        for (int i = 1; i <= dropped; ++i)
            _samples.push_back({on + i * (9 * kHour) / dropped, startLevel - i, false, Event::Level});
        _samples.push_back({base + 18 * kHour, startLevel - dropped, false, Event::Disconnected});
        const qint64 charge = base + 20 * kHour;
        _samples.push_back({charge, startLevel - dropped, true, Event::Connected});
        const int toCharge = 100 - (startLevel - dropped);
        for (int i = 1; i <= toCharge; ++i)
            _samples.push_back({charge + i * (90 * 60 * 1000) / toCharge, startLevel - dropped + i, true, Event::Level});
        _samples.push_back({charge + 90 * 60 * 1000 + 30 * 1000, 100, false, Event::Level});
        _samples.push_back({charge + 2 * kHour, 100, false, Event::Disconnected});
    }
    const int current = 87, from = 95;
    const qint64 sessionStart = nowMs - 2 * kHour;
    _samples.push_back({sessionStart, from, false, Event::Connected});
    for (int i = 1; i <= from - current; ++i)
        _samples.push_back({sessionStart + i * (2 * kHour) / (from - current), from - i, false, Event::Level});
    // The simulator then reports 87 as connected: same as the last sample,
    // and the marker would otherwise restart the session.
    _wasConnected = true;
    _save();
    emit changed();
}

void BatteryHistory::_load() {
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value("version").toInt() != kFileVersion) return;
    for (const auto& value : root.value("samples").toArray()) {
        const auto o = value.toObject();
        _samples.push_back({static_cast<qint64>(o.value("t").toDouble()), o.value("level").toInt(-1),
                            o.value("charging").toBool(), eventFromName(o.value("event").toString())});
    }
    std::stable_sort(_samples.begin(), _samples.end(), [](const Sample& a, const Sample& b) { return a.timeMs < b.timeMs; });
    if (!_samples.isEmpty()) _prune(_samples.last().timeMs);
}

void BatteryHistory::_save() const {
    const auto path = filePath();
    if (path.isEmpty()) return;
    QDir().mkpath(QFileInfo(path).path());
    QJsonArray samples;
    for (const auto& s : _samples) {
        samples.push_back(QJsonObject{
            {"t", static_cast<double>(s.timeMs)}, {"level", s.level}, {"charging", s.charging}, {"event", eventName(s.event)}});
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonObject{{"version", kFileVersion}, {"address", _address}, {"samples", samples}})
                   .toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace sony::devicecenter
