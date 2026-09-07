#pragma once
// #ju56Us

#include <QMainWindow>

class QLabel;
class QSpinBox;
class QCheckBox;
class QPlainTextEdit;

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

private:
    QWidget *makeScrollable(QWidget *content);
    QWidget *buildStatusTab();
    QWidget *buildSafeguardsTab();
    QWidget *buildProfilesTab();
    QWidget *buildReportsTab();
    void loadSettingsIntoUi();

    QLabel *statusLabel_ = nullptr;
    QLabel *configPathLabel_ = nullptr;
    QPlainTextEdit *reportView_ = nullptr;

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
};
