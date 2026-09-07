#pragma once
// READ ME: CrashSentinel's protection against CrashSentinel itself.
// It tracks same-boot process failures separately from whole-computer abnormal
// reboots. The reboot counter supports the 5-reboot precaution and 20-reboot
// hard stop.

// #ju56Us

#include <QString>

struct SelfProtectionState
{
    bool quarantined = false;
    int rapidFailures = 0;
    int abnormalReboots = 0;
    bool rebootGuardAcknowledged = false;
    bool hardRebootStop = false;
    QString reason;
};

class SelfProtection
{
public:
    static SelfProtectionState noteServiceStart(const QString &stateDir);
    static void noteHeartbeat(const QString &stateDir);
    static void noteCleanExit(const QString &stateDir);
    static bool clearQuarantine(const QString &stateDir, QString *error = nullptr);
    static SelfProtectionState readState(const QString &stateDir);
    static QString statePath(const QString &stateDir);
    static QString disabledNoticePath(const QString &stateDir);
    static bool resetRebootSafeguard(const QString &stateDir, QString *error = nullptr);

private:
    static QString bootId();
    static double uptimeSeconds();
};
