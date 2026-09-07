#pragma once
// READ ME: ResourceGuard decides whether collection stays normal, becomes
// degraded/slower, or enters critical-stop protection. It protects the machine
// from the monitor consuming scarce disk, memory, battery, or thermal headroom.

// #ju56Us

#include "crash_settings.h"
#include <QString>
#include <QStringList>

struct ResourceGuardState
{
    bool criticalStop = false;
    bool degraded = false;
    bool purgeRequested = false;
    int recommendedIntervalSeconds = 20;
    QStringList reasons;
};

class ResourceGuard
{
public:
    static ResourceGuardState evaluate(const CrashSettings &settings, const QString &stateDir);
    static QString hardcodedPolicyText();

private:
    static qint64 availableMemoryMiB();
    static qint64 freeDiskMiB(const QString &path);
    static int diskUsedPercent(const QString &path);
    static int batteryPercent();
    static bool batteryDischarging();
    static double maxTemperatureC();
    static double pressureSomeAvg10(const QString &path);
};
