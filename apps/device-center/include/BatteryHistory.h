#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

namespace sony::devicecenter {

// Charge-level log for one device plus the "time left" estimate derived from
// it. Pure Qt Core: no GUI, no clock of its own — every call takes the current
// time so tests can replay days in milliseconds.
//
// One JSON file per device address under the storage directory. A sample is
// written only when the level or the charging flag changes, so a headset
// polled twice a second still produces a few dozen points a day.
class BatteryHistory : public QObject {
    Q_OBJECT
public:
    enum class Event { Level, Connected, Disconnected };

    struct Sample {
        qint64 timeMs{0};   // Unix time in milliseconds
        int level{-1};      // 0-100, or -1 when the device did not report one
        bool charging{false};
        Event event{Event::Level};
    };

    struct Estimate {
        bool valid{false};
        qint64 remainingMs{0};
        double percentPerHour{0.0};  // discharge rate, positive
        qint64 sessionStartMs{0};    // when the current discharge began
    };

    // Samples older than this are dropped on load and save.
    static constexpr qint64 kRetentionMs = 180LL * 24 * 3600 * 1000;
    // A rate is only trusted once two level changes have been seen a while
    // apart; before that the estimate is "unknown". One percent is enough
    // because both ends of the measurement sit on a reported change, so the
    // interval between them is a whole percent of use.
    static constexpr int kMinDropPercent = 1;
    static constexpr qint64 kMinSessionSpanMs = 5LL * 60 * 1000;

    explicit BatteryHistory(QString storageDir, QObject* parent = nullptr);
    ~BatteryHistory() override;

    // Switch to another device: the previous log is flushed, the new one
    // loaded. An empty address means "no device", nothing is recorded.
    void setDevice(const QString& address);
    [[nodiscard]] QString device() const { return _address; }

    // Feed the state the controller just received. Appends a sample only on
    // a change, and connect/disconnect markers on the edges.
    void observe(qint64 nowMs, bool connected, int level, bool charging);

    [[nodiscard]] const QVector<Sample>& samples() const { return _samples; }
    // Samples at or after sinceMs, as maps for QML: {t, level, charging, event}.
    [[nodiscard]] QVariantList samplesSince(qint64 sinceMs) const;

    [[nodiscard]] Estimate estimate(qint64 nowMs) const;

    // Replaces the log with a synthetic week of use ending at nowMs. Used by
    // the simulator so the chart and the estimate can be looked at at once.
    void seedDemoData(qint64 nowMs);

    static QString sanitizeAddress(const QString& address);
    [[nodiscard]] QString filePath() const;

signals:
    void changed();

private:
    void _append(Sample sample);
    void _load();
    void _save() const;
    void _prune(qint64 nowMs);

    QString _storageDir;
    QString _address;
    QVector<Sample> _samples;
    bool _wasConnected{false};
};

} // namespace sony::devicecenter
