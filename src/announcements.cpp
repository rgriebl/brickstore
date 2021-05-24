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
#include <QRegularExpression>
#include <QNetworkReply>
#include <QByteArray>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QIcon>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QDebug>
#include <QDesktopServices>

#include "announcements.h"
#include "announcements_p.h"
#include "config.h"
#include "systeminfo.h"
#include "utility.h"


Announcements::Announcements(const QString &baseUrl, QWidget *parent)
    : QObject(parent)
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

void Announcements::check()
{
    QNetworkReply *reply = m_nam.get(QNetworkRequest(m_rawAnnouncementsUrl));

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
            return;

        QString md = QString::fromUtf8(reply->readAll());
        QRegularExpression header(R"(^## (.+?) - (\d{4})-(\d{2})-(\d{2}) \[\]\(([^)]*)\)$)"_l1);
        header.setPatternOptions(QRegularExpression::MultilineOption);
        // ## TITLE - YYYY-MM-DD [](<CONDITIONS>)
        // TEXT

        QVariantMap conditions = SystemInfo::inst()->asMap();
        conditions["language"_l1] = Config::inst()->language();

        int captureNextStart = 0;
        int nextHeader = 0;

        QString nextTitle;
        QDate nextDate;
        QVector<Announcement> announcements;

        while (true) {
            QRegularExpressionMatch match = header.match(md, nextHeader);

            if (captureNextStart) {
                int captureEnd = match.hasMatch() ? match.capturedStart() : md.size();

                Announcement a;
                a.m_date = nextDate;
                a.m_title = nextTitle;
                a.m_text = md.mid(captureNextStart, captureEnd - captureNextStart).trimmed();

                quint64 id = (quint64(QDate(2021, 1, 1).daysTo(a.m_date)) << 32)
                        | qChecksum((const char *) a.m_title.constData(), a.m_title.size() * 2,
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
    });
}


void Announcements::markAnnouncementRead(quint64 id)
{
    auto it = std::find_if(m_announcements.begin(), m_announcements.end(), [id](const auto &a) {
        return a.m_id == id;
    });
    if (it != m_announcements.end()) {
        if (!m_readIds.contains(id)) {
            m_readIds.append(id);

            QVariantList vl;
            for (const quint64 &id : qAsConst(m_readIds))
                vl << id;
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

void Announcements::showNewAnnouncements()
{
    if (!hasNewAnnouncements())
        return;

    QString md;
    QVector<quint64> shownIds;

    for (const auto &a : qAsConst(m_announcements)) {
        if (m_readIds.contains(a.m_id))
            continue;
        shownIds << a.m_id;

        if (!md.isEmpty())
            md = md % "\n\n___\n\n"_l1;
        md = md % "**"_l1 % a.m_title % "** &mdash; *"_l1 %
                QLocale().toString(a.m_date, QLocale::ShortFormat) % "*\n\n"_l1 % a.m_text;
    }

    if (shownIds.isEmpty())
        return;

    auto ao = new AnnouncementsDialog(md, m_parent);
    if (ao->exec() == QDialog::Accepted) {
        for (const auto &id : qAsConst(shownIds))
            markAnnouncementRead(id);
    }
}

void Announcements::showAllAnnouncements()
{
    QDesktopServices::openUrl(m_wikiAnnouncementsUrl);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


AnnouncementsDialog::AnnouncementsDialog(const QString &markdown, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    parentWidget()->installEventFilter(this);

    QVBoxLayout *top = new QVBoxLayout(this);

    QHBoxLayout *lay = new QHBoxLayout;
    lay->setSpacing(lay->spacing() * 2);
    lay->addStretch(1);

    auto icon = new QLabel();
    int s = fontMetrics().height();
    icon->setPixmap(QIcon::fromTheme("dialog-warning"_l1).pixmap(s * 3));
    icon->setFixedSize(s * 3, s * 3);
    lay->addWidget(icon, 0, Qt::AlignVCenter | Qt::AlignLeft);

    m_header = new QLabel();
    auto f = font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.5);
    m_header->setFont(f);
    m_header->setText(tr("Important Announcements"));
    lay->addWidget(m_header, 1, Qt::AlignVCenter | Qt::AlignLeft);
    lay->addStretch(1);
    top->addLayout(lay);

    m_browser = new QTextBrowser();
    m_browser->setReadOnly(true);
    m_browser->setFrameStyle(QFrame::NoFrame);
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    m_browser->setPlainText(markdown);
#else
    m_browser->setMarkdown(markdown % "\n\n___\n"_l1);
#endif
    m_browser->setOpenLinks(true);
    m_browser->setOpenExternalLinks(true);
    m_browser->viewport()->setAutoFillBackground(false);
    m_browser->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_browser->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_browser->setFixedWidth(fontMetrics().averageCharWidth() * 100);
    m_browser->zoomIn(2);


    top->addWidget(m_browser, 1);

    auto *buttons = new QDialogButtonBox();
    buttons->setCenterButtons(true);
    buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Mark read"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    top->addWidget(buttons);

    paletteChange();

    QMetaObject::invokeMethod(this, [this]() {
        int h = m_browser->document()->size().toSize().height();
        m_browser->setFixedHeight(h);
        resize(sizeHint());

        sizeChange();
    }, Qt::QueuedConnection);
}

bool AnnouncementsDialog::eventFilter(QObject *o, QEvent *e)
{
    bool b = QWidget::eventFilter(o, e);

    if (e->type() == QEvent::Resize && o && o == parentWidget())
        sizeChange();

    if (e->type() == QEvent::PaletteChange)
        paletteChange();

    return b;
}

void AnnouncementsDialog::resizeEvent(QResizeEvent *)
{
    sizeChange();
}

void AnnouncementsDialog::paintEvent(QPaintEvent *pe)
{
    QPainter p(this);
    p.setClipRect(pe->rect());
    p.setOpacity(.98);
    auto r = fontMetrics().height() / 2;
    QPen pen(QColor(0, 0, 0, 128));
    pen.setWidthF(2);
    p.setPen(pen);
    p.setBrush(m_stripes);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), r, r);
}

void AnnouncementsDialog::sizeChange()
{
    // center in parent
    auto p = parentWidget()->pos();
    auto s = parentWidget()->frameSize();
    move(p.x() + (s.width() - width()) / 2, p.y() + (s.height() - height()) / 2);
}

void AnnouncementsDialog::paletteChange()
{
    QColor fg = palette().color(QPalette::Active, QPalette::Text);
    QColor bg;
    QColor stripe;
    int hue = palette().color(QPalette::Highlight).hslHue();
    if (fg.lightnessF() < 0.5) {
        bg = QColor::fromHsl(hue, 255, 220);
        stripe = Qt::white;
    } else {
        bg = QColor::fromHsl(hue, 255, 30);
        stripe = Qt::black;
    }
    m_stripes = QPixmap::fromImage(Utility::stripeImage(100, stripe, bg));
}
