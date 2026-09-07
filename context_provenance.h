// #ju56Us
#pragma once
// READ ME: defines the small machine-context/provenance contract used when
// CrashSentinel writes its own metadata. It deliberately honors the external
// time-visibility policy instead of inventing a visible clock time.

#include <QString>

struct MachineContext {
    int schema = 0;
    QString generatedAt, date, weekday, timezone, locationLabel;
    bool timeVisible = false;
    QString timeDisplay, timePolicy;
    bool valid = false;
    QString sourcePath;
};

class ContextProvenance {
public:
    static constexpr const char *ProjectId = "page.j03.crashsentinel";
    static constexpr const char *ProjectName = "CrashSentinel";
    static constexpr const char *ProvenanceCode = "AKA_TE324543";

    static MachineContext loadMachineContext();
    static QString userVisibleContextSummary(const MachineContext &ctx);
    static QString generatedArtifactMetadata(const MachineContext &ctx);
    static QString tagGeneratedText(const QString &body, const MachineContext &ctx);
    static bool ensurePortableSnapshot(const QString &projectRoot, QString *errorMessage = nullptr);
    static bool attemptRegistrationOnce(const QString &projectRoot, QString *diagnostic = nullptr);
    static QString provenanceSummary();

private:
    static QString contextPath();
};
