// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QSysInfo>
#include <QCryptographicHash>
#include <QString>
#include <QSettings>

#include "exception.h"
#include "credentialsmanager.h"

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  include <wincred.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <QtCore/private/qcore_mac_p.h>
#  include <Security/Security.h>
#elif defined(Q_OS_LINUX) && defined(BS_HAS_LIBSECRET)
extern "C" {
#  undef signals
#  include <libsecret/secret.h>
}

static const SecretSchema brickstoreSchema = {
    "dev.brickstore.Credentials",
    SECRET_SCHEMA_NONE, {
        { "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
        { "id",      SECRET_SCHEMA_ATTRIBUTE_STRING },
        { nullptr,   SECRET_SCHEMA_ATTRIBUTE_STRING },
     },
    0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};
#endif


static QByteArray codeCredential(const QByteArray &in, bool encode)
{
    // This binds the credential to the current OS installation, plus it obscures it quite nicely:
    // - we calculate a "machineId" based on QSysInfo::machineUniqueId() and a BrickStore specific
    //   BLOB appended
    // - we hash this once with Blake2s_128 to get a 16 byte value on any platform
    // - the encrypted credential is then the credential XORed with the machine-id hash
    // - since the machine id can change (e.g. when the user reinstalls the OS), we also preend a
    //   hash of that hash, in order to fail sensibly on loading

    static const auto brickStoreId = "BrickStore\x89\x59\x00\x22\x3c\x76"_qba;
    static const auto machineId = QByteArray(QSysInfo::machineUniqueId() + brickStoreId);
    static const auto normalizedMachineId = QCryptographicHash::hash(machineId,
                                                                     QCryptographicHash::Blake2s_128);
    static const auto hashedMachineId = QCryptographicHash::hash(normalizedMachineId,
                                                                 QCryptographicHash::Blake2s_128);
    static const int hashLen = QCryptographicHash::hashLength(QCryptographicHash::Blake2s_128);

    auto xorByteArray = [](const QByteArray &data, const QByteArray &key) -> QByteArray
    {
        QByteArray result;
        result.resize(data.size());
        for (int i = 0; i < data.size(); ++i)
            result[i] = data[i] ^ key[i % key.size()];
        return result;
    };

    if (encode) {
        return hashedMachineId + xorByteArray(in, normalizedMachineId);
    } else {
        const auto savedHashedMachineId = in.left(hashLen);
        if (!savedHashedMachineId.isEmpty() && (savedHashedMachineId != hashedMachineId))
            throw Exception("credentials were saved on a different machine");

        return xorByteArray(in.mid(hashLen), normalizedMachineId);
    }
}

QByteArray CredentialsManager::load(const QString &service, const QString &id)
{
    QByteArray codedCredential;

#if defined(Q_OS_WINDOWS)
    CREDENTIALW *credentialw = nullptr;
    QString serviceAndId = service + u'/' + id;

    if (CredReadW((LPCWSTR) serviceAndId.utf16(), CRED_TYPE_GENERIC, 0, &credentialw)) {
        codedCredential = QByteArray((const char *) credentialw->CredentialBlob,
                                     credentialw->CredentialBlobSize);
        CredFree(credentialw);
    } else {
        auto error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LPWSTR msg = nullptr;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, error, 0, (LPWSTR) &msg, 0, nullptr);
            // remove potential \r\n at the end
            QString errorString = QString::fromWCharArray(msg).trimmed();
            HeapFree(GetProcessHeap(), 0, msg);
            throw Exception(errorString);
        }
    }

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    QCFString cfservice = service;
    QCFString cfkey = id;

    const void *queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData };
    const void *queryValues[] = { kSecClassGenericPassword, cfservice, cfkey, kCFBooleanTrue };

    QCFType<CFDictionaryRef> query = CFDictionaryCreate(0, queryKeys, queryValues, 4, 0, 0);

    CFTypeRef data;
    OSStatus status = SecItemCopyMatching(query, &data);

    if (status == errSecSuccess) {
        if (data)
            codedCredential = QByteArray::fromCFData((CFDataRef) data);
    } else if (status != errSecItemNotFound) {
        QCFType<CFStringRef> msg = SecCopyErrorMessageString(status, nullptr);
        throw Exception(QString::fromCFString(msg));
    }

#elif defined(Q_OS_LINUX) && defined(BS_HAS_LIBSECRET)
    GError *error = nullptr;
    QByteArray serviceUtf8 = service.toUtf8();
    QByteArray idUtf8 = id.toUtf8();

    auto *secret = ::secret_password_lookup_binary_sync(&brickstoreSchema, nullptr, &error,
                                                        "service", serviceUtf8.constData(),
                                                        "id", idUtf8.constData(),
                                                        nullptr);
    if (error) {
        QString msg = QString::fromUtf8(error->message);
        g_error_free(error);
        throw Exception("libsecret error: %1").arg(msg);
    }

    if (secret) {
        const QByteArray contentType = ::secret_value_get_content_type(secret);
        gsize secretSize = 0;
        const auto secretPointer = ::secret_value_get(secret, &secretSize);
        codedCredential = QByteArray(secretPointer, qsizetype(secretSize));

        ::secret_value_unref(secret);

        if (contentType != "application/octet-stream")
            throw Exception("libsecret: unexpected content type: %1").arg(QString::fromUtf8(contentType));
    }

#else
    codedCredential = QSettings().value(u"/FallbackCredentialsManager/"_qs + service + u'/' + id)
                          .toByteArray();
#endif

    return codeCredential(codedCredential, false);
}

void CredentialsManager::save(const QString &service, const QString &id, const QByteArray &credential)
{
    const QByteArray codedCredential = codeCredential(credential, true);

#if defined(Q_OS_WINDOWS)
    QString serviceAndId = service + u'/' + id;

    CREDENTIALW credentialw;
    ::memset(&credentialw, 0, sizeof(CREDENTIALW));
    credentialw.Type = CRED_TYPE_GENERIC;
    credentialw.TargetName = (LPWSTR) serviceAndId.utf16();
    credentialw.CredentialBlobSize = codedCredential.size();
    credentialw.CredentialBlob = (LPBYTE) codedCredential.constData();
    credentialw.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&credentialw, 0)) {
        auto error = GetLastError();
        LPWSTR msg = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, error, 0, (LPWSTR) &msg, 0, nullptr);
        // remove potential \r\n at the end
        QString errorString = QString::fromWCharArray(msg).trimmed();
        HeapFree(GetProcessHeap(), 0, msg);
        throw Exception(errorString);
    }

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    QCFString cfservice = service;
    QCFString cfkey = id;
    QCFType<CFDataRef> cfcredential = codedCredential.toCFData();

    const void *queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
    const void *queryValues[] = { kSecClassGenericPassword, cfservice, cfkey, cfcredential };

    QCFType<CFDictionaryRef> query = CFDictionaryCreate(0, queryKeys, queryValues, 4, 0, 0);

    SecItemDelete(query);
    OSStatus status = SecItemAdd(query, nullptr);

    if (status != errSecSuccess) {
        QCFType<CFStringRef> msg = SecCopyErrorMessageString(status, nullptr);
        throw Exception(QString::fromCFString(msg));
    }

#elif defined(Q_OS_LINUX) && defined(BS_HAS_LIBSECRET)
    GError *error = nullptr;
    QByteArray serviceUtf8 = service.toUtf8();
    QByteArray idUtf8 = id.toUtf8();
    QByteArray serviceAndIdUtf8 = serviceUtf8 + '/' + idUtf8;

    auto secret = ::secret_value_new(codedCredential.constData(), codedCredential.size(),
                                     "application/octet-stream");

    ::secret_password_store_binary_sync(&brickstoreSchema, SECRET_COLLECTION_DEFAULT,
                                        serviceAndIdUtf8.constData(), secret,
                                        nullptr, &error,
                                        "service", serviceUtf8.constData(),
                                        "id", idUtf8.constData(),
                                        nullptr);
    ::secret_value_unref(secret);

    if (error) {
        QString msg = QString::fromUtf8(error->message);
        g_error_free(error);
        throw Exception("libsecret error: %1").arg(msg);
    }

#else
    QSettings().setValue(u"/FallbackCredentialsManager/"_qs + service + u'/' + id,
                         QVariant::fromValue(codedCredential));
#endif
}
