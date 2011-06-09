/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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


class PictureWidgetPrivate {
public:
    BrickLink::Picture *m_pic;
    QTextBrowser *      m_tlabel;
    bool                m_connected;
    int                 m_img_height;
    QImage              m_img;
};

class LargePictureWidgetPrivate {
public:
    BrickLink::Picture *m_pic;
};

PictureWidget::PictureWidget(QWidget *parent, Qt::WindowFlags f)
        : QFrame(parent, f)
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
    connect(d->m_tlabel, SIGNAL(copyAvailable(bool)), this, SLOT(checkContextMenu(bool)));

    QAction *a;
    a = new QAction(this);
    a->setObjectName("picture_reload");
    a->setIcon(QIcon(":/images/reload.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(doUpdate()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("picture_magnify");
    a->setIcon(QIcon(":/images/viewmagp.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(viewLargeImage()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("picture_bl_catalog");
    a->setIcon(QIcon(":/images/edit_bl_catalog.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLCatalogInfo()));
    addAction(a);
    a = new QAction(this);
    a->setObjectName("picture_bl_priceguide");
    a->setIcon(QIcon(":/images/edit_bl_priceguide.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLPriceGuideInfo()));
    addAction(a);
    a = new QAction(this);
    a->setObjectName("picture_bl_lotsforsale");
    a->setIcon(QIcon(":/images/edit_bl_lotsforsale.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLLotsForSale()));
    addAction(a);

    languageChange();
    redraw();
}

void PictureWidget::languageChange()
{
    findChild<QAction *> ("picture_reload")->setText(tr("Update"));
    findChild<QAction *> ("picture_magnify")->setText(tr("View large image..."));
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
    return QSize(4 + 2*80 + 4, 4 + 80 + 4 + 5 * (fm.height() + 1) + 4);
}

void PictureWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    viewLargeImage();
}

void PictureWidget::doUpdate()
{
    if (d->m_pic) {
        d->m_pic->update(true);
        redraw();
    }
}

void PictureWidget::viewLargeImage()
{
    if (!d->m_pic)
        return;

    BrickLink::Picture *lpic = BrickLink::core()->largePicture(d->m_pic->item(), true);

    if (lpic) {
        LargePictureWidget *l = new LargePictureWidget(lpic, this);
        l->show();
        l->raise();
        l->activateWindow();
        l->setFocus();
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

    if (!d->m_connected && pic)
        d->m_connected = connect(BrickLink::core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(gotUpdate(BrickLink::Picture *)));

    setToolTip(pic ? tr("Double-click to view the large image.") : QString());
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
        d->m_tlabel->setText(QString::null);
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

void PictureWidget::checkContextMenu(bool b)
{
    d->m_tlabel->setContextMenuPolicy(b ? Qt::DefaultContextMenu : Qt::NoContextMenu);
}

// -------------------------------------------------------------------------


LargePictureWidget::LargePictureWidget(BrickLink::Picture *lpic, QWidget *parent)
        : QLabel(parent)
{
    d = new LargePictureWidgetPrivate();

    setWindowFlags(Qt::Tool | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);
    //setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);


    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setFrameStyle(QFrame::NoFrame);
    setAlignment(Qt::AlignCenter);
    setFixedSize(640, 480);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    setWordWrap(true);

    d->m_pic = lpic;
    if (d->m_pic)
        d->m_pic->addRef();

    connect(BrickLink::core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(gotUpdate(BrickLink::Picture *)));

    QAction *a;
    a = new QAction(this);
    a->setObjectName("picture_reload");
    a->setIcon(QIcon(":/images/reload.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(doUpdate()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("picture_close");
    a->setIcon(QIcon(":/images/file_close.png"));
    connect(a, SIGNAL(triggered()), this, SLOT(close()));
    addAction(a);

    languageChange();
    redraw();
}

void LargePictureWidget::languageChange()
{
    findChild<QAction *> ("picture_reload")->setText(tr("Update"));
    findChild<QAction *> ("picture_close")->setText(tr("Close"));
    setToolTip(tr("Double-click to close this window."));
}

LargePictureWidget::~LargePictureWidget()
{
    if (d->m_pic)
        d->m_pic->release();
    delete d;
}

void LargePictureWidget::gotUpdate(BrickLink::Picture *pic)
{
    if (pic == d->m_pic)
        redraw();
}

void LargePictureWidget::redraw()
{
    if (d->m_pic) {
        setWindowTitle(QString(d->m_pic->item()->id()) + " " + d->m_pic->item()->name());

        if (d->m_pic->updateStatus() == BrickLink::Updating)
            setText(QLatin1String("<center><i>") +
                    tr("Please wait ...updating") +
                    QLatin1String("</i></center>"));
        else if (d->m_pic->valid())
            setPixmap(d->m_pic->pixmap());
        else
            setText(QString::null);
    }
}

void LargePictureWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    e->accept();
    close();
}

void LargePictureWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return) {
        e->accept();
        close();
    }
}

void LargePictureWidget::doUpdate()
{
    if (d->m_pic)
        d->m_pic->update(true);
    redraw();
}


