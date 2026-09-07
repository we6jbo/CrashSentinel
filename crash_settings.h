#pragma once
// READ ME: user-adjustable preferences live here.
// These values can request MORE conservative behavior, but ResourceGuard has
// hardcoded safety floors/ceilings that user settings cannot weaken.

// #ju56Us

#include <QString>

struct CrashSettings
{
    // User-configurable thresholds. The hardcoded floor/ceiling in ResourceGuard
    // always wins if the user chooses something less protective.
    int minDiskFreeGiB = 10;
    int maxDiskUsedPercent = 75;
    int minMemoryAvailableMiB = 6144;
    int warningBatteryPercent = 30;
    int criticalBatteryPercent = 8;
    int maxCpuPressureSomeAvg10 = 35;
    int maxIoPressureSomeAvg10 = 25;
    int maxTemperatureC = 85;
    int logRetentionDays = 7;
    int snapshotIntervalSeconds = 20;
    bool gamingProfile = false;
    bool pauseHeavyCollectionOnPressure = true;
    bool autoPurgeOldLogs = true;

    static CrashSettings load();
    bool save(QString *errorMessage = nullptr) const;
    static QString configPath();
};
