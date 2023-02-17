// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <tuple>
#include <QObject>
#include <QVector>
#include <QDate>
#include <QNetworkAccessManager>

#include "qcoro/task.h"


class Announcements : public QObject
{
    Q_OBJECT
public:
    explicit Announcements(const QString &baseUrl, QObject *parent = nullptr);

    bool hasNewAnnouncements() const;
    QString announcementsWikiUrl() const;

    // keys: id, title, date, text
    Q_INVOKABLE QVariantList unreadAnnouncements() const;
    Q_INVOKABLE void markAnnouncementRead(quint32 id);

public slots:
    QCoro::Task<> check();

signals:
    void newAnnouncements();

private:
    QNetworkAccessManager m_nam;
    QObject *m_parent;
    QString m_rawAnnouncementsUrl;
    QString m_wikiAnnouncementsUrl;

    // id == (days since 2021.1.1) << 16 | crc16(title data as utf16)

    struct Announcement {
        quint32 m_id = 0;
        QString m_title;
        QDate m_date;
        QString m_text;
    };
    QVector<Announcement> m_announcements; // sorted by id (-> implicitly sorted by date)
    QVector<quint32> m_readIds;

    friend class AnnouncementsDialog;
};
