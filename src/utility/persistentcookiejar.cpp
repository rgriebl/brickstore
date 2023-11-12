// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QFile>
#include <QNetworkCookie>
#include <QDebug>

#include "exception.h"
#include "passwordmanager.h"
#include "persistentcookiejar.h"


PersistentCookieJar::PersistentCookieJar(const QString &datadir, const QString &name, QObject *parent)
    : QNetworkCookieJar(parent)
    , m_dataDir(datadir)
    , m_name(name)
{ }

QList<QNetworkCookie> PersistentCookieJar::cookiesForUrl(const QUrl &url) const
{
    const_cast<PersistentCookieJar *>(this)->load();
    return QNetworkCookieJar::cookiesForUrl(url);
}

bool PersistentCookieJar::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    save();
    return QNetworkCookieJar::setCookiesFromUrl(cookieList, url);
}

void PersistentCookieJar::load()
{
    QMutexLocker lock(&m_mutex);
    if (m_loaded)
        return;

    try {
        auto cookies = PasswordManager::load(u"BrickStore"_qs, m_name + u"-Cookies"_qs);
        setAllCookies(QNetworkCookie::parseCookies(qUncompress(cookies)));
    } catch (const Exception &e) {
        qWarning() << "Could not load cookies for" << m_name << ":" << e.errorString();
    }
    m_loaded = true;
}

void PersistentCookieJar::save()
{
    QMutexLocker lock(&m_mutex);
    QByteArray data;
    QList<QNetworkCookie> cookieList = allCookies();
    for (const auto &cookie : cookieList)
        data = data + cookie.toRawForm() + '\n';

    try {
        PasswordManager::save(u"BrickStore"_qs, m_name + u"-Cookies"_qs, qCompress(data));
    } catch (const Exception &e) {
        qWarning() << "Could not save cookies for" << m_name << ":" << e.errorString();
    }
}

#include "moc_persistentcookiejar.cpp"
