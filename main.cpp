// #ju56Us
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>

#include "mainwindow.h"
#include "context_provenance.h"
#include "crash_monitor.h"

static bool serviceRequested(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--service"))
            return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    if (serviceRequested(argc, argv)) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("CrashSentinel"));
        QCoreApplication::setOrganizationName(QStringLiteral("j03.page"));

        CrashMonitor monitor;
        if (!monitor.start()) {
            qCritical() << "CrashSentinel service could not initialize its state directory.";
            return 2;
        }

        qInfo() << "CrashSentinel service mode active. No GUI will be shown.";
        return app.exec();
    }

    QApplication app(argc, argv);

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
