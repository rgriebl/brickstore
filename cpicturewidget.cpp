/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <QLabel>
#include <QMenu>
#include <QLayout>
#include <QApplication>
#include <QAction>
#include <QDesktopServices>
#include <QMouseEvent>

#include "cpicturewidget.h"


class CPictureWidgetPrivate {
public:
    BrickLink::Picture *m_pic;
    QLabel *            m_plabel;
    QLabel *            m_tlabel;
    bool                m_connected;
};

class CLargePictureWidgetPrivate {
public:
    BrickLink::Picture *m_pic;
};

CPictureWidget::CPictureWidget(QWidget *parent, Qt::WindowFlags f)
        : QFrame(parent, f)
{
    d = new CPictureWidgetPrivate();

    d->m_pic = 0;
    d->m_connected = false;

    setBackgroundRole(QPalette::Base);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setAutoFillBackground(true);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    int fw = frameWidth() * 2;
    d->m_plabel = new QLabel(this);
    d->m_plabel->setAlignment(Qt::AlignCenter);
    d->m_plabel->setFixedSize(80, 80);
    d->m_plabel->setContextMenuPolicy(Qt::NoContextMenu);

    d->m_tlabel = new QLabel("Ay<br />Ay<br />Ay<br />Ay<br />Ay", this);
    d->m_tlabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    d->m_tlabel->setWordWrap(true);
    d->m_tlabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    d->m_tlabel->setFixedSize(2 * d->m_plabel->width(), d->m_tlabel->sizeHint().height());
    d->m_tlabel->setText(QString());
    d->m_tlabel->setContextMenuPolicy(Qt::NoContextMenu);

    QBoxLayout *lay = new QVBoxLayout(this);
    lay->setMargin(fw + 4);
    lay->setSpacing(4);
    lay->addWidget(d->m_plabel, 0, Qt::AlignCenter );
    lay->addWidget(d->m_tlabel, 0, Qt::AlignCenter );

    QAction *a;
    a = new QAction(this);
    a->setObjectName("edit_reload");
    a->setIcon(QIcon(":/images/22x22/reload"));
    connect(a, SIGNAL(triggered()), this, SLOT(doUpdate()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("edit_magnify");
    a->setIcon(QIcon(":/images/22x22/viewmagp"));
    connect(a, SIGNAL(triggered()), this, SLOT(viewLargeImage()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("edit_bl_catalog");
    a->setIcon(QIcon(":/images/22x22/edit_bl_catalog"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLCatalogInfo()));
    addAction(a);
    a = new QAction(this);
    a->setObjectName("edit_bl_priceguide");
    a->setIcon(QIcon(":/images/22x22/edit_bl_priceguide"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLPriceGuideInfo()));
    addAction(a);
    a = new QAction(this);
    a->setObjectName("edit_bl_lotsforsale");
    a->setIcon(QIcon(":/images/22x22/edit_bl_lotsforsale"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLLotsForSale()));
    addAction(a);

    languageChange();
    redraw();
}

void CPictureWidget::languageChange()
{
    findChild<QAction *> ("edit_reload")->setText(tr("Update"));
    findChild<QAction *> ("edit_magnify")->setText(tr("View large image..."));
    findChild<QAction *> ("edit_bl_catalog")->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *> ("edit_bl_priceguide")->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *> ("edit_bl_lotsforsale")->setText(tr("Show Lots for Sale on BrickLink..."));
}

CPictureWidget::~CPictureWidget()
{
    if (d->m_pic)
        d->m_pic->release();

    delete d;
}

QSize CPictureWidget::sizeHint() const
{
    return layout()->minimumSize();
}

void CPictureWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    viewLargeImage();
}

void CPictureWidget::doUpdate()
{
    if (d->m_pic) {
        d->m_pic->update(true);
        redraw();
    }
}

void CPictureWidget::viewLargeImage()
{
    if (!d->m_pic)
        return;

    BrickLink::Picture *lpic = BrickLink::inst()->largePicture(d->m_pic->item(), true);

    if (lpic) {
        CLargePictureWidget *l = new CLargePictureWidget(lpic, this);
        l->show();
        l->raise();
        l->activateWindow();
        l->setFocus();
    }
}

void CPictureWidget::showBLCatalogInfo()
{
    if (d->m_pic && d->m_pic->item())
        QDesktopServices::openUrl(BrickLink::inst()->url(BrickLink::URL_CatalogInfo, d->m_pic->item()));
}

void CPictureWidget::showBLPriceGuideInfo()
{
    if (d->m_pic && d->m_pic->item() && d->m_pic->color())
        QDesktopServices::openUrl(BrickLink::inst()->url(BrickLink::URL_PriceGuideInfo, d->m_pic->item(), d->m_pic->color()));
}

void CPictureWidget::showBLLotsForSale()
{
    if (d->m_pic && d->m_pic->item() && d->m_pic->color())
        QDesktopServices::openUrl(BrickLink::inst()->url(BrickLink::URL_LotsForSale, d->m_pic->item(), d->m_pic->color()));
}

void CPictureWidget::setPicture(BrickLink::Picture *pic)
{
    if (pic == d->m_pic)
        return;

    if (d->m_pic)
        d->m_pic->release();
    d->m_pic = pic;
    if (d->m_pic)
        d->m_pic->addRef();

    if (!d->m_connected && pic)
        d->m_connected = connect(BrickLink::inst(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(gotUpdate(BrickLink::Picture *)));

    setToolTip(pic ? tr("Double-click to view the large image.") : QString());

    redraw();
}

BrickLink::Picture *CPictureWidget::picture() const
{
    return d->m_pic;
}

void CPictureWidget::gotUpdate(BrickLink::Picture *pic)
{
    if (pic == d->m_pic)
        redraw();
}

void CPictureWidget::redraw()
{
    if (d->m_pic && (d->m_pic->updateStatus() == BrickLink::Updating)) {
        d->m_tlabel->setText(QLatin1String("<center><i>") +
                                           tr("Please wait ...updating") +
                                           QLatin1String("</i></center>"));
        d->m_plabel->setPixmap(QPixmap());
    }
    else if (d->m_pic && d->m_pic->valid()) {
        d->m_tlabel->setText(QLatin1String("<center><b>") +
                             d->m_pic->item()->id() +
                             QLatin1String("</b>&nbsp; ") +
                             d->m_pic->item()->name() +
                             QLatin1String("</center>"));
        d->m_plabel->setPixmap(d->m_pic->pixmap());
    }
    else {
        d->m_tlabel->setText(QString::null);
        d->m_plabel->setPixmap(QPixmap());
    }
}


// -------------------------------------------------------------------------


CLargePictureWidget::CLargePictureWidget(BrickLink::Picture *lpic, QWidget *parent)
        : QLabel(parent)
{
    d = new CLargePictureWidgetPrivate();

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
    a->setObjectName("edit_reload");
    a->setIcon(QIcon(":/images/22x22/reload"));
    connect(a, SIGNAL(triggered()), this, SLOT(doUpdate()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("edit_close");
    a->setIcon(QIcon(":/images/22x22/file_close"));
    connect(a, SIGNAL(triggered()), this, SLOT(close()));
    addAction(a);

    languageChange();
    redraw();
}

void CLargePictureWidget::languageChange()
{
    findChild<QAction *> ("edit_reload")->setText(tr("Update"));
    findChild<QAction *> ("edit_close")->setText(tr("Close"));
    setToolTip(tr("Double-click to close this window."));
}

CLargePictureWidget::~CLargePictureWidget()
{
    if (d->m_pic)
        d->m_pic->release();
    delete d;
}

void CLargePictureWidget::gotUpdate(BrickLink::Picture *pic)
{
    if (pic == d->m_pic)
        redraw();
}

void CLargePictureWidget::redraw()
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

void CLargePictureWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    e->accept();
    close();
}

void CLargePictureWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return) {
        e->accept();
        close();
    }
}

void CLargePictureWidget::doUpdate()
{
    if (d->m_pic)
        d->m_pic->update(true);
    redraw();
}


