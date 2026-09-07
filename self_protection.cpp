// #ju56Us
// READ ME: self-protection state machine.
// A changed boot ID while the prior service state was still "active" counts as
// an abnormal reboot observed while monitoring was active. That is correlation,
// NOT proof CrashSentinel caused the reboot.
//
// 5 reboots: precautionary quarantine unless deliberately acknowledged.
// 20 reboots: hard stop; ordinary quarantine clearing is refused.
// The full reset is intentionally separate because it erases the reboot count.

#include "self_protection.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

static QJsonObject readObject(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto d = QJsonDocument::fromJson(f.readAll());
    return d.isObject() ? d.object() : QJsonObject();
}

static bool writeObject(const QString &path, const QJsonObject &o)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}

// Write a human-readable explanation beside the machine-readable JSON state.
static bool writeDisabledNotice(const QString &stateDir,
                                int abnormalReboots,
                                bool hardStop)
{
    const QString path = QDir(stateDir).filePath("CRASHSENTINEL-DISABLED.txt");
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out << "#ju56Us\n";
    out << "CrashSentinel automatic reboot safeguard\n\n";
    out << "CrashSentinel monitoring has been disabled because the computer appears to have rebooted abnormally while CrashSentinel monitoring was active.\n\n";
    out << "Observed abnormal reboot count: " << abnormalReboots << "\n\n";

    if (hardStop) {
        out << "HARD SAFETY STOP: the count reached 20. CrashSentinel will not automatically resume monitoring. Twenty abnormal reboots is too many to continue automatically even though this does NOT prove CrashSentinel caused the crashes.\n\n";
    } else {
        out << "PRECAUTIONARY STOP: the count reached at least 5. CrashSentinel may or may not be related to the crashes, so monitoring was stopped as a safety precaution.\n\n";
    }

    out << "To re-enable monitoring deliberately after the 5-reboot stop:\n";
    out << "  CrashSentinel --clear-quarantine\n\n";
    out << "For a Snap installation, use the installed CrashSentinel command "
           "with --clear-quarantine, then allow snapd to manage the monitor "
           "service normally.\n\n";
    out << "At the 20-reboot hard stop, normal quarantine clearing is refused. "
           "The user must explicitly reset the reboot safeguard with:\n";
    out << "  CrashSentinel --reset-reboot-safeguard\n\n";
    out << "The 20-reboot reset clears the abnormal reboot counter. Use it only "
           "after reviewing the crash evidence.\n\n";
    out << "Do not interpret this safeguard as proof that CrashSentinel caused the reboot. It is intentionally conservative.\n";
    return f.commit();
}

QString SelfProtection::bootId()
{
    QFile f("/proc/sys/kernel/random/boot_id");
    if (!f.open(QIODevice::ReadOnly))
        return "unknown";
    return QString::fromUtf8(f.readAll()).trimmed();
}

double SelfProtection::uptimeSeconds()
{
    QFile f("/proc/uptime");
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    return QString::fromUtf8(f.readLine()).section(' ', 0, 0).toDouble();
}

QString SelfProtection::statePath(const QString &stateDir)
{
    return QDir(stateDir).filePath("self-protection.json");
}

QString SelfProtection::disabledNoticePath(const QString &stateDir)
{
    return QDir(stateDir).filePath("CRASHSENTINEL-DISABLED.txt");
}


SelfProtectionState SelfProtection::readState(const QString &stateDir)
{
    const QJsonObject o = readObject(statePath(stateDir));
    SelfProtectionState state;
    state.quarantined = o.value("quarantined").toBool(false);
    state.rapidFailures = o.value("rapid_failures").toInt(0);
    state.abnormalReboots = o.value("abnormal_reboots").toInt(0);
    state.rebootGuardAcknowledged = o.value("reboot_guard_acknowledged").toBool(false);
    state.hardRebootStop = o.value("hard_reboot_stop").toBool(false);
    state.reason = o.value("reason").toString();
    return state;
}

// Core safety decision: classify the previous run before allowing this service start.
SelfProtectionState SelfProtection::noteServiceStart(const QString &stateDir)
{
    QDir().mkpath(stateDir);
    const QString path = statePath(stateDir);
    QJsonObject o = readObject(path);

    const QString currentBoot = bootId();
    const QString priorBoot = o.value("boot_id").toString();
    const QString priorState = o.value("run_state").toString();
    const double priorHeartbeat = o.value("last_heartbeat_uptime").toDouble(-1.0);
    const double now = uptimeSeconds();

    int failures = o.value("rapid_failures").toInt(0);
    int abnormalReboots = o.value("abnormal_reboots").toInt(0);
    bool acknowledged = o.value("reboot_guard_acknowledged").toBool(false);
    bool hardStop = o.value("hard_reboot_stop").toBool(false);
    bool quarantined = o.value("quarantined").toBool(false);

    if (!quarantined && priorBoot == currentBoot && priorState == "active" &&
        priorHeartbeat >= 0 && now >= 0 && now - priorHeartbeat < 180.0) {
        ++failures;
    } else if (priorBoot != currentBoot) {
        failures = 0;
    }

    // A changed boot ID while the previous service state was still active is
    // counted as an abnormal reboot observed while CrashSentinel was running.
    // This is correlation, not proof of causation.
    if (!priorBoot.isEmpty() && priorBoot != "unknown" && currentBoot != "unknown" &&
        priorBoot != currentBoot && priorState == "active") {
        ++abnormalReboots;
    }

    QString reason = o.value("reason").toString();

    if (failures >= 3) {
        quarantined = true;
        reason = "CrashSentinel restarted abnormally at least three times during the same boot. Heavy monitoring was self-quarantined to avoid a possible crash loop. This does not prove CrashSentinel caused a whole-system crash.";
    }

    if (abnormalReboots >= 5 && !acknowledged) {
        quarantined = true;
        reason = QString("CrashSentinel observed %1 abnormal computer reboots while monitoring was active. Monitoring has been disabled as a precaution because CrashSentinel could be involved, although this does not prove causation. See CRASHSENTINEL-DISABLED.txt.").arg(abnormalReboots);
    }

    if (abnormalReboots >= 20) {
        hardStop = true;
        quarantined = true;
        acknowledged = false;
        reason = QString("HARD SAFETY STOP: CrashSentinel observed %1 abnormal computer reboots while monitoring was active. Monitoring will not continue automatically. This still does not prove CrashSentinel caused the crashes. See CRASHSENTINEL-DISABLED.txt.").arg(abnormalReboots);
    }

    o["_audit_tag"] = "#ju56Us";
    o["schema"] = 2;
    o["boot_id"] = currentBoot;
    o["run_state"] = quarantined ? "quarantined" : "active";
    o["start_uptime_seconds"] = now;
    o["last_heartbeat_uptime"] = now;
    o["rapid_failures"] = failures;
    o["abnormal_reboots"] = abnormalReboots;
    o["reboot_guard_acknowledged"] = acknowledged;
    o["hard_reboot_stop"] = hardStop;
    o["quarantined"] = quarantined;
    o["reason"] = reason;
    writeObject(path, o);

    if (quarantined && abnormalReboots >= 5)
        writeDisabledNotice(stateDir, abnormalReboots, hardStop);

    SelfProtectionState state;
    state.quarantined = quarantined;
    state.rapidFailures = failures;
    state.abnormalReboots = abnormalReboots;
    state.rebootGuardAcknowledged = acknowledged;
    state.hardRebootStop = hardStop;
    state.reason = reason;
    return state;
}

// Mark the current service as alive using monotonic uptime, not visible wall-clock time.
void SelfProtection::noteHeartbeat(const QString &stateDir)
{
    const QString path = statePath(stateDir);
    QJsonObject o = readObject(path);
    o["_audit_tag"] = "#ju56Us";
    o["boot_id"] = bootId();
    if (!o.value("quarantined").toBool(false))
        o["run_state"] = "active";
    o["last_heartbeat_uptime"] = uptimeSeconds();
    writeObject(path, o);
}

// A clean exit prevents the next boot/start from being misclassified as abnormal.
void SelfProtection::noteCleanExit(const QString &stateDir)
{
    const QString path = statePath(stateDir);
    QJsonObject o = readObject(path);
    o["_audit_tag"] = "#ju56Us";
    o["boot_id"] = bootId();
    o["run_state"] = "clean_exit";
    o["last_heartbeat_uptime"] = uptimeSeconds();
    o["rapid_failures"] = 0;
    writeObject(path, o);
}

// Normal acknowledgement: preserves abnormal reboot count and cannot bypass hard stop.
bool SelfProtection::clearQuarantine(const QString &stateDir, QString *error)
{
    const QString path = statePath(stateDir);
    QJsonObject o = readObject(path);
    const int abnormalReboots = o.value("abnormal_reboots").toInt(0);
    const bool hardStop = o.value("hard_reboot_stop").toBool(false);

    if (hardStop) {
        if (error)
            *error = "CrashSentinel reached the 20-abnormal-reboot hard safety stop. Use the explicit reboot-safeguard reset before restarting monitoring.";
        return false;
    }

    o["_audit_tag"] = "#ju56Us";
    o["schema"] = 2;
    o["boot_id"] = bootId();
    o["run_state"] = "cleared";
    o["rapid_failures"] = 0;
    o["quarantined"] = false;
    o["reason"] = "";
    if (abnormalReboots >= 5)
        o["reboot_guard_acknowledged"] = true;

    if (!writeObject(path, o)) {
        if (error) *error = "Could not write self-protection state.";
        return false;
    }

    QFile::remove(disabledNoticePath(stateDir));
    return true;
}


// Explicit full reset: intentionally clears the accumulated reboot count.
bool SelfProtection::resetRebootSafeguard(const QString &stateDir, QString *error)
{
    const QString path = statePath(stateDir);
    QJsonObject o = readObject(path);
    o["_audit_tag"] = "#ju56Us";
    o["schema"] = 2;
    o["boot_id"] = bootId();
    o["run_state"] = "cleared";
    o["rapid_failures"] = 0;
    o["abnormal_reboots"] = 0;
    o["reboot_guard_acknowledged"] = false;
    o["hard_reboot_stop"] = false;
    o["quarantined"] = false;
    o["reason"] = "";
    if (!writeObject(path, o)) {
        if (error) *error = "Could not reset the reboot safeguard state.";
        return false;
    }
    QFile::remove(disabledNoticePath(stateDir));
    return true;
}
