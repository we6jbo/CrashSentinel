#pragma once
// READ ME: StatusSnapshot is the GUI-friendly summary of service/package/
// self-protection/crash evidence. It keeps presentation code from needing to
// understand every low-level state file.

// #ju56Us
#include <QString>

struct StatusSnapshot {
    QString version;
    QString channel;
    QString packageType;
    QString guiUser;
    QString daemonEvidenceUser;
    QString serviceState;
    QString selfProtection;
    QString crashSummary;
    QString impactSummary;
    QString stateDir;
};

class StatusSnapshotBuilder
{
public:
    static StatusSnapshot build();
    static QString defaultStateDir();
};
