// #ju56Us
// READ ME: deeper post-crash evidence collectors.
// Each section should say what was observed and its limitations. In particular,
// ACCESS LIMITED / UNAVAILABLE means "not checked", never "healthy".

#include "deep_diagnostics.h"
#include "state_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

// Explain why a command/file may be unavailable in this package environment.
static QString accessContext()
{
    return QStringLiteral(
        "Package mode: %1\n"
        "Unavailable evidence can be caused by Linux permissions, package "
        "confinement, a missing tool, or an unsupported kernel interface. "
        "CrashSentinel does not treat unavailable evidence as a healthy result.\n")
        .arg(StatePaths::packagingMode());
}

// Execute a diagnostic helper and preserve unavailable/denied/incomplete distinctions.
static QString runCommand(const QString &program,
                          const QStringList &args,
                          int timeoutMs = 10000)
{
    QProcess p;
    p.start(program, args);

    if (!p.waitForStarted(1500)) {
        return QStringLiteral(
                   "[UNAVAILABLE] Could not start %1: %2\n%3")
            .arg(program, p.errorString(), accessContext());
    }

    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        return QStringLiteral(
                   "[UNAVAILABLE] %1 timed out while collecting evidence.\n%2")
            .arg(program, accessContext());
    }

    QString result =
        QString::fromUtf8(p.readAllStandardOutput()) +
        QString::fromUtf8(p.readAllStandardError());

    const QString lower = result.toLower();
    const bool accessDenied =
        lower.contains(QStringLiteral("permission denied")) ||
        lower.contains(QStringLiteral("operation not permitted")) ||
        lower.contains(QStringLiteral("access denied"));

    if (accessDenied) {
        result += QStringLiteral(
            "\n[ACCESS LIMITED] The command encountered an access-denied "
            "condition. This can be normal under Snap/Flatpak confinement "
            "or when the current service lacks host privileges. This is "
            "not evidence that the checked component is healthy.\n");
        result += accessContext();
    } else if (p.exitStatus() != QProcess::NormalExit ||
               p.exitCode() != 0) {
        result += QStringLiteral(
            "\n[COMMAND INCOMPLETE] The diagnostic command exited without "
            "a clean success result. Treat this section as incomplete, not "
            "as proof of system health.\n");
        result += accessContext();
    }

    return result;
}

static QString readText(const QString &path, int maxBytes = 131072)
{
    QFile f(path);

    if (!f.open(QIODevice::ReadOnly)) {
        return QStringLiteral(
                   "[UNAVAILABLE] Could not read %1: %2\n%3")
            .arg(path, f.errorString(), accessContext());
    }

    return QString::fromUtf8(f.read(maxBytes));
}

static bool writeAtomic(const QString &path, const QString &text)
{
    QSaveFile f(path);

    if (!f.open(QIODevice::WriteOnly))
        return false;

    f.write(text.toUtf8());
    return f.commit();
}

static QString lastLines(const QString &text, int count)
{
    QStringList lines = text.split('\n', Qt::KeepEmptyParts);

    if (lines.size() > count)
        lines = lines.mid(lines.size() - count);

    return lines.join(QStringLiteral("\n"));
}

// Storage inventory plus SMART/NVMe evidence when permissions and tools allow it.
static QString storageHealthReport()
{
    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel storage health evidence\n\n";

    out << "===== BLOCK DEVICES =====\n";
    out << runCommand(
        QStringLiteral("lsblk"),
        {QStringLiteral("-d"),
         QStringLiteral("-o"),
         QStringLiteral("NAME,MODEL,SERIAL,SIZE,ROTA,TYPE")});

    out << "\n===== SMART / NVMe HEALTH =====\n";

    const QString smartctl =
        QStandardPaths::findExecutable(QStringLiteral("smartctl"));

    if (smartctl.isEmpty()) {
        out << "smartctl is not installed or not available in PATH.\n";
        out << "CrashSentinel did not infer SMART health without the tool.\n";
        return report;
    }

    const QString scan =
        runCommand(
            smartctl,
            {QStringLiteral("--scan-open")},
            8000);

    out << "Device scan:\n" << scan << '\n';

    int devices = 0;

    for (const QString &line :
         scan.split('\n', Qt::SkipEmptyParts)) {
        const QString device =
            line.section(' ', 0, 0).trimmed();

        if (!device.startsWith(QStringLiteral("/dev/")))
            continue;

        if (++devices > 8) {
            out << "\n[additional devices omitted]\n";
            break;
        }

        out << "\n===== " << device << " =====\n";

        out << runCommand(
                   smartctl,
                   {QStringLiteral("-H"),
                    QStringLiteral("-A"),
                    device},
                   10000)
                   .left(65536);
    }

    return report;
}

// Search specifically for MCE/machine-check/EDAC evidence; absence is not proof of health.
static QString memoryHardwareEvidence()
{
    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel MCE / EDAC / memory hardware evidence\n\n";

    const QString journal =
        runCommand(
            QStringLiteral("journalctl"),
            {QStringLiteral("-b"),
             QStringLiteral("-1"),
             QStringLiteral("-k"),
             QStringLiteral("--no-pager"),
             QStringLiteral("-o"),
             QStringLiteral("short-monotonic")},
            12000);

    QStringList matches;

    for (const QString &line :
         journal.split('\n', Qt::SkipEmptyParts)) {
        const QString lower = line.toLower();

        const bool mce =
            lower.contains(QStringLiteral("mce:")) ||
            lower.contains(QStringLiteral("machine check")) ||
            lower.contains(QStringLiteral("hardware error"));

        const bool specificEdac =
            lower.contains(QStringLiteral("edac")) &&
            (lower.contains(QStringLiteral("corrected")) ||
             lower.contains(QStringLiteral("uncorrected")) ||
             lower.contains(QStringLiteral(" error")) ||
             lower.contains(QStringLiteral(" ce ")) ||
             lower.contains(QStringLiteral(" ue ")));

        if (mce || specificEdac)
            matches << line;
    }

    matches.removeDuplicates();

    if (matches.isEmpty()) {
        out << "No specific MCE, Machine Check, hardware-error, or "
               "corrected/uncorrected EDAC memory-error evidence was "
               "found in the captured previous-boot kernel journal.\n";
        out << "Absence of recorded evidence does not prove hardware is healthy.\n";
    } else {
        out << matches.mid(0, 200).join(QStringLiteral("\n")) << '\n';
    }

    return report;
}

// Read platform/DMI/kernel inventory useful when comparing firmware-related crashes.
static QString firmwareInventory()
{
    struct Entry {
        const char *label;
        const char *path;
    };

    const Entry entries[] = {
        {"System vendor", "/sys/class/dmi/id/sys_vendor"},
        {"Product name", "/sys/class/dmi/id/product_name"},
        {"Product version", "/sys/class/dmi/id/product_version"},
        {"Board vendor", "/sys/class/dmi/id/board_vendor"},
        {"Board name", "/sys/class/dmi/id/board_name"},
        {"BIOS vendor", "/sys/class/dmi/id/bios_vendor"},
        {"BIOS version", "/sys/class/dmi/id/bios_version"},
        {"BIOS date", "/sys/class/dmi/id/bios_date"}
    };

    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel firmware / platform inventory\n\n";

    for (const Entry &entry : entries) {
        const QString value =
            readText(QString::fromLatin1(entry.path), 4096)
                .trimmed();

        out << entry.label << ": "
            << (value.isEmpty()
                    ? QStringLiteral("[unavailable]")
                    : value)
            << '\n';
    }

    out << "\n===== KERNEL =====\n";
    out << runCommand(
        QStringLiteral("uname"),
        {QStringLiteral("-a")});

    return report;
}

// Summarize userspace coredumps; these cannot explain every whole-system reset.
static QString coredumpSummary()
{
    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel coredump summary\n\n";

    const QString tool =
        QStandardPaths::findExecutable(
            QStringLiteral("coredumpctl"));

    if (tool.isEmpty()) {
        out << "coredumpctl is unavailable.\n";
        return report;
    }

    const QString all =
        runCommand(
            tool,
            {QStringLiteral("--no-pager"),
             QStringLiteral("list")},
            12000);

    out << "Most recent captured entries (up to 80 lines):\n";
    out << lastLines(all, 80) << '\n';

    return report;
}

// Capture the end of the previous journal to look for the last persisted events.
static QString previousBootFinalTail()
{
    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel previous-boot final journal tail\n\n";

    out << runCommand(
        QStringLiteral("journalctl"),
        {QStringLiteral("-b"),
         QStringLiteral("-1"),
         QStringLiteral("-n"),
         QStringLiteral("250"),
         QStringLiteral("--no-pager"),
         QStringLiteral("-o"),
         QStringLiteral("short-monotonic")},
        12000);

    return report;
}

// Look for orderly shutdown markers; missing markers support investigation, not proof.
static QString cleanShutdownEvidence(const QString &tail)
{
    const QStringList markers = {
        QStringLiteral("Reached target System Power Off"),
        QStringLiteral("Reached target Reboot"),
        QStringLiteral("systemd-shutdown"),
        QStringLiteral("Powering Off"),
        QStringLiteral("Rebooting"),
        QStringLiteral("reboot: Power down"),
        QStringLiteral("reboot: Restarting system")
    };

    QStringList matches;

    for (const QString &line :
         tail.split('\n', Qt::SkipEmptyParts)) {
        for (const QString &marker : markers) {
            if (line.contains(marker, Qt::CaseInsensitive)) {
                matches << line;
                break;
            }
        }
    }

    matches.removeDuplicates();

    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel clean-shutdown evidence check\n\n";

    if (matches.isEmpty()) {
        out << "No recognized orderly shutdown marker was found in the "
               "captured previous-boot final journal tail.\n";
        out << "This supports investigating an abrupt stop, but it is not "
               "proof because journal data can be incomplete or lost.\n";
    } else {
        out << "Recognized orderly shutdown/reboot markers were found:\n\n";
        out << matches.join(QStringLiteral("\n")) << '\n';
    }

    return report;
}

// Distinguish absent, permission-limited, readable-empty, and actual pstore records.
static QString pstoreEvidence()
{
    const QString path = QStringLiteral("/sys/fs/pstore");
    QFileInfo pathInfo(path);

    QString report;
    QTextStream out(&report);

    out << "#ju56Us\n";
    out << "CrashSentinel pstore / ramoops evidence\n\n";

    if (!pathInfo.exists()) {
        out << "[UNAVAILABLE] /sys/fs/pstore is not present on this system.\n";
        out << "No conclusion about crash health can be drawn from an unavailable "
               "pstore interface.\n";
        return report;
    }

    if (!pathInfo.isDir()) {
        out << "[UNAVAILABLE] /sys/fs/pstore exists but is not accessible as a "
               "directory.\n";
        out << "This is not evidence that no crash record exists.\n";
        return report;
    }

    QDir dir(path);

    const QStringList allFiles =
        dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    const QFileInfoList readableFiles =
        dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);

    if (!pathInfo.isReadable()) {
        out << "[ACCESS LIMITED] /sys/fs/pstore exists, but CrashSentinel "
               "does not have permission to read it.\n";
        out << "A kernel crash record could be present but hidden from this "
               "process. This is not evidence that pstore is empty or that "
               "the system is healthy.\n";
        out << StatePaths::stateExplanation() << '\n';
        return report;
    }

    if (allFiles.isEmpty()) {
        out << "The pstore directory is readable and currently contains no "
               "crash-record files.\n";
        out << "An empty pstore does not prove that no hardware, firmware, "
               "power, or kernel failure occurred.\n";
        return report;
    }

    if (readableFiles.isEmpty()) {
        out << "[ACCESS LIMITED] pstore contains one or more crash-record "
               "files, but none are readable by CrashSentinel.\n";
        out << "A crash record may exist but be unavailable because of Linux "
               "permissions or package confinement. This is not a healthy "
               "result.\n";
        out << StatePaths::stateExplanation() << '\n';
        return report;
    }

    int count = 0;
    for (const QFileInfo &fi : readableFiles) {
        if (++count > 16) {
            out << "\n[additional pstore files omitted]\n";
            break;
        }

        out << "\n===== " << fi.fileName() << " =====\n";
        out << readText(fi.absoluteFilePath(), 65536);
    }

    return report;
}

// Generate individual reports first, then combine them into one comprehensive report.
bool DeepDiagnostics::writeReports(const QString &stateDir,
                                   QString *error)
{
    const QString reportsDir =
        QDir(stateDir).filePath(
            QStringLiteral("reports"));

    if (!QDir().mkpath(reportsDir)) {
        if (error)
            *error =
                QStringLiteral(
                    "Could not create diagnostics report directory.");
        return false;
    }

    const QString storage = storageHealthReport();
    const QString memory = memoryHardwareEvidence();
    const QString firmware = firmwareInventory();
    const QString coredumps = coredumpSummary();
    const QString finalTail = previousBootFinalTail();
    const QString shutdown = cleanShutdownEvidence(finalTail);
    const QString pstore = pstoreEvidence();

    struct Output {
        const char *name;
        const QString *text;
    };

    const Output outputs[] = {
        {"storage-health.txt", &storage},
        {"memory-hardware-evidence.txt", &memory},
        {"firmware-inventory.txt", &firmware},
        {"coredump-summary.txt", &coredumps},
        {"previous-boot-final-tail.txt", &finalTail},
        {"clean-shutdown-evidence.txt", &shutdown},
        {"pstore-evidence.txt", &pstore}
    };

    for (const Output &output : outputs) {
        if (!writeAtomic(
                QDir(reportsDir).filePath(
                    QString::fromLatin1(output.name)),
                *output.text)) {
            if (error)
                *error =
                    QStringLiteral("Could not write %1")
                        .arg(QString::fromLatin1(output.name));
            return false;
        }
    }

    QString combined;
    QTextStream out(&combined);

    out << "#ju56Us\n";
    out << "CrashSentinel comprehensive diagnostics\n";
    out << "These are captured observations, not a guarantee of root cause.\n";
    out << "Package mode: " << StatePaths::packagingMode() << "\n";
    out << StatePaths::stateExplanation() << "\n";

    for (const Output &output : outputs) {
        out << "\n\n============================================================\n";
        out << output.name << '\n';
        out << "============================================================\n";
        out << *output.text;
    }

    if (!writeAtomic(
            QDir(reportsDir).filePath(
                QStringLiteral("comprehensive-diagnostics.txt")),
            combined)) {
        if (error)
            *error =
                QStringLiteral(
                    "Could not write comprehensive diagnostics.");
        return false;
    }

    return true;
}
