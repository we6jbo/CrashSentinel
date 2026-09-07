// #ju56Us
// READ ME: this is CrashSentinel's process entry point.
// There are three paths through main():
//   1) recovery command-line actions,
//   2) --service background monitor mode,
//   3) the normal Qt Widgets GUI.
// Service self-protection is checked BEFORE CrashMonitor::start(), so a
// quarantine or reboot hard-stop prevents heavy monitoring from beginning.

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLockFile>

#include "mainwindow.h"
#include "context_provenance.h"
#include "crash_monitor.h"
#include "self_protection.h"
#include "state_paths.h"

// Helper used by the explicit recovery command-line switches.
static bool argumentRequested(int argc,
                              char *argv[],
                              const QString &wanted)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == wanted)
            return true;
    }
    return false;
}

// Detect --service without constructing the GUI application first.
static bool serviceRequested(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--service"))
            return true;
    }
    return false;
}

// START HERE: process mode selection and service startup safety checks.
int main(int argc, char *argv[])
{

    const bool clearQuarantineRequested =
        argumentRequested(argc, argv, QStringLiteral("--clear-quarantine"));
    const bool resetRebootSafeguardRequested =
        argumentRequested(argc, argv,
                          QStringLiteral("--reset-reboot-safeguard"));

    if (clearQuarantineRequested || resetRebootSafeguardRequested) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("CrashSentinel"));
        QCoreApplication::setOrganizationName(QStringLiteral("j03.page"));

        const QString stateDir = StatePaths::sharedStateDir();
        QString error;
        bool ok = false;

        if (resetRebootSafeguardRequested) {
            ok = SelfProtection::resetRebootSafeguard(stateDir, &error);
            if (ok)
                qInfo().noquote()
                    << "CrashSentinel reboot safeguard was explicitly reset. "
                       "The abnormal reboot counter is now zero.";
        } else {
            ok = SelfProtection::clearQuarantine(stateDir, &error);
            if (ok)
                qInfo().noquote()
                    << "CrashSentinel quarantine was explicitly cleared. "
                       "If the five-reboot precautionary stop had been reached, "
                       "the abnormal reboot counter is preserved.";
        }

        if (!ok) {
            qCritical().noquote() << error;
            return 4;
        }

        return 0;
    }

    if (serviceRequested(argc, argv)) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("CrashSentinel"));
        QCoreApplication::setOrganizationName(QStringLiteral("j03.page"));

        CrashMonitor monitor;

        const QString serviceStateDir = monitor.publicStateDir();

        if (!QDir().mkpath(serviceStateDir)) {
            qCritical()
                << "CrashSentinel could not create its state directory for "
                   "single-instance protection:"
                << serviceStateDir;
            return 2;
        }

        // Defense in depth: never allow two monitor instances to collect/write at once.
        QLockFile serviceLock(
            QDir(serviceStateDir).filePath(
                QStringLiteral("service-instance.lock")));

        if (!serviceLock.tryLock(0)) {
            qint64 existingPid = 0;
            QString existingHost;
            QString existingApp;

            const bool haveOwner =
                serviceLock.getLockInfo(
                    &existingPid,
                    &existingHost,
                    &existingApp);

            if (haveOwner) {
                qCritical().noquote()
                    << QStringLiteral(
                           "CrashSentinel monitor is already running "
                           "(PID %1 on %2, application %3). "
                           "No second service instance was started.")
                           .arg(existingPid)
                           .arg(existingHost)
                           .arg(existingApp);
            } else {
                qCritical()
                    << "CrashSentinel monitor is already running, or the "
                       "service lock is currently unavailable. "
                       "No second service instance was started.";
            }

            return 3;
        }

        // IMPORTANT: quarantine/reboot guard is evaluated before heavy monitoring starts.
        const SelfProtectionState protection =
            SelfProtection::noteServiceStart(monitor.publicStateDir());

        if (protection.quarantined) {
            qCritical().noquote()
                << "CrashSentinel is self-quarantined:"
                << protection.reason;
            return 0;
        }

        QObject::connect(&app, &QCoreApplication::aboutToQuit, [&monitor] {
            SelfProtection::noteCleanExit(monitor.publicStateDir());
        });

        if (!monitor.start()) {
            qCritical() << "CrashSentinel service could not initialize its state directory.";
            return 2;
        }

        qInfo() << "CrashSentinel service mode active. No GUI will be shown.";
        return app.exec();
    }

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("CrashSentinel"));
    QCoreApplication::setOrganizationName(QStringLiteral("j03.page"));

    const QString projectRoot = QCoreApplication::applicationDirPath();

    QString snapshotError;
    ContextProvenance::ensurePortableSnapshot(projectRoot, &snapshotError);

    QString registrationDiagnostic;
    ContextProvenance::attemptRegistrationOnce(projectRoot, &registrationDiagnostic);

    const MachineContext context = ContextProvenance::loadMachineContext();

    qInfo().noquote() << ContextProvenance::provenanceSummary();
    qInfo().noquote() << ContextProvenance::userVisibleContextSummary(context);

    if (!snapshotError.isEmpty())
        qWarning().noquote() << "Portable snapshot warning:" << snapshotError;

    qInfo().noquote() << registrationDiagnostic;

    MainWindow window;
    window.show();

    return app.exec();
}
