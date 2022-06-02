/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QtCore/QRegularExpression>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkReply>
#include <QtGui/QDesktopServices>

#include "qcoro/network/qcoronetworkreply.h"
#include "utility/utility.h"
#include "utility/systeminfo.h"
#include "announcements.h"
#include "config.h"


Announcements::Announcements(const QString &baseUrl, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_parent(parent)
{
    m_rawAnnouncementsUrl = baseUrl;
    m_rawAnnouncementsUrl.replace("github.com"_l1, "https://raw.githubusercontent.com/wiki"_l1);
    m_rawAnnouncementsUrl.append("/Announcements.md"_l1);

    m_wikiAnnouncementsUrl = baseUrl;
    m_wikiAnnouncementsUrl.prepend("https://"_l1);
    m_wikiAnnouncementsUrl.append("/wiki/Announcements"_l1);

    const auto vl = Config::inst()->value("Announcements/ReadIds"_l1).toList();
    for (const QVariant &v : vl)
        m_readIds << v.toULongLong();
}

QCoro::Task<> Announcements::check()
{
    QNetworkReply *reply = co_await m_nam.get(QNetworkRequest(m_rawAnnouncementsUrl));

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
        co_return;

    QString md = QString::fromUtf8(reply->readAll());
    static const QRegularExpression header(R"(^## (.+?) - (\d{4})-(\d{2})-(\d{2}) \[\]\(([^)]*)\)$)"_l1,
                                           QRegularExpression::MultilineOption);
    // ## TITLE - YYYY-MM-DD [](<CONDITIONS>)
    // TEXT

    QVariantMap conditions = SystemInfo::inst()->asMap();
    conditions["language"_l1] = Config::inst()->language();

    qsizetype captureNextStart = 0;
    qsizetype nextHeader = 0;

    QString nextTitle;
    QDate nextDate;
    QVector<Announcement> announcements;

    while (true) {
        QRegularExpressionMatch match = header.match(md, nextHeader);

        if (captureNextStart) {
            auto captureEnd = match.hasMatch() ? match.capturedStart() : md.size();

            Announcement a;
            a.m_date = nextDate;
            a.m_title = nextTitle;
            a.m_text = md.mid(captureNextStart, captureEnd - captureNextStart).trimmed();

            quint32 id = (quint32(QDate(2021, 1, 1).daysTo(a.m_date)) << 16)
                    | qChecksum(QByteArrayView(reinterpret_cast<const char *>(a.m_title.constData()),
                                               a.m_title.size() * 2),
                                Qt::ChecksumIso3309);
            a.m_id = id;

            announcements.append(a);
            captureNextStart = 0;
        }

        if (!match.hasMatch())
            break;

        QString title = match.captured(1);
        QDate date = QDate(match.captured(2).toInt(), match.captured(3).toInt(),
                           match.captured(4).toInt());
        QString allConds = match.captured(5).trimmed();

        bool conditionMatch = true;

        if (!allConds.isEmpty()) {
            QStringList condList = allConds.split(","_l1);
            for (const QString &cond : condList) {
                QString key = cond.section(":"_l1, 0, 0).trimmed();
                QString val = cond.section(":"_l1, 1, -1).trimmed();

                QRegularExpression valRE(QRegularExpression::wildcardToRegularExpression(val));
                valRE.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
                if (!conditions.contains(key) || !valRE.match(conditions.value(key).toString()).hasMatch()) {
                    conditionMatch = false;
                    break;
                }
            }
        }

        if (conditionMatch) {
            nextDate = date;
            nextTitle = title;
            captureNextStart = match.capturedEnd() + 1;
        }

        nextHeader = match.capturedEnd();
    }

    std::sort(announcements.begin(), announcements.end(), [](const auto &a1, const auto &a2) {
        return (a1.m_id < a2.m_id);
    });

    m_announcements = announcements;
    if (hasNewAnnouncements())
        emit newAnnouncements();
}


void Announcements::markAnnouncementRead(quint32 id)
{
    auto it = std::find_if(m_announcements.begin(), m_announcements.end(), [id](const auto &a) {
        return a.m_id == id;
    });
    if (it != m_announcements.end()) {
        if (!m_readIds.contains(id)) {
            m_readIds.append(id);

            QVariantList vl;
            for (const quint32 &readId : qAsConst(m_readIds))
                vl << readId;

            Config::inst()->setValue("Announcements/ReadIds"_l1, vl);
        }
    }
}

bool Announcements::hasNewAnnouncements() const
{
    for (const auto &a : m_announcements) {
        if (!m_readIds.contains(a.m_id))
            return true;
    }
    return false;
}

QString Announcements::announcementsWikiUrl() const
{
    return m_wikiAnnouncementsUrl;
}

QVariantList Announcements::unreadAnnouncements() const
{
    QVariantList vl;

    for (const auto &a : m_announcements) {
        if (m_readIds.contains(a.m_id))
            continue;
        QVariantMap vm {
            { "id"_l1, a.m_id },
            { "date"_l1, a.m_date },
            { "title"_l1, a.m_title },
            { "text"_l1, a.m_text },
        };
        vl.append(vm);
    }
    return vl;
}

#include "moc_announcements.cpp"
