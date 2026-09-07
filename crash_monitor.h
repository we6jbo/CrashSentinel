#pragma once
// READ ME: CrashMonitor is the recurring background evidence collector.
// start() initializes monitoring; collect() performs one cycle. Helper methods
// rotate history, read journals, analyze the previous boot, and report measured
// service impact. ResourceGuard can slow/suspend expensive collection.

// #ju56Us

#include <QObject>
#include <QTimer>
#include "crash_settings.h"

class CrashMonitor : public QObject
{
    Q_OBJECT
public:
    explicit CrashMonitor(QObject *parent = nullptr);
    bool start();

    // Exposed only so service startup/self-protection can use the exact same
    // state directory as the monitor.
    QString publicStateDir() const;

private slots:
    void collect();

private:
    QString stateDir() const;
    QString run(const QString &program, const QStringList &args, int timeoutMs = 5000) const;
    bool writeAtomic(const QString &path, const QByteArray &data) const;
    QString readFile(const QString &path) const;

    void rotateHistory(const QByteArray &snapshot);
    void writeJournalTail();
    void pruneHistory(int keepCount);
    void purgeForDiskPressure();
    void pruneByAge(int retentionDays);
    void analyzePreviousBootOnce();
    QString previousBootAnalysis() const;
    void writeFinalStopLog(const QStringList &reasons);
    void applyTimerInterval(int seconds);
    void writeServiceHealth(qint64 collectionMs,
                            qint64 snapshotBytes,
                            const QString &mode,
                            const QStringList &reasons) const;

    QTimer timer_;
    CrashSettings settings_;
    bool suspendedForCriticalResource_ = false;
};
