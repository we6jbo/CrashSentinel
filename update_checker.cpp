// #ju56Us
// READ ME: fetches the update page and compares semantic version components.
// This code only reports availability/instructions. It does not self-update.

#include "update_checker.h"
#include "version_info.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

static QList<int> semverParts(QString v)
{
    v = v.trimmed();
    v.remove(QRegularExpression(QStringLiteral("^[^0-9]*")));
    v = v.section('-', 0, 0);

    QList<int> parts;
    for (const QString &part : v.split('.'))
        parts << part.toInt();

    while (parts.size() < 3)
        parts << 0;

    return parts;
}

static bool newerThan(const QString &candidate,
                      const QString &current)
{
    const auto a = semverParts(candidate);
    const auto b = semverParts(current);

    for (int i = 0; i < qMin(a.size(), b.size()); ++i) {
        if (a[i] != b[i])
            return a[i] > b[i];
    }

    return false;
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

// Network check only: report advertised version/instructions; never install an update here.
void UpdateChecker::check()
{
    QNetworkRequest request(
        QUrl(QString::fromLatin1(
            CrashSentinelVersion::UpdatePage)));

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("CrashSentinel/%1 update-check")
            .arg(QString::fromLatin1(
                CrashSentinelVersion::Version)));

    QNetworkReply *reply = network_.get(request);

    connect(reply, &QNetworkReply::finished,
            this, [this, reply] {
        UpdateResult result;

        result.instructionsUrl =
            QString::fromLatin1(
                CrashSentinelVersion::UpdateAnchor);

        if (reply->error() != QNetworkReply::NoError) {
            result.message =
                QStringLiteral("Update check failed: ") +
                reply->errorString();

            reply->deleteLater();
            emit finished(result);
            return;
        }

        const QString html =
            QString::fromUtf8(reply->readAll());

        reply->deleteLater();

        QRegularExpression versionRe(
            QStringLiteral(
                R"(id\s*=\s*["']crashsentinel-update["'][^>]*data-version\s*=\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption);

        QRegularExpression urlRe(
            QStringLiteral(
                R"(id\s*=\s*["']crashsentinel-update["'][^>]*data-update-url\s*=\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption);

        const auto versionMatch =
            versionRe.match(html);

        if (!versionMatch.hasMatch()) {
            result.message =
                QStringLiteral(
                    "The update page was reachable, but no "
                    "CrashSentinel update manifest was found.");

            emit finished(result);
            return;
        }

        result.checkSucceeded = true;
        result.availableVersion =
            versionMatch.captured(1).trimmed();

        const auto urlMatch =
            urlRe.match(html);

        if (urlMatch.hasMatch())
            result.instructionsUrl =
                urlMatch.captured(1).trimmed();

        result.updateAvailable =
            newerThan(
                result.availableVersion,
                QString::fromLatin1(
                    CrashSentinelVersion::Version));

        result.message =
            result.updateAvailable
                ? QStringLiteral("Update available: %1")
                      .arg(result.availableVersion)
                : QStringLiteral(
                      "No newer update advertised. Installed: %1")
                      .arg(QString::fromLatin1(
                          CrashSentinelVersion::Version));

        emit finished(result);
    });
}
