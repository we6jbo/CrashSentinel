#pragma once
// #ju56Us

#include <QMainWindow>

class QLabel;
class QSpinBox;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class UpdateChecker;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void saveSettings();
    void refreshStatus();
    void applyGamingDefaults();
    void applyBalancedDefaults();
    void checkForUpdates();
    void openUpdateInstructions();
    void clearQuarantine();
    void verifyPublishingSetup();
    void hidePublishingTab();
    void showPublishingTab();
    void noteScrollDiscovered();
    void tickScrollDiscoveryUsage();

private:
    QWidget *makeScrollable(QWidget *content);
    QWidget *buildPublishingTab();
    QWidget *buildStatusTab();
    QWidget *buildSafeguardsTab();
    QWidget *buildProfilesTab();
    QWidget *buildReportsTab();
    QWidget *buildRecoveryDiagnosticsTab();
    void refreshRecoveryDiagnostics();
    void backupBeforeRecovery();
    void clearRecoveryQuarantine();
    void rebuildRecoveryReports();
    void exportRecoveryDiagnostics();
    void loadSettingsIntoUi();
    QString statusText() const;

    QLabel *recoverySummary_ = nullptr;
    QPlainTextEdit *recoveryDetails_ = nullptr;
    QPushButton *clearRecoveryQuarantineButton_ = nullptr;
    QPushButton *rebuildRecoveryReportsButton_ = nullptr;

    QTabWidget *tabs_ = nullptr;
    QWidget *publishingTabPage_ = nullptr;
    QLabel *publishingSummary_ = nullptr;
    QPlainTextEdit *publishingDetails_ = nullptr;
    QPushButton *hidePublishingButton_ = nullptr;
    QLabel *scrollDiscoveryHint_ = nullptr;
    QTimer *scrollDiscoveryTimer_ = nullptr;
    int scrollDiscoveryUsageSeconds_ = 0;
    bool scrollDiscovered_ = false;

    QLabel *versionLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *configPathLabel_ = nullptr;
    QLabel *updateLabel_ = nullptr;

    QPlainTextEdit *statusDetails_ = nullptr;
    QPlainTextEdit *reportView_ = nullptr;

    QPushButton *checkUpdatesButton_ = nullptr;
    QPushButton *openUpdateButton_ = nullptr;
    QPushButton *clearQuarantineButton_ = nullptr;

    QSpinBox *diskGiB_ = nullptr;
    QSpinBox *diskUsedPct_ = nullptr;
    QSpinBox *memoryMiB_ = nullptr;
    QSpinBox *batteryWarn_ = nullptr;
    QSpinBox *batteryCritical_ = nullptr;
    QSpinBox *cpuPressure_ = nullptr;
    QSpinBox *ioPressure_ = nullptr;
    QSpinBox *temperatureC_ = nullptr;
    QSpinBox *retentionDays_ = nullptr;
    QSpinBox *intervalSeconds_ = nullptr;

    QCheckBox *gamingProfile_ = nullptr;
    QCheckBox *pauseHeavy_ = nullptr;
    QCheckBox *autoPurge_ = nullptr;

    UpdateChecker *updateChecker_ = nullptr;
    QString updateInstructionsUrl_;
};
