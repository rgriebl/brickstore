/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QTextBrowser>
#include <QPalette>
#include <QMenu>
#include <QLayout>
#include <QApplication>
#include <QAction>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QPainter>

#include "bricklink.h"
#include "picturewidget.h"


class PictureWidgetPrivate
{
public:
    BrickLink::Picture *m_pic = nullptr;
    QTextBrowser *      m_tlabel;
    bool                m_connected;
    int                 m_img_height;
    QImage              m_img;
};

class LargePictureWidgetPrivate
{
public:
    BrickLink::Picture *m_pic;
};

PictureWidget::PictureWidget(QWidget *parent)
    : QFrame(parent)
{
    d = new PictureWidgetPrivate();

    d->m_pic = 0;
    d->m_connected = false;

    setBackgroundRole(QPalette::Base);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setAutoFillBackground(true);
    setContextMenuPolicy(Qt::NoContextMenu);

    d->m_tlabel = new QTextBrowser(this);
    d->m_tlabel->setFrameStyle(QFrame::NoFrame);
    d->m_tlabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    d->m_tlabel->setLineWrapMode(QTextEdit::WidgetWidth);
    d->m_tlabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    d->m_tlabel->setContextMenuPolicy(Qt::NoContextMenu);
    QPalette pal = d->m_tlabel->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    d->m_tlabel->setPalette(pal);
    connect(d->m_tlabel, &QTextEdit::copyAvailable,
            this, [this](bool b) {
        d->m_tlabel->setContextMenuPolicy(b ? Qt::DefaultContextMenu : Qt::NoContextMenu);
    });

    QAction *a;
    a = new QAction(this);
    a->setObjectName("picture_reload");
    a->setIcon(QIcon(":/images/reload.png"));
    connect(a, &QAction::triggered,
            this, &PictureWidget::doUpdate);
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("picture_bl_catalog");
    a->setIcon(QIcon(":/images/edit_bl_catalog.png"));
    connect(a, &QAction::triggered,
            this, &PictureWidget::showBLCatalogInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName("picture_bl_priceguide");
    a->setIcon(QIcon(":/images/edit_bl_priceguide.png"));
    connect(a, &QAction::triggered,
            this, &PictureWidget::showBLPriceGuideInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName("picture_bl_lotsforsale");
    a->setIcon(QIcon(":/images/edit_bl_lotsforsale.png"));
    connect(a, &QAction::triggered,
            this, &PictureWidget::showBLLotsForSale);
    addAction(a);

    languageChange();
    redraw();
}

void PictureWidget::languageChange()
{
    findChild<QAction *> ("picture_reload")->setText(tr("Update"));
    findChild<QAction *> ("picture_bl_catalog")->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *> ("picture_bl_priceguide")->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *> ("picture_bl_lotsforsale")->setText(tr("Show Lots for Sale on BrickLink..."));
}

PictureWidget::~PictureWidget()
{
    if (d->m_pic)
        d->m_pic->release();

    delete d;
}

QSize PictureWidget::sizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return { 4 + 2 * d->m_default_size + 4, 4 + d->m_default_size + 4 + 5 * (fm.height() + 1) + 4 };
}

void PictureWidget::doUpdate()
{
    if (d->m_pic) {
        d->m_pic->update(true);
        redraw();
    }
}

void PictureWidget::showBLCatalogInfo()
{
    if (d->m_pic && d->m_pic->item())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_CatalogInfo, d->m_pic->item()));
}

void PictureWidget::showBLPriceGuideInfo()
{
    if (d->m_pic && d->m_pic->item() && d->m_pic->color())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo, d->m_pic->item(), d->m_pic->color()));
}

void PictureWidget::showBLLotsForSale()
{
    if (d->m_pic && d->m_pic->item() && d->m_pic->color())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_LotsForSale, d->m_pic->item(), d->m_pic->color()));
}

void PictureWidget::setPicture(BrickLink::Picture *pic)
{
    if (pic == d->m_pic)
        return;

    if (d->m_pic)
        d->m_pic->release();
    d->m_pic = pic;
    if (d->m_pic)
        d->m_pic->addRef();

    if (!d->m_connected && pic) {
        d->m_connected = connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
                                 this, &PictureWidget::gotUpdate);
    }

    setContextMenuPolicy(pic ? Qt::ActionsContextMenu : Qt::NoContextMenu);

    redraw();
}

BrickLink::Picture *PictureWidget::picture() const
{
    return d->m_pic;
}

void PictureWidget::gotUpdate(BrickLink::Picture *pic)
{
    if (pic == d->m_pic)
        redraw();
}

void PictureWidget::redraw()
{
    if (d->m_pic && (d->m_pic->updateStatus() == BrickLink::Updating)) {
        d->m_tlabel->setHtml(QLatin1String("<center><i>") +
                                           tr("Please wait ...updating") +
                                           QLatin1String("</i></center>"));
        d->m_img = QImage();
    }
    else if (d->m_pic && d->m_pic->valid()) {
        d->m_tlabel->setHtml(QLatin1String("<center><b>") +
                             d->m_pic->item()->id() +
                             QLatin1String("</b>&nbsp; ") +
                             d->m_pic->item()->name() +
                             QLatin1String("</center>"));
        d->m_img = d->m_pic->image();
        d->m_img_height = d->m_pic->image().height();
    }
    else {
        d->m_tlabel->setText(QString());
        d->m_img = QImage();
    }
    update();
}

void PictureWidget::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);

    QRect cr = contentsRect();
    if (cr.width() >= 80 && !d->m_img.isNull()) {
        QPainter p(this);
        p.setClipRect(e->rect());

        QPoint tl = cr.topLeft() + QPoint((cr.width() - d->m_img.width()) / 2, 4 + (80 - d->m_img_height) / 2);
        p.drawImage(tl, d->m_img);
    }
}

void PictureWidget::resizeEvent(QResizeEvent *e)
{
    QFrame::resizeEvent(e);

    QRect cr = contentsRect();
    d->m_tlabel->setGeometry(cr.left() + 4, cr.top() + 4 + 80 + 4, cr.width() - 2*4, cr.height() - (4 + 80 + 4 + 4));
}

bool PictureWidget::event(QEvent *e)
{
    if (d->m_pic && e && e->type() == QEvent::ToolTip) {
        return BrickLink::ToolTip::inst()->show(d->m_pic->item(), d->m_pic->color(),
                                                static_cast<QHelpEvent *>(e)->globalPos(), this);
    }
    return QFrame::event(e);
}

#include "moc_picturewidget.cpp"
