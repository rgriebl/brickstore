// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QNetworkCookieJar>
#include <QMutex>


class PersistentCookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    PersistentCookieJar(const QString &datadir, const QString &name, QObject *parent = nullptr);

    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override;
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override;

private:
    void load();
    void save();

    QMutex m_mutex;
    QString m_dataDir;
    QString m_name;
    bool m_loaded = false;
};
