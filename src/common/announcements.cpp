// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QRegularExpression>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkReply>
#include <QtGui/QDesktopServices>

#include <QCoro/QCoroNetworkReply>

#include "common/systeminfo.h"
#include "announcements.h"
#include "config.h"


Announcements::Announcements(const QString &baseUrl, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_parent(parent)
{
    m_rawAnnouncementsUrl = baseUrl;
    m_rawAnnouncementsUrl.replace(u"github.com"_qs, u"https://raw.githubusercontent.com/wiki"_qs);
    m_rawAnnouncementsUrl.append(u"/Announcements.md"_qs);

    m_wikiAnnouncementsUrl = baseUrl;
    m_wikiAnnouncementsUrl.prepend(u"https://"_qs);
    m_wikiAnnouncementsUrl.append(u"/wiki/Announcements"_qs);

    const auto vl = Config::inst()->value(u"Announcements/ReadIds"_qs).toList();
    m_readIds.reserve(vl.size());
    for (const QVariant &v : vl)
        m_readIds << v.toUInt();
}

QCoro::Task<> Announcements::check()
{
    QNetworkReply *reply = co_await m_nam.get(QNetworkRequest(m_rawAnnouncementsUrl));

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
        co_return;

    QString md = QString::fromUtf8(reply->readAll());
    static const QRegularExpression header(uR"(^## (.+?) - (\d{4})-(\d{2})-(\d{2}) \[\]\(([^)]*)\)$)"_qs,
                                           QRegularExpression::MultilineOption);
    // ## TITLE - YYYY-MM-DD [](<CONDITIONS>)
    // TEXT

    QVariantMap conditions = SystemInfo::inst()->asMap();
    conditions[u"language"_qs] = Config::inst()->language();

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
            const QStringList condList = allConds.split(u","_qs);
            for (const QString &cond : condList) {
                QString key = cond.section(u":"_qs, 0, 0).trimmed();
                QString val = cond.section(u":"_qs, 1, -1).trimmed();

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
            vl.reserve(m_readIds.size());
            for (const quint32 &readId : std::as_const(m_readIds))
                vl << readId;

            Config::inst()->setValue(u"Announcements/ReadIds"_qs, vl);
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
            { u"id"_qs, a.m_id },
            { u"date"_qs, a.m_date },
            { u"title"_qs, a.m_title },
            { u"text"_qs, a.m_text },
        };
        vl.append(vm);
    }
    return vl;
}

#include "moc_announcements.cpp"
