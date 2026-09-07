// #ju56Us
// READ ME: core monitoring loop.
// Important design rule: evidence collection must be lightweight and must not
// claim a cause that Linux did not record. Severe power, firmware, hardware, or
// kernel failures can occur before the reason reaches persistent storage.

#include "crash_monitor.h"
#include "context_provenance.h"
#include "repair_attestation.h"
#include "resource_guard.h"
#include "self_protection.h"
#include "deep_diagnostics.h"
#include "state_paths.h"
#include "version_info.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfoList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

#include <pwd.h>
#include <unistd.h>

// Monotonic uptime survives wall-clock policy concerns and is suitable for intervals.
static QString monotonicSeconds()
{
    QFile f(QStringLiteral("/proc/uptime"));
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("unknown");
    return QString::fromUtf8(f.readLine().split(' ').value(0));
}

static QString contextVisibleStamp(const MachineContext &ctx)
{
    if (!ctx.timeVisible)
        return ctx.date.isEmpty()
            ? QStringLiteral("time-hidden-by-policy")
            : ctx.date + QStringLiteral(" time-hidden-by-policy");

    QString v = ctx.date;
    if (!ctx.timeDisplay.isEmpty()) v += QStringLiteral(" ") + ctx.timeDisplay;
    if (!ctx.timezone.isEmpty()) v += QStringLiteral(" ") + ctx.timezone;
    if (!ctx.timePolicy.isEmpty()) v += QStringLiteral(" (") + ctx.timePolicy + QStringLiteral(")");
    return v;
}

static QString effectiveUserName()
{
    const uid_t uid = geteuid();
    if (passwd *pw = getpwuid(uid))
        return QString::fromLocal8Bit(pw->pw_name);
    return QString::number(uid);
}

// Measure this process's resident memory so Status can report monitoring impact.
static double currentRssMiB()
{
    QFile statm(QStringLiteral("/proc/self/statm"));

    if (statm.open(QIODevice::ReadOnly)) {
        const auto parts =
            statm.readLine().simplified().split(' ');

        if (parts.size() >= 2) {
            bool ok = false;

            const qint64 residentPages =
                parts[1].toLongLong(&ok);

            const long pageSize =
                sysconf(_SC_PAGESIZE);

            if (ok && pageSize > 0) {
                return double(residentPages) *
                       double(pageSize) /
                       (1024.0 * 1024.0);
            }
        }
    }

    return -1.0;
}

CrashMonitor::CrashMonitor(QObject *parent) : QObject(parent)
{
    connect(&timer_, &QTimer::timeout, this, &CrashMonitor::collect);
}

// Always use the shared package-aware state resolver.
QString CrashMonitor::stateDir() const
{
    return StatePaths::sharedStateDir();
}

QString CrashMonitor::publicStateDir() const
{
    return stateDir();
}

// Run a read-only helper command with a timeout; callers must treat failure as unknown.
QString CrashMonitor::run(const QString &program, const QStringList &args, int timeoutMs) const
{
    QProcess p;
    p.start(program, args);

    if (!p.waitForStarted(1500))
        return QStringLiteral("[unavailable]\n");

    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        return QStringLiteral("[timed out]\n");
    }

    return QString::fromUtf8(p.readAllStandardOutput()) +
           QString::fromUtf8(p.readAllStandardError());
}

bool CrashMonitor::writeAtomic(const QString &path, const QByteArray &data) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    f.write(data);
    return f.commit();
}

QString CrashMonitor::readFile(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("[unavailable]\n");
    return QString::fromUtf8(f.readAll());
}

void CrashMonitor::applyTimerInterval(int seconds)
{
    const int ms = qBound(20, seconds, 600) * 1000;
    if (timer_.interval() != ms)
        timer_.setInterval(ms);
}

void CrashMonitor::pruneHistory(int keepCount)
{
    QDir h(QDir(stateDir()).filePath(QStringLiteral("history")));
    QFileInfoList entries =
        h.entryInfoList({QStringLiteral("snapshot-*.txt")},
                        QDir::Files, QDir::Time);

    for (int i = keepCount; i < entries.size(); ++i)
        QFile::remove(entries.at(i).absoluteFilePath());
}

void CrashMonitor::pruneByAge(int retentionDays)
{
    const QDateTime cutoff =
        QDateTime::currentDateTime().addDays(-qMax(1, retentionDays));

    for (const QString &sub :
         {QStringLiteral("history"), QStringLiteral("reports")}) {
        QDir d(QDir(stateDir()).filePath(sub));

        for (const QFileInfo &fi : d.entryInfoList(QDir::Files)) {
            if (fi.lastModified() < cutoff)
                QFile::remove(fi.absoluteFilePath());
        }
    }
}

void CrashMonitor::purgeForDiskPressure()
{
    QDir h(QDir(stateDir()).filePath(QStringLiteral("history")));
    QFileInfoList entries =
        h.entryInfoList({QStringLiteral("snapshot-*.txt")},
                        QDir::Files, QDir::Time);

    // Preserve the newest four snapshots even under disk pressure.
    for (int i = 4; i < entries.size(); ++i)
        QFile::remove(entries.at(i).absoluteFilePath());
}

// Keep a bounded rolling snapshot history rather than growing without limit.
void CrashMonitor::rotateHistory(const QByteArray &snapshot)
{
    const QString dir =
        QDir(stateDir()).filePath(QStringLiteral("history"));
    QDir().mkpath(dir);

    const QString stamp =
        QStringLiteral("uptime-") + monotonicSeconds().replace('.', '_');

    writeAtomic(
        QDir(dir).filePath(QStringLiteral("snapshot-") + stamp +
                           QStringLiteral(".txt")),
        snapshot);

    pruneHistory(18);
}

void CrashMonitor::writeJournalTail()
{
    const QString tail =
        run(QStringLiteral("journalctl"),
            {QStringLiteral("-b"), QStringLiteral("0"),
             QStringLiteral("-n"), QStringLiteral("250"),
             QStringLiteral("--no-pager"),
             QStringLiteral("-o"), QStringLiteral("short-precise")},
            7000);

    writeAtomic(
        QDir(stateDir()).filePath(QStringLiteral("journal-tail-current.txt")),
        tail.toUtf8());
}

// Classify evidence from the previous boot without claiming unsupported causation.
QString CrashMonitor::previousBootAnalysis() const
{
    const QString full =
        run(QStringLiteral("journalctl"),
            {QStringLiteral("-b"), QStringLiteral("-1"),
             QStringLiteral("-n"), QStringLiteral("600"),
             QStringLiteral("--no-pager"),
             QStringLiteral("-o"), QStringLiteral("short-precise")},
            12000);

    const QString kernel =
        run(QStringLiteral("journalctl"),
            {QStringLiteral("-b"), QStringLiteral("-1"),
             QStringLiteral("-k"), QStringLiteral("--no-pager"),
             QStringLiteral("-o"), QStringLiteral("short-precise")},
            12000);

    const QString warnings =
        run(QStringLiteral("journalctl"),
            {QStringLiteral("-b"), QStringLiteral("-1"),
             QStringLiteral("-p"), QStringLiteral("warning..alert"),
             QStringLiteral("--no-pager"),
             QStringLiteral("-o"), QStringLiteral("short-precise")},
            12000);

    struct Rule {
        QString needle;
        QString severity;
        QString label;
    };

    const QList<Rule> rules = {
        {"kernel panic", "CONFIRMED", "Kernel panic signature"},
        {"soft lockup", "CONFIRMED", "CPU soft lockup"},
        {"hard lockup", "CONFIRMED", "CPU hard lockup"},
        {"hung task", "CONFIRMED", "Hung task"},
        {"out of memory", "CONFIRMED", "Out-of-memory condition"},
        {"oom-kill", "CONFIRMED", "OOM killer activity"},
        {"mce:", "CONFIRMED", "Machine Check Exception"},
        {"hardware error", "CONFIRMED", "Hardware error"},
        {"i/o error", "CONFIRMED", "Storage I/O error"},
        {"btrfs error", "CONFIRMED", "Btrfs error"},
        {"tree-log replay", "SUSPICIOUS", "Btrfs tree-log replay / unclean-stop recovery"},
        {"pcie bus error", "CONFIRMED", "PCIe bus error"},
        {"acpi error", "SUSPICIOUS", "ACPI firmware/interpreter error"},
        {"gpu hang", "CONFIRMED", "GPU hang"},
        {"critical temperature", "CONFIRMED", "Critical temperature"},
        {"thermal thrott", "SUSPICIOUS", "Thermal throttling"},
        {"journal corrupted or uncleanly shut down", "CONFIRMED", "Unclean journal shutdown/corruption"}
    };

    const QStringList sourceLines =
        (full + QStringLiteral("\n") +
         kernel + QStringLiteral("\n") +
         warnings).split('\n');

    QStringList evidenceBlocks;

    for (const Rule &rule : rules) {
        QStringList matches;

        for (const QString &line : sourceLines) {
            if (line.contains(rule.needle, Qt::CaseInsensitive) &&
                !line.contains(QStringLiteral("CrashSentinel"),
                               Qt::CaseInsensitive)) {
                matches << line;
            }
        }

        if (!matches.isEmpty()) {
            matches.removeDuplicates();

            evidenceBlocks <<
                QStringLiteral("[%1] %2\nEvidence:\n  %3")
                    .arg(rule.severity,
                         rule.label,
                         matches.mid(0, 5).join(QStringLiteral("\n  ")));
        }
    }

    QString report;
    QTextStream out(&report);
    const MachineContext ctx = ContextProvenance::loadMachineContext();

    out << "#ju56Us\n"
        << "project_id=" << ContextProvenance::ProjectId << "\n"
        << "provenance_code=" << ContextProvenance::ProvenanceCode << "\n"
        << "context_stamp=" << contextVisibleStamp(ctx) << "\n"
        << "monotonic_seconds_since_boot=" << monotonicSeconds()
        << "\n\n";

    out << "===== EVIDENCE CLASSIFICATION =====\n";

    if (evidenceBlocks.isEmpty())
        out << "No supported crash signature detected.\n";
    else
        out << evidenceBlocks.join(QStringLiteral("\n\n")) << "\n";

    out << "\nIMPORTANT: a severe hardware, firmware, power, or kernel "
           "reset can occur before Linux flushes the cause to disk.\n";

    out << "\n===== PREVIOUS BOOT WARNINGS =====\n" << warnings;
    out << "\n===== PREVIOUS BOOT FINAL EVENTS =====\n" << full;
    out << "\n===== PREVIOUS BOOT KERNEL =====\n" << kernel;

    return report;
}

// One analysis per current boot; the boot-id marker prevents duplicate work.
void CrashMonitor::analyzePreviousBootOnce()
{
    const QString reports =
        QDir(stateDir()).filePath(QStringLiteral("reports"));
    QDir().mkpath(reports);

    const QString bootId =
        readFile(QStringLiteral("/proc/sys/kernel/random/boot_id")).trimmed();

    const QString marker =
        QDir(reports).filePath(QStringLiteral("analysis-boot-id.txt"));

    if (readFile(marker).trimmed() == bootId)
        return;

    writeAtomic(
        QDir(reports).filePath(QStringLiteral("previous-boot-analysis.txt")),
        previousBootAnalysis().toUtf8());

    writeAtomic(marker, (bootId + QStringLiteral("\n")).toUtf8());
}

void CrashMonitor::writeFinalStopLog(const QStringList &reasons)
{
    const MachineContext ctx = ContextProvenance::loadMachineContext();

    QString text;
    QTextStream out(&text);

    out << "#ju56Us\n"
        << "project_id=" << ContextProvenance::ProjectId << "\n"
        << "provenance_code=" << ContextProvenance::ProvenanceCode << "\n"
        << "context_stamp=" << contextVisibleStamp(ctx) << "\n"
        << "monotonic_seconds_since_boot=" << monotonicSeconds()
        << "\n\nCrashSentinel entered critical resource protection.\n";

    for (const QString &reason : reasons)
        out << "- " << reason << "\n";

    out << "\nThe service remains alive at a very slow interval, but "
           "normal/heavy collection is suspended until resources recover.\n";

    writeAtomic(
        QDir(stateDir()).filePath(QStringLiteral("critical-resource-stop.txt")),
        text.toUtf8());
}

// Persist measured collector cost and execution identity for the GUI/status report.
void CrashMonitor::writeServiceHealth(qint64 collectionMs,
                                      qint64 snapshotBytes,
                                      const QString &mode,
                                      const QStringList &reasons) const
{
    QJsonObject o;
    o.insert(QStringLiteral("_audit_tag"), QStringLiteral("#ju56Us"));
    o.insert(QStringLiteral("schema"), 1);
    o.insert(QStringLiteral("version"),
             QString::fromLatin1(CrashSentinelVersion::Version));
    o.insert(QStringLiteral("channel"),
             QString::fromLatin1(CrashSentinelVersion::Channel));
    o.insert(QStringLiteral("effective_user"), effectiveUserName());
    o.insert(QStringLiteral("effective_uid"), int(geteuid()));
    o.insert(QStringLiteral("collection_duration_ms"), int(collectionMs));
    o.insert(QStringLiteral("rss_mib"), currentRssMiB());
    o.insert(QStringLiteral("snapshot_kib"),
             double(snapshotBytes) / 1024.0);
    o.insert(QStringLiteral("collection_interval_seconds"),
             timer_.interval() / 1000);
    o.insert(QStringLiteral("resource_mode"), mode);
    o.insert(QStringLiteral("resource_reasons"),
             reasons.join(QStringLiteral(" | ")));
    o.insert(QStringLiteral("monotonic_seconds_since_boot"),
             monotonicSeconds());

    writeAtomic(
        QDir(stateDir()).filePath(QStringLiteral("service-health.json")),
        QJsonDocument(o).toJson(QJsonDocument::Indented));
}

// Initialize one monitor session; self-protection has already run in main.cpp.
bool CrashMonitor::start()
{
    if (!QDir().mkpath(stateDir()))
        return false;

    settings_ = CrashSettings::load();
    analyzePreviousBootOnce();
    QString deepDiagnosticsError;
    if (!DeepDiagnostics::writeReports(stateDir(), &deepDiagnosticsError)) {
        qWarning().noquote()
            << "CrashSentinel deep diagnostics warning:"
            << deepDiagnosticsError;
    }
    applyTimerInterval(settings_.snapshotIntervalSeconds);
    collect();
    timer_.start();
    return true;
}

// MAIN MONITOR LOOP: evaluate resources first, then decide how much work is safe.
void CrashMonitor::collect()
{
    QElapsedTimer collectionTimer;
    collectionTimer.start();

    settings_ = CrashSettings::load();

    const ResourceGuardState guard =
        ResourceGuard::evaluate(settings_, stateDir());

    if (guard.purgeRequested)
        purgeForDiskPressure();

    if (settings_.autoPurgeOldLogs)
        pruneByAge(settings_.logRetentionDays);

    applyTimerInterval(guard.recommendedIntervalSeconds);

    if (guard.criticalStop) {
        if (!suspendedForCriticalResource_)
            writeFinalStopLog(guard.reasons);

        suspendedForCriticalResource_ = true;

        QFile heartbeat(
            QDir(stateDir()).filePath(QStringLiteral("heartbeat.log")));

        if (heartbeat.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&heartbeat);
            out << "monotonic_seconds_since_boot=" << monotonicSeconds()
                << " #ju56Us critical-protection AKA_TE324543\n";
        }

        SelfProtection::noteHeartbeat(stateDir());

        writeServiceHealth(collectionTimer.elapsed(),
                           0,
                           QStringLiteral("critical-protection"),
                           guard.reasons);
        return;
    }

    suspendedForCriticalResource_ = false;

    const MachineContext ctx = ContextProvenance::loadMachineContext();

    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n"
        << "project_id=" << ContextProvenance::ProjectId << "\n"
        << "provenance_code=" << ContextProvenance::ProvenanceCode << "\n"
        << "context_stamp=" << contextVisibleStamp(ctx) << "\n"
        << "monotonic_seconds_since_boot=" << monotonicSeconds() << "\n"
        << "resource_mode="
        << (guard.degraded ? "degraded" : "normal") << "\n";

    if (!guard.reasons.isEmpty())
        out << "resource_reasons="
            << guard.reasons.join(QStringLiteral(" | ")) << "\n";

    out << "\n===== UPTIME =====\n"
        << run(QStringLiteral("uptime"), {});

    out << "\n===== MEMORY =====\n"
        << run(QStringLiteral("free"), {QStringLiteral("-h")});

    out << "\n===== CPU PRESSURE =====\n"
        << readFile(QStringLiteral("/proc/pressure/cpu"));

    out << "\n===== MEMORY PRESSURE =====\n"
        << readFile(QStringLiteral("/proc/pressure/memory"));

    out << "\n===== IO PRESSURE =====\n"
        << readFile(QStringLiteral("/proc/pressure/io"));

    out << "\n===== DISK SPACE =====\n"
        << run(QStringLiteral("df"), {QStringLiteral("-hT")});

    if (!(guard.degraded && settings_.pauseHeavyCollectionOnPressure)) {
        out << "\n===== FAILED SERVICES =====\n"
            << run(QStringLiteral("systemctl"),
                   {QStringLiteral("--failed"),
                    QStringLiteral("--no-pager")});

        out << "\n===== RECENT KERNEL WARNINGS =====\n"
            << run(QStringLiteral("journalctl"),
                   {QStringLiteral("-k"),
                    QStringLiteral("-p"),
                    QStringLiteral("warning..alert"),
                    QStringLiteral("-n"),
                    QStringLiteral("180"),
                    QStringLiteral("--no-pager")},
                   7000);

        out << "\n===== RECENT SYSTEM ERRORS =====\n"
            << run(QStringLiteral("journalctl"),
                   {QStringLiteral("-p"),
                    QStringLiteral("err..alert"),
                    QStringLiteral("-n"),
                    QStringLiteral("180"),
                    QStringLiteral("--no-pager")},
                   7000);

        const QString sensors =
            QStandardPaths::findExecutable(QStringLiteral("sensors"));

        if (!sensors.isEmpty())
            out << "\n===== THERMALS =====\n" << run(sensors, {});

        writeJournalTail();
    } else {
        out << "\n===== HEAVY COLLECTORS =====\n"
            << "Skipped by resource safeguard.\n";
    }

    const QByteArray bytes = report.toUtf8();

    writeAtomic(
        QDir(stateDir()).filePath(QStringLiteral("latest-snapshot.txt")),
        bytes);

    rotateHistory(bytes);

    const QString hash =
        QString::fromLatin1(
            QCryptographicHash::hash(
                bytes, QCryptographicHash::Sha256).toHex());

    QString attestationError;
    RepairAttestation::update(
        stateDir(), hash, &attestationError);

    QFile heartbeat(
        QDir(stateDir()).filePath(QStringLiteral("heartbeat.log")));

    if (heartbeat.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream hb(&heartbeat);
        hb << contextVisibleStamp(ctx)
           << " monotonic_seconds_since_boot=" << monotonicSeconds()
           << " #ju56Us alive AKA_TE324543 snapshot_sha256="
           << hash << "\n";
    }

    SelfProtection::noteHeartbeat(stateDir());

    writeServiceHealth(
        collectionTimer.elapsed(),
        bytes.size(),
        guard.degraded ? QStringLiteral("degraded")
                       : QStringLiteral("normal"),
        guard.reasons);

    qInfo().noquote()
        << "CrashSentinel service snapshot written to"
        << stateDir()
        << (guard.degraded
                ? "(protective/degraded mode)"
                : "(normal mode)");
}
