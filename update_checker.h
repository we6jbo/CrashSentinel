#pragma once
// READ ME: asynchronous update-check interface.
// CrashSentinel checks the project website for advertised version metadata; the
// package manager/store remains the authority for actually installing updates.

// #ju56Us

#include <QObject>
#include <QNetworkAccessManager>

struct UpdateResult {
    bool checkSucceeded = false;
    bool updateAvailable = false;
    QString availableVersion;
    QString message;
    QString instructionsUrl;
};

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);
    void check();

signals:
    void finished(const UpdateResult &result);

private:
    QNetworkAccessManager network_;
};
Q_DECLARE_METATYPE(UpdateResult)
