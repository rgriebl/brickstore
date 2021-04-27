/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#include <tuple>
#include <QObject>
#include <QVector>
#include <QDate>
#include <QNetworkAccessManager>


class Announcements : public QObject
{
    Q_OBJECT
public:
    explicit Announcements(const QString &baseUrl, QWidget *parent = nullptr);

    bool hasNewAnnouncements() const;
    void showNewAnnouncements();
    void showAllAnnouncements();

public slots:
    void check();
    void markAnnouncementRead(quint64 id);

signals:
    void newAnnouncements();

private:
    QNetworkAccessManager m_nam;
    QWidget *m_parent;
    QString m_rawAnnouncementsUrl;
    QString m_wikiAnnouncementsUrl;

    // id == (days since 2021.1.1) << 32 | crc16(title data as utf16)

    struct Announcement {
        quint64 m_id = 0;
        QString m_title;
        QDate m_date;
        QString m_text;
    };
    QVector<Announcement> m_announcements; // sorted by id (-> implicitly sorted by date)
    QVector<quint64> m_readIds;
};
