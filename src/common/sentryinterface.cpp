// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDateTime>
#include <QLoggingCategory>
#include <QCoreApplication>

#include "sentryinterface.h"

#if defined(Q_OS_ANDROID)
#  include <QLibrary>
#  include <QJniObject>
#  include <jni.h>
#elif defined(SENTRY_ENABLED)
#  include <QStandardPaths>
#  include "sentry.h"
#endif

#if !defined(SENTRY_H_INCLUDED)
union sentry_value_u {
    uint64_t _bits;
    double _double;
};
typedef union sentry_value_u sentry_value_t;
#endif

Q_LOGGING_CATEGORY(LogSentry, "sentry")


class SentryInterfacePrivate
{
public:
    QByteArray m_dsn;
    QByteArray m_release;
    bool m_debug = false;
    bool m_userConsentRequired = true;

#if defined(Q_OS_ANDROID)
    QLibrary m_libsentry;
#endif

    void (*m_sentry_set_tag)(const char *, const char *) = nullptr;
    sentry_value_t (*m_sentry_value_new_breadcrumb)(const char *, const char *) = nullptr;
    int (*m_sentry_value_set_by_key)(sentry_value_t, const char *, sentry_value_t) = nullptr;
    sentry_value_t (*m_sentry_value_new_string)(const char *) = nullptr;
    sentry_value_t (*m_sentry_value_new_int32)(int32_t) = nullptr;
    void (*m_sentry_add_breadcrumb)(sentry_value_t) = nullptr;
    int (*m_sentry_close)(void) = nullptr;
    void (*m_sentry_user_consent_reset)(void) = nullptr;
    void (*m_sentry_user_consent_give)(void) = nullptr;
    void (*m_sentry_user_consent_revoke)(void) = nullptr;
};

bool SentryInterface::isEnabled()
{
#if defined(SENTRY_ENABLED)
    return true;
#else
    return false;
#endif
}

SentryInterface::SentryInterface(const char *dsn, const char *release)
    : d(new SentryInterfacePrivate)
{
    d->m_dsn = dsn;
    d->m_release = release;

#if defined(SENTRY_ENABLED)
#  if defined(Q_OS_ANDROID)
    d->m_libsentry.setFileName(u"sentry"_qs);
    if (d->m_libsentry.load()) {
        d->m_sentry_set_tag = reinterpret_cast<decltype(d->m_sentry_set_tag)>(d->m_libsentry.resolve("sentry_set_tag"));
        d->m_sentry_value_new_breadcrumb = reinterpret_cast<decltype(d->m_sentry_value_new_breadcrumb)>(d->m_libsentry.resolve("sentry_value_new_breadcrumb"));
        d->m_sentry_value_set_by_key = reinterpret_cast<decltype(d->m_sentry_value_set_by_key)>(d->m_libsentry.resolve("sentry_value_set_by_key"));
        d->m_sentry_value_new_string = reinterpret_cast<decltype(d->m_sentry_value_new_string)>(d->m_libsentry.resolve("sentry_value_new_string"));
        d->m_sentry_value_new_int32 = reinterpret_cast<decltype(d->m_sentry_value_new_int32)>(d->m_libsentry.resolve("sentry_value_new_int32"));
        d->m_sentry_add_breadcrumb = reinterpret_cast<decltype(d->m_sentry_add_breadcrumb)>(d->m_libsentry.resolve("sentry_add_breadcrumb"));
        d->m_sentry_close = reinterpret_cast<decltype(d->m_sentry_close)>(d->m_libsentry.resolve("sentry_close"));
        d->m_sentry_user_consent_reset = reinterpret_cast<decltype(d->m_sentry_user_consent_reset)>(d->m_libsentry.resolve("sentry_user_consent_reset"));
        d->m_sentry_user_consent_give = reinterpret_cast<decltype(d->m_sentry_user_consent_give)>(d->m_libsentry.resolve("sentry_user_consent_give"));
        d->m_sentry_user_consent_revoke = reinterpret_cast<decltype(d->m_sentry_user_consent_revoke)>(d->m_libsentry.resolve("sentry_user_consent_revoke"));
    }
#  else
    d->m_sentry_set_tag = sentry_set_tag;
    d->m_sentry_value_new_breadcrumb = sentry_value_new_breadcrumb;
    d->m_sentry_value_set_by_key = sentry_value_set_by_key;
    d->m_sentry_value_new_string = sentry_value_new_string;
    d->m_sentry_value_new_int32 = sentry_value_new_int32;
    d->m_sentry_add_breadcrumb = sentry_add_breadcrumb;
    d->m_sentry_close = sentry_close;
    d->m_sentry_user_consent_reset = sentry_user_consent_reset;
    d->m_sentry_user_consent_give = sentry_user_consent_give;
    d->m_sentry_user_consent_revoke = sentry_user_consent_revoke;
#  endif
#endif
}

SentryInterface::~SentryInterface()
{ }

void SentryInterface::setDebug(bool on)
{
    d->m_debug = on;
}

void SentryInterface::setUserConsentRequired(bool on)
{
    d->m_userConsentRequired = on;
}

bool SentryInterface::init()
{
    bool ok = false;

#if defined(SENTRY_ENABLED)
#  if defined(Q_OS_ANDROID)
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject jniDsn = QJniObject::fromString(QString::fromLatin1(d->m_dsn));
    QJniObject jniRelease = QJniObject::fromString(QString::fromLatin1(d->m_release));

    ok = activity.callMethod<jboolean>("setupSentry",
                                       jniDsn.object<jstring>(),
                                       jniRelease.object<jstring>(),
                                       jboolean(d->m_debug),
                                       jboolean(d->m_userConsentRequired));
#  else
    auto *sentry = sentry_options_new();
    if (d->m_debug) {
        sentry_options_set_debug(sentry, 1);
        sentry_options_set_logger(sentry, [](sentry_level_t level, const char *message, va_list args, void *) {
                QDebug dbg(static_cast<QString *>(nullptr));
                switch (level) {
                default:
                case SENTRY_LEVEL_DEBUG:   dbg = QMessageLogger().debug(LogSentry); break;
                case SENTRY_LEVEL_INFO:    dbg = QMessageLogger().info(LogSentry); break;
                case SENTRY_LEVEL_WARNING: dbg = QMessageLogger().warning(LogSentry); break;
                case SENTRY_LEVEL_ERROR:
                case SENTRY_LEVEL_FATAL:   dbg = QMessageLogger().critical(LogSentry); break;
                }
                dbg.noquote() << QString::vasprintf(QByteArray(message).replace("%S", "%ls").constData(), args);
            }, nullptr);
    }
    sentry_options_set_dsn(sentry, d->m_dsn.constData());
    sentry_options_set_release(sentry, d->m_release.constData());
    sentry_options_set_require_user_consent(sentry, d->m_userConsentRequired ? 1 : 0);

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/.sentry";
    QString crashHandler = QCoreApplication::applicationDirPath() + u"/crashpad_handler";

#    if defined(Q_OS_WINDOWS)
    crashHandler.append(u".exe"_qs);
    sentry_options_set_handler_pathw(sentry, reinterpret_cast<const wchar_t *>(crashHandler.utf16()));
    sentry_options_set_database_pathw(sentry, reinterpret_cast<const wchar_t *>(dbPath.utf16()));
#    else
    sentry_options_set_handler_path(sentry, crashHandler.toLocal8Bit().constData());
    sentry_options_set_database_path(sentry, dbPath.toLocal8Bit().constData());
#    endif
    ok = (sentry_init(sentry) == 0);
#  endif

    if (ok)
        qCInfo(LogSentry) << "Successfully initialized sentry.io";
    else
        qCWarning(LogSentry) << "Could not initialize sentry.io!";
#endif
    return ok;
}

void SentryInterface::close()
{
    if (d->m_sentry_close)
        d->m_sentry_close();
}

void SentryInterface::userConsentReset()
{
    if (d->m_sentry_user_consent_reset)
        d->m_sentry_user_consent_reset();
}

void SentryInterface::userConsentGive()
{
    if (d->m_sentry_user_consent_give)
        d->m_sentry_user_consent_give();
}

void SentryInterface::userConsentRevoke()
{
    if (d->m_sentry_user_consent_revoke)
        d->m_sentry_user_consent_revoke();
}

void SentryInterface::setTag(const QByteArray &key, const QByteArray &value)
{
    if (d->m_sentry_set_tag)
        d->m_sentry_set_tag(key.constData(), value.constData());
}

void SentryInterface::addBreadcrumb(QtMsgType msgType,
                                    const QMessageLogContext &msgCtx,
                                    const QString &msg)
{
    if (d->m_sentry_value_new_breadcrumb
            && d->m_sentry_value_set_by_key
            && d->m_sentry_value_new_string
            && d->m_sentry_value_new_int32
            && d->m_sentry_add_breadcrumb) {
        sentry_value_t crumb = d->m_sentry_value_new_breadcrumb("default", msg.toUtf8());
        if (msgCtx.category)
            d->m_sentry_value_set_by_key(crumb, "category", d->m_sentry_value_new_string(msgCtx.category));
        msgType = std::clamp(msgType, QtDebugMsg, QtInfoMsg);
        static const char *msgTypeLevels[5] = { "debug", "warning", "error", "fatal", "info" };
        d->m_sentry_value_set_by_key(crumb, "level", d->m_sentry_value_new_string(msgTypeLevels[msgType]));
        const auto now = QDateTime::currentSecsSinceEpoch();
        d->m_sentry_value_set_by_key(crumb, "timestamp", d->m_sentry_value_new_int32(int32_t(now)));
        d->m_sentry_add_breadcrumb(crumb);
    }
}
