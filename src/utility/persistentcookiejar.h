// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QNetworkCookieJar>
#include <QWaitCondition>
#include <QMutex>

QT_FORWARD_DECLARE_CLASS(QThread)


class PersistentCookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    PersistentCookieJar(const QString &datadir, const QString &name,
                        const QByteArrayList &persistSessionCookies, QObject *parent = nullptr);
    ~PersistentCookieJar() override;

    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override;
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override;

private:
    static void dumpCookies(const QList<QNetworkCookie> &cookies);

    QMutex m_mutex;
    QThread *m_saveThread = nullptr;
    bool m_stopSaveThread = false;
    QWaitCondition m_saveCondition;
    QByteArray m_lastSaveData;
    QByteArray m_nextSaveData;
    QByteArrayList m_persistSessionCookies;

    QString m_dataDir;
    QString m_name;
};
