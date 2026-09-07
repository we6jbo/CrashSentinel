// #ju56Us
// READ ME: loads and saves the user-configurable CrashSentinel settings.
// Keep compatibility in mind when adding fields: old settings files may not
// contain newly introduced keys, so defaults matter.

#include "crash_settings.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QFile>

QString CrashSettings::configPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config/j03.page/CrashSentinel");
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("settings.json"));
}

// Load each key with a safe default so older/missing config remains usable.
CrashSettings CrashSettings::load()
{
    CrashSettings s;
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly))
        return s;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return s;

    const QJsonObject o = doc.object();
    s.minDiskFreeGiB = o.value("minDiskFreeGiB").toInt(s.minDiskFreeGiB);
    s.maxDiskUsedPercent = o.value("maxDiskUsedPercent").toInt(s.maxDiskUsedPercent);
    s.minMemoryAvailableMiB = o.value("minMemoryAvailableMiB").toInt(s.minMemoryAvailableMiB);
    s.warningBatteryPercent = o.value("warningBatteryPercent").toInt(s.warningBatteryPercent);
    s.criticalBatteryPercent = o.value("criticalBatteryPercent").toInt(s.criticalBatteryPercent);
    s.maxCpuPressureSomeAvg10 = o.value("maxCpuPressureSomeAvg10").toInt(s.maxCpuPressureSomeAvg10);
    s.maxIoPressureSomeAvg10 = o.value("maxIoPressureSomeAvg10").toInt(s.maxIoPressureSomeAvg10);
    s.maxTemperatureC = o.value("maxTemperatureC").toInt(s.maxTemperatureC);
    s.logRetentionDays = o.value("logRetentionDays").toInt(s.logRetentionDays);
    s.snapshotIntervalSeconds = o.value("snapshotIntervalSeconds").toInt(s.snapshotIntervalSeconds);
    s.gamingProfile = o.value("gamingProfile").toBool(s.gamingProfile);
    s.pauseHeavyCollectionOnPressure = o.value("pauseHeavyCollectionOnPressure").toBool(s.pauseHeavyCollectionOnPressure);
    s.autoPurgeOldLogs = o.value("autoPurgeOldLogs").toBool(s.autoPurgeOldLogs);
    return s;
}

// Persist only user-adjustable preferences; hard safety limits live in ResourceGuard.
bool CrashSettings::save(QString *errorMessage) const
{
    QJsonObject o;
    o.insert("_audit_tag", "#ju56Us");
    o.insert("schema", 1);
    o.insert("minDiskFreeGiB", minDiskFreeGiB);
    o.insert("maxDiskUsedPercent", maxDiskUsedPercent);
    o.insert("minMemoryAvailableMiB", minMemoryAvailableMiB);
    o.insert("warningBatteryPercent", warningBatteryPercent);
    o.insert("criticalBatteryPercent", criticalBatteryPercent);
    o.insert("maxCpuPressureSomeAvg10", maxCpuPressureSomeAvg10);
    o.insert("maxIoPressureSomeAvg10", maxIoPressureSomeAvg10);
    o.insert("maxTemperatureC", maxTemperatureC);
    o.insert("logRetentionDays", logRetentionDays);
    o.insert("snapshotIntervalSeconds", snapshotIntervalSeconds);
    o.insert("gamingProfile", gamingProfile);
    o.insert("pauseHeavyCollectionOnPressure", pauseHeavyCollectionOnPressure);
    o.insert("autoPurgeOldLogs", autoPurgeOldLogs);

    QSaveFile f(configPath());
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = f.errorString();
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not commit settings file.");
        return false;
    }
    return true;
}
