#pragma once
// READ ME: local publishing-readiness model.
// These checks can prove that required local files/configuration exist. They
// cannot prove that GitHub, Flathub, or the Snap Store accepted publication.

// #ju56Us

#include <QString>

struct PublishingStatus
{
    bool githubReady = false;
    bool snapReady = false;
    bool flatpakReady = false;
    bool allLocalChecksPass = false;
    QString details;
};

class PublishingStatusBuilder
{
public:
    static PublishingStatus detect(const QString &projectRoot);
    static QString stateFile();
    static bool isHiddenByUser();
    static bool setHiddenByUser(bool hidden, QString *error = nullptr);
};
