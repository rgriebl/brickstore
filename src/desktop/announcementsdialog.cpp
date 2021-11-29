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
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QIcon>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QTextBrowser>

#include "utility/utility.h"
#include "common/announcements.h"
#include "announcementsdialog.h"


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
    m_browser->setMarkdown(markdown % "\n\n___\n"_l1);
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


void AnnouncementsDialog::showNewAnnouncements(Announcements *announcements, QWidget *parent)
{
    if (!announcements || !announcements->hasNewAnnouncements())
        return;

    QString md;
    QVector<quint64> shownIds;

    for (const auto &a : qAsConst(announcements->m_announcements)) {
        if (announcements->m_readIds.contains(a.m_id))
            continue;
        shownIds << a.m_id;

        if (!md.isEmpty())
            md = md % "\n\n___\n\n"_l1;
        md = md % "**"_l1 % a.m_title % "** &mdash; *"_l1 %
                QLocale().toString(a.m_date, QLocale::ShortFormat) % "*\n\n"_l1 % a.m_text;
    }

    if (shownIds.isEmpty())
        return;

    auto ao = new AnnouncementsDialog(md, parent);
    if (ao->exec() == QDialog::Accepted) {
        for (const auto &id : qAsConst(shownIds))
            announcements->markAnnouncementRead(id);
    }
}

#include "moc_announcementsdialog.cpp"
