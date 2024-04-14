// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>

#include <QFile>
#include <QNetworkCookie>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>

#include "exception.h"
#include "credentialsmanager.h"
#include "persistentcookiejar.h"

using namespace std::chrono_literals;


PersistentCookieJar::PersistentCookieJar(const QString &datadir, const QString &name,
                                         const QByteArrayList &persistSessionCookies,
                                         QObject *parent)
    : QNetworkCookieJar(parent)
    , m_persistSessionCookies(persistSessionCookies)
    , m_dataDir(datadir)
    , m_name(name)
{
    // load
    try {
        const QByteArray raw = CredentialsManager::load(u"BrickStore"_qs, m_name + u"-Cookies"_qs);
        if (!raw.isEmpty()) {
            m_lastSaveData = m_nextSaveData = qUncompress(raw);
            setAllCookies(QNetworkCookie::parseCookies(m_lastSaveData));
        }
    } catch (const Exception &e) {
        qWarning() << "Could not load cookies for" << m_name << ":" << e.errorString();
    }

    // save
    m_saveThread = QThread::create([this]() {
        bool stop = false;
        while (!stop) {
            m_mutex.lock();
            m_saveCondition.wait(&m_mutex, QDeadlineTimer(30min));
            QByteArray data = m_nextSaveData;
            stop = m_stopSaveThread;
            m_mutex.unlock();

            if (m_lastSaveData != data) {
                m_lastSaveData = data;

                try {
                    CredentialsManager::save(u"BrickStore"_qs, m_name + u"-Cookies"_qs, qCompress(data));
                } catch (const Exception &e) {
                    qWarning() << "Could not save cookies for" << m_name << ":" << e.errorString();
                }
            }
        }
    });

    m_saveThread->start();
}

PersistentCookieJar::~PersistentCookieJar()
{
    m_mutex.lock();
    m_stopSaveThread = true;
    m_saveCondition.wakeAll();
    m_mutex.unlock();

    m_saveThread->wait();
    delete m_saveThread;
}

QList<QNetworkCookie> PersistentCookieJar::cookiesForUrl(const QUrl &url) const
{
    return QNetworkCookieJar::cookiesForUrl(url);
}

bool PersistentCookieJar::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    bool result = QNetworkCookieJar::setCookiesFromUrl(cookieList, url);

    QByteArray data;
    QList<QNetworkCookie> all = allCookies();
    for (const auto &cookie : all) {
        if (!cookie.isSessionCookie() || m_persistSessionCookies.contains(cookie.name()))
            data = data + cookie.toRawForm() + '\n';
    }

    m_mutex.lock();
    m_nextSaveData = data;
    m_mutex.unlock();

    return result;
}

void PersistentCookieJar::dumpCookies(const QList<QNetworkCookie> &cookies)
{
    for (const auto &cookie : cookies) {
        qWarning() << " *" << cookie.name() << cookie.expirationDate() << cookie.value().left(16)
                   << cookie.domain();
    }
}

#include "moc_persistentcookiejar.cpp"
