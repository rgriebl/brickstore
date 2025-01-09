// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QStringBuilder>
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

#include <QCoro/QCoroSignal>

#include "utility/utility.h"
#include "common/announcements.h"
#include "announcementsdialog.h"


AnnouncementsDialog::AnnouncementsDialog(const QString &markdown, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);

    auto *top = new QVBoxLayout(this);

    auto *lay = new QHBoxLayout;
    lay->setSpacing(lay->spacing() * 2);
    lay->addStretch(1);

    auto icon = new QLabel();
    int s = fontMetrics().height();
    icon->setPixmap(QIcon::fromTheme(u"dialog-warning"_qs).pixmap(s * 3));
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
    m_browser->setMarkdown(markdown + u"\n\n___\n");
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
    }, Qt::QueuedConnection);
}

void AnnouncementsDialog::resizeEvent(QResizeEvent *)
{
    // center in parent
    auto p = parentWidget()->pos();
    auto s = parentWidget()->frameSize();
    move(p.x() + (s.width() - width()) / 2, p.y() + (s.height() - height()) / 2);
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

void AnnouncementsDialog::paletteChange()
{
    QColor fg = palette().color(QPalette::Active, QPalette::Text);
    QColor bg;
    QColor stripe;
    int hue = palette().color(QPalette::Highlight).hslHue();
    if (fg.lightnessF() < 0.5f) {
        bg = QColor::fromHsl(hue, 255, 220);
        stripe = Qt::white;
    } else {
        bg = QColor::fromHsl(hue, 255, 30);
        stripe = Qt::black;
    }
    m_stripes = QPixmap::fromImage(Utility::stripeImage(100, stripe, bg));
}


QCoro::Task<> AnnouncementsDialog::showNewAnnouncements(Announcements *announcements, QWidget *parent)
{
    if (!announcements || !announcements->hasNewAnnouncements())
        co_return;

    QString md;
    QVector<quint32> shownIds;
    const QVariantList vl = announcements->unreadAnnouncements();

    for (const QVariant &v : vl) {
        const QVariantMap vm = v.toMap();

        shownIds << vm.value(u"id"_qs).toUInt();

        if (!md.isEmpty())
            md = md + u"\n\n___\n\n";
        md = md + u"**" + vm.value(u"title"_qs).toString() + u"** &mdash; *"
                + QLocale().toString(vm.value(u"date"_qs).toDate(), QLocale::ShortFormat)
                + u"*\n\n" + vm.value(u"text"_qs).toString();
    }

    if (shownIds.isEmpty())
        co_return;

    AnnouncementsDialog dlg(md, parent);
    dlg.setWindowModality(Qt::ApplicationModal);
    dlg.show();

    if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
        for (const auto &id : std::as_const(shownIds))
            announcements->markAnnouncementRead(id);
    }
}

#include "moc_announcementsdialog.cpp"
