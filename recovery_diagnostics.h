#pragma once
// READ ME: lightweight recovery operations that remain useful even when the
// monitor is quarantined. Recovery is intentionally limited to CrashSentinel
// state; it does not claim to repair Linux, firmware, or hardware.

// #ju56Us

#include <QString>
#include <QStringList>

struct RecoveryDiagnosticsStatus
{
    bool quarantined = false;
    int rapidFailures = 0;
    QString quarantineReason;

    bool stateDirWritable = false;
    bool serviceHealthExists = false;
    bool previousBootReportExists = false;
    bool analysisMarkerExists = false;

    int guiLikeInstances = 0;
    int serviceInstances = 0;
    QStringList processLines;

    QString summary;
    QString details;
};

class RecoveryDiagnostics
{
public:
    static RecoveryDiagnosticsStatus inspect(const QString &stateDir);
    static bool clearSelfQuarantine(const QString &stateDir, QString *error = nullptr);
    static bool rebuildDerivedReportMarker(const QString &stateDir, QString *error = nullptr);
    static QString createBackup(const QString &stateDir, QString *error = nullptr);
    static QString exportDiagnosticReport(const QString &stateDir, QString *error = nullptr);
};
