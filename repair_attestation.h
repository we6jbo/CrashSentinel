#pragma once
// READ ME: creates a machine-local HMAC attestation showing that CrashSentinel
// recently ran and wrote evidence. It is NOT a certificate of hardware health
// and must never be described as one.

// #ju56Us

#include <QString>

class RepairAttestation
{
public:
    static bool update(const QString &stateDir,
                       const QString &snapshotSha256,
                       QString *errorMessage = nullptr);

private:
    static QByteArray hmacSha256(const QByteArray &key, const QByteArray &message);
    static QByteArray loadOrCreateSecret(const QString &stateDir, QString *errorMessage);
    static QString bootId();
};
