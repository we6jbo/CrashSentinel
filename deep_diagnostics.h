#pragma once
// READ ME: one-shot deeper diagnostic report generator.
// These collectors are read-only evidence gathering. Permission/confinement
// failures must remain distinguishable from a healthy result.

// #ju56Us

#include <QString>

class DeepDiagnostics
{
public:
    static bool writeReports(const QString &stateDir,
                             QString *error = nullptr);
};
