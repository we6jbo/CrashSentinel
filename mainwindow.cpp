// #ju56Us
// READ ME: this file builds almost all of the visible CrashSentinel interface.
// Qt Designer shows only the minimal mainwindow.ui placeholder; the runtime
// six-tab interface is assembled below with QWidget/QLayout objects.
// To change a tab, find its build...Tab() function. To change tab order, edit
// START HERE FOR THE INTERFACE: creates the six tabs and controls their order.

#include "mainwindow.h"

#include "crash_settings.h"
#include "resource_guard.h"
#include "self_protection.h"
#include "recovery_diagnostics.h"
#include "publishing_status.h"
#include "status_snapshot.h"
#include "update_checker.h"
#include "version_info.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QScrollBar>
#include <QCoreApplication>
#include <QTimer>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("CrashSentinel"));
    resize(900, 700);

    updateInstructionsUrl_ =
        QString::fromLatin1(CrashSentinelVersion::UpdateAnchor);

    updateChecker_ = new UpdateChecker(this);

    connect(updateChecker_, &UpdateChecker::finished,
            this, [this](const UpdateResult &result) {
        updateLabel_->setText(result.message);

        if (!result.instructionsUrl.isEmpty())
            updateInstructionsUrl_ = result.instructionsUrl;

        openUpdateButton_->setEnabled(
            !updateInstructionsUrl_.isEmpty());

        checkUpdatesButton_->setEnabled(true);
    });

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);

    scrollDiscoveryHint_ = new QLabel(
        QStringLiteral(
            "<b>You haven't scrolled down in CrashSentinel yet.</b> "
            "Some tabs have more options below. Scroll down to see them. "
            "This reminder will disappear permanently after you scroll once."));
    scrollDiscoveryHint_->setWordWrap(true);
    scrollDiscoveryHint_->setVisible(false);

    tabs_ = new QTabWidget(this);

    publishingTabPage_ = makeScrollable(buildPublishingTab());

    if (!PublishingStatusBuilder::isHiddenByUser())
        tabs_->addTab(
            publishingTabPage_,
            QStringLiteral("Install & Publish"));

    tabs_->addTab(makeScrollable(buildStatusTab()),
                  QStringLiteral("Status"));
    tabs_->addTab(makeScrollable(buildSafeguardsTab()),
                  QStringLiteral("Safeguards"));
    tabs_->addTab(makeScrollable(buildProfilesTab()),
                  QStringLiteral("Profiles"));
    tabs_->addTab(makeScrollable(buildReportsTab()),
                  QStringLiteral("Reports"));
    tabs_->addTab(makeScrollable(buildRecoveryDiagnosticsTab()),
                  QStringLiteral("Recovery & Diagnostics"));

    centralLayout->addWidget(scrollDiscoveryHint_);
    centralLayout->addWidget(tabs_);
    setCentralWidget(central);

    QSettings discoverySettings;
    scrollDiscovered_ =
        discoverySettings.value(
            QStringLiteral("ui/scroll_discovered"),
            false).toBool();

    scrollDiscoveryUsageSeconds_ =
        discoverySettings.value(
            QStringLiteral("ui/scroll_discovery_usage_seconds"),
            0).toInt();

    scrollDiscoveryTimer_ = new QTimer(this);
    scrollDiscoveryTimer_->setInterval(1000);

    connect(scrollDiscoveryTimer_, &QTimer::timeout,
            this, &MainWindow::tickScrollDiscoveryUsage);

    if (!scrollDiscovered_)
        scrollDiscoveryTimer_->start();

    loadSettingsIntoUi();
    refreshStatus();
}

// Wrap every tab in a scroll area so smaller screens can reach all controls.
QWidget *MainWindow::makeScrollable(QWidget *content)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);

    connect(scroll->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            [this](int value) {
                if (value != 0)
                    noteScrollDiscovered();
            });

    return scroll;
}

// TAB 1: Install & Publish. Local readiness checks only.
QWidget *MainWindow::buildPublishingTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *title =
        new QLabel(
            QStringLiteral("<h2>Install &amp; Publish</h2>"));
    title->setWordWrap(true);

    auto *intro =
        new QLabel(
            QStringLiteral(
                "This tab verifies what CrashSentinel can confirm "
                "locally for GitHub, Snap, and Flatpak/Flathub. "
                "External publication or store review is never "
                "assumed merely because local files exist."));
    intro->setWordWrap(true);

    publishingSummary_ = new QLabel;
    publishingSummary_->setWordWrap(true);

    publishingDetails_ = new QPlainTextEdit;
    publishingDetails_->setReadOnly(true);
    publishingDetails_->setMinimumHeight(360);

    auto *verify =
        new QPushButton(
            QStringLiteral(
                "Verify Installation & Publishing Steps"));

    hidePublishingButton_ =
        new QPushButton(
            QStringLiteral(
                "Hide Install & Publish After Setup"));

    connect(verify, &QPushButton::clicked,
            this, &MainWindow::verifyPublishingSetup);

    connect(hidePublishingButton_, &QPushButton::clicked,
            this, &MainWindow::hidePublishingTab);

    v->addWidget(title);
    v->addWidget(intro);
    v->addWidget(publishingSummary_);
    v->addWidget(publishingDetails_);
    v->addWidget(verify);
    v->addWidget(hidePublishingButton_);
    v->addStretch();

    QTimer::singleShot(
        0, this, &MainWindow::verifyPublishingSetup);

    return w;
}

// TAB 2: Status. Current package/service/resource/evidence summary.
QWidget *MainWindow::buildStatusTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *title =
        new QLabel(QStringLiteral("<h2>CrashSentinel Status</h2>"));
    title->setWordWrap(true);

    versionLabel_ = new QLabel;
    versionLabel_->setWordWrap(true);

    statusLabel_ = new QLabel;
    statusLabel_->setWordWrap(true);

    statusDetails_ = new QPlainTextEdit;
    statusDetails_->setReadOnly(true);
    statusDetails_->setMinimumHeight(300);

    configPathLabel_ = new QLabel;
    configPathLabel_->setWordWrap(true);

    updateLabel_ =
        new QLabel(QStringLiteral("Update check not run."));
    updateLabel_->setWordWrap(true);

    auto *refresh =
        new QPushButton(QStringLiteral("Refresh Status"));

    checkUpdatesButton_ =
        new QPushButton(QStringLiteral("Check for Updates"));

    openUpdateButton_ =
        new QPushButton(QStringLiteral("Open Update Instructions"));

    clearQuarantineButton_ =
        new QPushButton(QStringLiteral("Clear Self-Quarantine"));

    connect(refresh, &QPushButton::clicked,
            this, &MainWindow::refreshStatus);

    connect(checkUpdatesButton_, &QPushButton::clicked,
            this, &MainWindow::checkForUpdates);

    connect(openUpdateButton_, &QPushButton::clicked,
            this, &MainWindow::openUpdateInstructions);

    connect(clearQuarantineButton_, &QPushButton::clicked,
            this, &MainWindow::clearQuarantine);

    v->addWidget(title);
    v->addWidget(versionLabel_);
    v->addWidget(statusLabel_);
    v->addWidget(statusDetails_);
    v->addWidget(configPathLabel_);
    v->addWidget(updateLabel_);
    v->addWidget(refresh);
    v->addWidget(checkUpdatesButton_);
    v->addWidget(openUpdateButton_);
    v->addWidget(clearQuarantineButton_);
    v->addStretch();

    return w;
}

// TAB 3: Safeguards. User preferences plus non-weakenable hard limits.
QWidget *MainWindow::buildSafeguardsTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *hard =
        new QPlainTextEdit(ResourceGuard::hardcodedPolicyText());
    hard->setReadOnly(true);
    hard->setMinimumHeight(220);

    auto *formBox =
        new QGroupBox(QStringLiteral("User-configurable safeguards"));
    auto *form = new QFormLayout(formBox);

    auto spin = [](int min, int max) {
        auto *s = new QSpinBox;
        s->setRange(min, max);
        return s;
    };

    diskGiB_ = spin(1, 1024);
    diskUsedPct_ = spin(20, 99);
    memoryMiB_ = spin(512, 131072);
    batteryWarn_ = spin(10, 100);
    batteryCritical_ = spin(5, 50);
    cpuPressure_ = spin(1, 100);
    ioPressure_ = spin(1, 100);
    temperatureC_ = spin(50, 95);
    retentionDays_ = spin(1, 365);
    intervalSeconds_ = spin(20, 600);

    form->addRow(
        QStringLiteral("Keep at least this disk free (GiB):"),
        diskGiB_);
    form->addRow(
        QStringLiteral("Degrade if disk used exceeds (%):"),
        diskUsedPct_);
    form->addRow(
        QStringLiteral("Reserve memory for other apps (MiB):"),
        memoryMiB_);
    form->addRow(
        QStringLiteral("Battery warning reserve (%):"),
        batteryWarn_);
    form->addRow(
        QStringLiteral("User critical battery (%):"),
        batteryCritical_);
    form->addRow(
        QStringLiteral("CPU pressure avg10 limit (%):"),
        cpuPressure_);
    form->addRow(
        QStringLiteral("I/O pressure avg10 limit (%):"),
        ioPressure_);
    form->addRow(
        QStringLiteral("Temperature limit (C):"),
        temperatureC_);
    form->addRow(
        QStringLiteral("Log retention (days):"),
        retentionDays_);
    form->addRow(
        QStringLiteral("Normal snapshot interval (seconds):"),
        intervalSeconds_);

    pauseHeavy_ = new QCheckBox(
        QStringLiteral(
            "Pause heavy collectors when resources are pressured"));

    autoPurge_ = new QCheckBox(
        QStringLiteral(
            "Automatically purge old CrashSentinel history when disk is pressured"));

    auto *save =
        new QPushButton(QStringLiteral("Save Settings"));

    connect(save, &QPushButton::clicked,
            this, &MainWindow::saveSettings);

    v->addWidget(new QLabel(
        QStringLiteral(
            "<b>Hardcoded safeguards cannot be weakened by user settings.</b>")));
    v->addWidget(hard);
    v->addWidget(formBox);
    v->addWidget(pauseHeavy_);
    v->addWidget(autoPurge_);
    v->addWidget(save);
    v->addStretch();

    return w;
}

// TAB 4: Profiles. Convenience presets, not hardware-equivalence claims.
QWidget *MainWindow::buildProfilesTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    gamingProfile_ =
        new QCheckBox(
            QStringLiteral("Gaming / performance-sensitive computer"));

    auto *balanced =
        new QPushButton(QStringLiteral("Apply Balanced Defaults"));

    auto *gaming =
        new QPushButton(QStringLiteral("Apply Gaming Defaults"));

    connect(balanced, &QPushButton::clicked,
            this, &MainWindow::applyBalancedDefaults);

    connect(gaming, &QPushButton::clicked,
            this, &MainWindow::applyGamingDefaults);

    auto *note = new QLabel(
        QStringLiteral(
            "CrashSentinel does not claim equivalence to a specific CPU "
            "or GPU model. The gaming profile instead preserves measurable "
            "headroom: available memory, CPU/I/O pressure, disk reserve, "
            "battery reserve, and reduced monitoring frequency under load."));

    note->setWordWrap(true);

    v->addWidget(gamingProfile_);
    v->addWidget(note);
    v->addWidget(balanced);
    v->addWidget(gaming);
    v->addStretch();

    return w;
}

// TAB 6: Recovery & Diagnostics. Lightweight recovery even during quarantine.
QWidget *MainWindow::buildRecoveryDiagnosticsTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *title =
        new QLabel(QStringLiteral("<h2>Recovery &amp; Diagnostics</h2>"));
    title->setWordWrap(true);

    auto *intro =
        new QLabel(QStringLiteral(
            "This tab helps recover from unusual CrashSentinel conditions "
            "without requiring terminal or state-file surgery. Recovery "
            "functions remain lightweight even when heavy monitoring is "
            "self-quarantined."));
    intro->setWordWrap(true);

    recoverySummary_ = new QLabel;
    recoverySummary_->setWordWrap(true);

    recoveryDetails_ = new QPlainTextEdit;
    recoveryDetails_->setReadOnly(true);
    recoveryDetails_->setMinimumHeight(360);

    auto *refresh =
        new QPushButton(QStringLiteral("Show What Is Going On"));
    auto *backup =
        new QPushButton(QStringLiteral("Create Backup Before Repair"));

    clearRecoveryQuarantineButton_ =
        new QPushButton(QStringLiteral("Clear Self-Quarantine"));

    rebuildRecoveryReportsButton_ =
        new QPushButton(QStringLiteral("Rebuild Derived Reports"));

    auto *exportButton =
        new QPushButton(QStringLiteral("Export Diagnostic Report"));
    auto *showPublishing =
        new QPushButton(
            QStringLiteral("Show Install & Publish Tab Again"));

    connect(refresh, &QPushButton::clicked,
            this, &MainWindow::refreshRecoveryDiagnostics);
    connect(backup, &QPushButton::clicked,
            this, &MainWindow::backupBeforeRecovery);
    connect(clearRecoveryQuarantineButton_, &QPushButton::clicked,
            this, &MainWindow::clearRecoveryQuarantine);
    connect(rebuildRecoveryReportsButton_, &QPushButton::clicked,
            this, &MainWindow::rebuildRecoveryReports);
    connect(exportButton, &QPushButton::clicked,
            this, &MainWindow::exportRecoveryDiagnostics);
    connect(showPublishing, &QPushButton::clicked,
            this, &MainWindow::showPublishingTab);

    v->addWidget(title);
    v->addWidget(intro);
    v->addWidget(recoverySummary_);
    v->addWidget(recoveryDetails_);
    v->addWidget(refresh);
    v->addWidget(backup);
    v->addWidget(clearRecoveryQuarantineButton_);
    v->addWidget(rebuildRecoveryReportsButton_);
    v->addWidget(exportButton);
    v->addWidget(showPublishing);
    v->addStretch();

    QTimer::singleShot(0, this, &MainWindow::refreshRecoveryDiagnostics);
    return w;
}

// TAB 5: Reports. Shows the stored previous-boot analysis.
QWidget *MainWindow::buildReportsTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    reportView_ = new QPlainTextEdit;
    reportView_->setReadOnly(true);

    auto *refresh =
        new QPushButton(
            QStringLiteral("Reload Previous-Boot Analysis"));

    connect(refresh, &QPushButton::clicked,
            this, &MainWindow::refreshStatus);

    v->addWidget(reportView_);
    v->addWidget(refresh);

    return w;
}

// Copy persisted settings into the visible controls.
void MainWindow::loadSettingsIntoUi()
{
    const CrashSettings s = CrashSettings::load();

    diskGiB_->setValue(s.minDiskFreeGiB);
    diskUsedPct_->setValue(s.maxDiskUsedPercent);
    memoryMiB_->setValue(s.minMemoryAvailableMiB);
    batteryWarn_->setValue(s.warningBatteryPercent);
    batteryCritical_->setValue(s.criticalBatteryPercent);
    cpuPressure_->setValue(s.maxCpuPressureSomeAvg10);
    ioPressure_->setValue(s.maxIoPressureSomeAvg10);
    temperatureC_->setValue(s.maxTemperatureC);
    retentionDays_->setValue(s.logRetentionDays);
    intervalSeconds_->setValue(s.snapshotIntervalSeconds);

    gamingProfile_->setChecked(s.gamingProfile);
    pauseHeavy_->setChecked(s.pauseHeavyCollectionOnPressure);
    autoPurge_->setChecked(s.autoPurgeOldLogs);
}

// Copy visible controls back to persistent settings, then refresh status.
void MainWindow::saveSettings()
{
    CrashSettings s;

    s.minDiskFreeGiB = diskGiB_->value();
    s.maxDiskUsedPercent = diskUsedPct_->value();
    s.minMemoryAvailableMiB = memoryMiB_->value();
    s.warningBatteryPercent = batteryWarn_->value();
    s.criticalBatteryPercent = batteryCritical_->value();
    s.maxCpuPressureSomeAvg10 = cpuPressure_->value();
    s.maxIoPressureSomeAvg10 = ioPressure_->value();
    s.maxTemperatureC = temperatureC_->value();
    s.logRetentionDays = retentionDays_->value();
    s.snapshotIntervalSeconds = intervalSeconds_->value();

    s.gamingProfile = gamingProfile_->isChecked();
    s.pauseHeavyCollectionOnPressure =
        pauseHeavy_->isChecked();
    s.autoPurgeOldLogs = autoPurge_->isChecked();

    QString error;

    statusLabel_->setText(
        s.save(&error)
            ? QStringLiteral("<b>Settings saved.</b>")
            : QStringLiteral("<b>Settings save failed:</b> %1")
                  .arg(error.toHtmlEscaped()));

    refreshStatus();
}

void MainWindow::applyBalancedDefaults()
{
    diskGiB_->setValue(10);
    diskUsedPct_->setValue(75);
    memoryMiB_->setValue(4096);
    batteryWarn_->setValue(25);
    batteryCritical_->setValue(8);
    cpuPressure_->setValue(35);
    ioPressure_->setValue(25);
    temperatureC_->setValue(85);
    intervalSeconds_->setValue(20);
    retentionDays_->setValue(7);
    gamingProfile_->setChecked(false);
    pauseHeavy_->setChecked(true);
    autoPurge_->setChecked(true);
}

void MainWindow::applyGamingDefaults()
{
    diskGiB_->setValue(10);
    diskUsedPct_->setValue(75);
    memoryMiB_->setValue(6144);
    batteryWarn_->setValue(35);
    batteryCritical_->setValue(8);
    cpuPressure_->setValue(15);
    ioPressure_->setValue(15);
    temperatureC_->setValue(82);
    intervalSeconds_->setValue(60);
    retentionDays_->setValue(7);
    gamingProfile_->setChecked(true);
    pauseHeavy_->setChecked(true);
    autoPurge_->setChecked(true);
}

// Build the plain-language status/details text shown to the user.
QString MainWindow::statusText() const
{
    const StatusSnapshot status =
        StatusSnapshotBuilder::build();

    QString text;
    QTextStream out(&text);

    out << "Version: " << status.version << "\n";
    out << "Channel: " << status.channel << "\n";
    out << "Package mode: " << status.packageType << "\n";
    out << "GUI user: " << status.guiUser << "\n";
    out << "Monitoring daemon reported user: "
        << status.daemonEvidenceUser << "\n";
    out << "Service evidence: "
        << status.serviceState << "\n";
    out << "Self-protection: "
        << status.selfProtection << "\n";
    out << "State directory: "
        << status.stateDir << "\n\n";

    out << "Measured impact:\n"
        << status.impactSummary << "\n\n";

    out << "Previous crash evidence:\n"
        << status.crashSummary << "\n\n";

    out << "What the user can try next:\n"
        << "• Inspect SMART/storage health.\n"
        << "• Inspect pstore if supported.\n"
        << "• Inspect coredump summaries.\n"
        << "• Run a memory diagnostic if crashes continue.\n"
        << "• Compare behavior with the installed LTS kernel.\n"
        << "• Review firmware/BIOS and thermal evidence.\n\n"
        << "CrashSentinel should suggest tests before making "
           "automatic repairs.";

    return text;
}

// Re-read current state and repaint status/report widgets.
void MainWindow::refreshStatus()
{
    const CrashSettings settings = CrashSettings::load();

    QString stateDir =
        QStandardPaths::writableLocation(
            QStandardPaths::StateLocation);

    if (stateDir.isEmpty())
        stateDir =
            QDir::homePath() +
            QStringLiteral("/.local/state/CrashSentinel");

    const ResourceGuardState guard =
        ResourceGuard::evaluate(settings, stateDir);

    versionLabel_->setText(
        QStringLiteral("<b>CrashSentinel %1</b> — %2 channel")
            .arg(
                QString::fromLatin1(
                    CrashSentinelVersion::Version),
                QString::fromLatin1(
                    CrashSentinelVersion::Channel)));

    QString html =
        QStringLiteral(
            "<b>Resource mode:</b> %1<br>"
            "<b>Recommended collection interval:</b> %2 seconds")
            .arg(
                guard.criticalStop
                    ? QStringLiteral("critical-stop protection")
                    : (guard.degraded
                           ? QStringLiteral("degraded/protective")
                           : QStringLiteral("normal")))
            .arg(guard.recommendedIntervalSeconds);

    if (!guard.reasons.isEmpty()) {
        html +=
            QStringLiteral("<br><b>Reasons:</b><br>• ") +
            guard.reasons.join(
                QStringLiteral("<br>• "));
    }

    statusLabel_->setText(html);
    statusDetails_->setPlainText(statusText());

    configPathLabel_->setText(
        QStringLiteral("<b>Settings file:</b> %1")
            .arg(CrashSettings::configPath()));

    const SelfProtectionState protection =
        SelfProtection::readState(stateDir);

    clearQuarantineButton_->setEnabled(
        protection.quarantined);

    if (reportView_) {
        QFile f(
            QDir(stateDir).filePath(
                QStringLiteral(
                    "reports/previous-boot-analysis.txt")));

        if (f.open(QIODevice::ReadOnly)) {
            reportView_->setPlainText(
                QString::fromUtf8(f.readAll()));
        } else {
            reportView_->setPlainText(
                QStringLiteral(
                    "No previous-boot analysis is available yet."));
        }
    }
}

void MainWindow::checkForUpdates()
{
    checkUpdatesButton_->setEnabled(false);
    updateLabel_->setText(
        QStringLiteral("Checking for updates..."));
    updateChecker_->check();
}

void MainWindow::openUpdateInstructions()
{
    if (!updateInstructionsUrl_.isEmpty())
        QDesktopServices::openUrl(
            QUrl(updateInstructionsUrl_));
}

// User-requested normal quarantine clear. A 20-reboot hard stop must still refuse.
void MainWindow::clearQuarantine()
{
    const QString stateDir =
        StatusSnapshotBuilder::defaultStateDir();

    QString error;

    if (SelfProtection::clearQuarantine(
            stateDir, &error)) {
        updateLabel_->setText(
            QStringLiteral(
                "CrashSentinel self-quarantine cleared."));
    } else {
        updateLabel_->setText(
            QStringLiteral(
                "Could not clear self-quarantine: %1")
                .arg(error));
    }

    refreshStatus();
}


void MainWindow::refreshRecoveryDiagnostics()
{
    const QString stateDir =
        QStandardPaths::writableLocation(QStandardPaths::StateLocation);

    const RecoveryDiagnosticsStatus status =
        RecoveryDiagnostics::inspect(stateDir);

    recoverySummary_->setText(
        QStringLiteral("<b>%1</b>")
            .arg(status.summary.toHtmlEscaped()));

    recoveryDetails_->setPlainText(status.details);
    clearRecoveryQuarantineButton_->setEnabled(status.quarantined);
    rebuildRecoveryReportsButton_->setEnabled(
        status.previousBootReportExists || status.analysisMarkerExists);
}

void MainWindow::backupBeforeRecovery()
{
    const QString stateDir =
        QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    QString error;
    const QString path =
        RecoveryDiagnostics::createBackup(stateDir, &error);

    if (path.isEmpty()) {
        recoverySummary_->setText(
            QStringLiteral("<b>Backup failed:</b> %1")
                .arg(error.toHtmlEscaped()));
        return;
    }

    recoverySummary_->setText(
        QStringLiteral("<b>Recovery backup created:</b> %1")
            .arg(path.toHtmlEscaped()));
}

void MainWindow::clearRecoveryQuarantine()
{
    const QString stateDir =
        QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    QString error;

    if (!RecoveryDiagnostics::clearSelfQuarantine(stateDir, &error)) {
        recoverySummary_->setText(
            QStringLiteral("<b>Could not clear quarantine:</b> %1")
                .arg(error.toHtmlEscaped()));
        return;
    }

    refreshRecoveryDiagnostics();
}

void MainWindow::rebuildRecoveryReports()
{
    const QString stateDir =
        QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    QString error;

    if (!RecoveryDiagnostics::rebuildDerivedReportMarker(stateDir, &error)) {
        recoverySummary_->setText(
            QStringLiteral("<b>Could not prepare report rebuild:</b> %1")
                .arg(error.toHtmlEscaped()));
        return;
    }

    recoverySummary_->setText(
        QStringLiteral("<b>Derived-report marker cleared.</b> "
                       "Restart the monitor service when you want the "
                       "previous-boot analysis regenerated."));
}

void MainWindow::exportRecoveryDiagnostics()
{
    const QString stateDir =
        QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    QString error;
    const QString path =
        RecoveryDiagnostics::exportDiagnosticReport(stateDir, &error);

    if (path.isEmpty()) {
        recoverySummary_->setText(
            QStringLiteral("<b>Diagnostic export failed:</b> %1")
                .arg(error.toHtmlEscaped()));
        return;
    }

    recoverySummary_->setText(
        QStringLiteral("<b>Diagnostic report exported:</b> %1")
            .arg(path.toHtmlEscaped()));
}


// Re-run LOCAL publishing prerequisites; external publication is verified elsewhere.
void MainWindow::verifyPublishingSetup()
{
    QString root =
        qEnvironmentVariable(
            "CRASHSENTINEL_PROJECT_ROOT");

    if (root.isEmpty()) {
        QDir d(
            QCoreApplication::applicationDirPath());

        for (int i = 0; i < 7; ++i) {
            if (QFile::exists(d.filePath("CMakeLists.txt")) &&
                QFile::exists(d.filePath("main.cpp"))) {
                root = d.absolutePath();
                break;
            }

            if (!d.cdUp())
                break;
        }
    }

    const PublishingStatus status =
        PublishingStatusBuilder::detect(root);

    publishingDetails_->setPlainText(status.details);

    hidePublishingButton_->setEnabled(
        status.allLocalChecksPass);

    publishingSummary_->setText(
        status.allLocalChecksPass
            ? QStringLiteral(
                  "<b>All local preparation checks pass.</b> "
                  "If the external publication steps are also complete, "
                  "you may hide this tab.")
            : QStringLiteral(
                  "<b>Publishing setup is incomplete.</b> "
                  "The details below show what remains locally."));
}

void MainWindow::hidePublishingTab()
{
    QString error;

    if (!PublishingStatusBuilder::setHiddenByUser(
            true, &error)) {
        publishingSummary_->setText(
            QStringLiteral(
                "<b>Could not hide tab:</b> %1")
                .arg(error.toHtmlEscaped()));
        return;
    }

    const int index =
        tabs_->indexOf(publishingTabPage_);

    if (index >= 0)
        tabs_->removeTab(index);
}

void MainWindow::showPublishingTab()
{
    QString error;

    if (!PublishingStatusBuilder::setHiddenByUser(
            false, &error)) {
        if (recoverySummary_) {
            recoverySummary_->setText(
                QStringLiteral(
                    "<b>Could not restore Install &amp; Publish:</b> %1")
                    .arg(error.toHtmlEscaped()));
        }
        return;
    }

    if (tabs_->indexOf(publishingTabPage_) < 0)
        tabs_->insertTab(
            0,
            publishingTabPage_,
            QStringLiteral("Install & Publish"));

    tabs_->setCurrentWidget(publishingTabPage_);
    verifyPublishingSetup();
}

// Once a real scroll happens, permanently retire the discovery reminder.
void MainWindow::noteScrollDiscovered()
{
    if (scrollDiscovered_)
        return;

    scrollDiscovered_ = true;

    if (scrollDiscoveryTimer_)
        scrollDiscoveryTimer_->stop();

    if (scrollDiscoveryHint_)
        scrollDiscoveryHint_->setVisible(false);

    QSettings settings;
    settings.setValue(
        QStringLiteral("ui/scroll_discovered"),
        true);
    settings.setValue(
        QStringLiteral("ui/scroll_discovery_usage_seconds"),
        scrollDiscoveryUsageSeconds_);
}

void MainWindow::tickScrollDiscoveryUsage()
{
    if (scrollDiscovered_)
        return;

    ++scrollDiscoveryUsageSeconds_;

    if ((scrollDiscoveryUsageSeconds_ % 15) == 0) {
        QSettings settings;
        settings.setValue(
            QStringLiteral("ui/scroll_discovery_usage_seconds"),
            scrollDiscoveryUsageSeconds_);
    }

    if (scrollDiscoveryUsageSeconds_ >= 300 &&
        scrollDiscoveryHint_) {
        scrollDiscoveryHint_->setVisible(true);
    }
}
