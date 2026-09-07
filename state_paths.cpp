// #ju56Us
// READ ME: chooses CrashSentinel's state directory and explains package mode.
// CRASHSENTINEL_STATE_DIR is an explicit override; SNAP_COMMON is used for Snap;
// development/native falls back to Qt's per-user StateLocation.

#include "state_paths.h"

#include <QDir>
#include <QStandardPaths>

namespace
{
QString cleanEnv(const char *name)
{
    return qEnvironmentVariable(name).trimmed();
}
}

// IMPORTANT: both GUI and monitor must agree on this location to share evidence safely.
QString StatePaths::sharedStateDir()
{
    const QString overrideDir =
        cleanEnv("CRASHSENTINEL_STATE_DIR");

    if (!overrideDir.isEmpty())
        return QDir::cleanPath(overrideDir);

    const QString snapCommon =
        cleanEnv("SNAP_COMMON");

    if (!snapCommon.isEmpty())
        return QDir(snapCommon).filePath(
            QStringLiteral("state"));

    QString state =
        QStandardPaths::writableLocation(
            QStandardPaths::StateLocation);

    if (state.isEmpty())
        state =
            QDir::homePath() +
            QStringLiteral("/.local/state/CrashSentinel");

    return QDir::cleanPath(state);
}

// Small label used for diagnostics and status; it does not grant privileges.
QString StatePaths::packagingMode()
{
    if (!cleanEnv("SNAP").isEmpty())
        return QStringLiteral("Snap shared state");

    if (!cleanEnv("FLATPAK_ID").isEmpty())
        return QStringLiteral("Flatpak GUI/report-viewer state");

    if (!cleanEnv("CRASHSENTINEL_STATE_DIR").isEmpty())
        return QStringLiteral("Explicit shared state override");

    return QStringLiteral("Development/native user state");
}

QString StatePaths::stateExplanation()
{
    if (!cleanEnv("SNAP").isEmpty()) {
        return QStringLiteral(
            "Snap mode: the system monitor and GUI use the same "
            "SNAP_COMMON/state location. Monitor-produced evidence is "
            "intended to be readable by the GUI. Privileged repair or "
            "service-control actions may still require snap/system "
            "service authorization.");
    }

    if (!cleanEnv("FLATPAK_ID").isEmpty()) {
        return QStringLiteral(
            "Flatpak mode: CrashSentinel operates as a GUI/report viewer. "
            "The Flatpak package does not claim unrestricted host-monitor "
            "access. Missing host evidence may reflect sandbox confinement.");
    }

    return QStringLiteral(
        "Development/native mode: CrashSentinel uses the normal per-user "
        "state location unless CRASHSENTINEL_STATE_DIR is explicitly set.");
}
