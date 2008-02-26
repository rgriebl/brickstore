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
#include <QMenu>
#include <QVariant>
#include <QAction>
#include <QHeaderView>
#include <QDesktopServices>
#include <QToolTip>
#include <QApplication>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QHelpEvent>

#include "qtemporaryresource.h"

#include "cframework.h"
#include "cpicturewidget.h"

#include "cappearsinwidget.h"


class AppearsInDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    AppearsInDelegate(QObject *parent)
        : QStyledItemDelegate(parent), m_tooltip_pic(0)
    {
        connect(BrickLink::inst(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(pictureUpdated(BrickLink::Picture *)));
    }
    
public slots: // this is really is a faked virtual
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
    {
        if (event->type() == QEvent::ToolTip && index.isValid()) {
            const BrickLink::AppearsInModel *model = qobject_cast<const BrickLink::AppearsInModel *>(index.model());          
            if (!model)
                goto out;

            const BrickLink::Item::AppearsInItem *appears = model->appearsIn(index);            
            if (!appears)
                goto out;
                
            BrickLink::Picture *pic = BrickLink::inst()->picture(appears->second, appears->second->defaultColor(), true);

            if (!pic)
                goto out;

            QTemporaryResource::registerResource("#/appears_in_set_tooltip_picture.png", pic->valid() ? pic->image() : QImage());
            m_tooltip_pic = (pic->updateStatus() == BrickLink::Updating) ? pic : 0;

            // need to 'clear' to reset the image cache of the QTextDocument
            foreach (QWidget *w, QApplication::topLevelWidgets()) {
                if (w->inherits("QTipLabel")) {
                    qobject_cast<QLabel *>(w)->clear();
                    break;
                }
            }
            QString tt = createToolTip(appears->second, pic);
            if (!tt.isEmpty()) {
                QToolTip::showText(event->globalPos(), tt, view);
                return true;
            }
        }
    out:    
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    
private:
    QString createToolTip(const BrickLink::Item *item, BrickLink::Picture *pic) const
    {
        QString str = QLatin1String("<div class=\"appearsin\"><table><tr><td rowspan=\"2\">%1</td><td><b>%2</b></td></tr><tr><td>%3</td></tr></table></div>");
        QString img_left = QLatin1String("<img src=\"#/appears_in_set_tooltip_picture.png\" />");
        QString note_left = QLatin1String("<i>") + CAppearsInWidget::tr("[Image is loading]") + QLatin1String("</i>");

        if (pic && (pic->updateStatus() == BrickLink::Updating))
            return str.arg(note_left).arg(item->id()).arg(item->name());
        else
            return str.arg(img_left).arg(item->id()).arg(item->name());
    }

private slots:
    void pictureUpdated(BrickLink::Picture *pic)
    {
        if (!pic || pic != m_tooltip_pic)
            return;

        m_tooltip_pic = 0;

        if (QToolTip::isVisible() && QToolTip::text().startsWith("<div class=\"appearsin\">")) {
            QTemporaryResource::registerResource("#/appears_in_set_tooltip_picture.png", pic->image());

            //xx QPoint pos;
            foreach (QWidget *w, QApplication::topLevelWidgets()) {
                if (w->inherits("QTipLabel")) {
                    //xx pos = w->pos();
                    QSize extra = w->size() - w->sizeHint();
                    qobject_cast<QLabel *>(w)->clear();
                    qobject_cast<QLabel *>(w)->setText(createToolTip(pic->item(), pic));
                    w->resize(w->sizeHint() + extra);
                    break;
                }
            }
            //xx if (!pos.isNull()) {
            //xx     QToolTip::showText(pos, createToolTip(pic->item(), pic));
        }
    }
    
private:
    BrickLink::Picture *m_tooltip_pic;
};


class CAppearsInWidgetPrivate {
public:
    const BrickLink::Item * m_item;
    const BrickLink::Color *m_color;
    QTimer *                m_resize_timer;
};

CAppearsInWidget::CAppearsInWidget(QWidget *parent)
    : QTreeView(parent)
{
    d = new CAppearsInWidgetPrivate();
    d->m_resize_timer = new QTimer(this);

    setAlternatingRowColors(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setSortingEnabled(true);
    sortByColumn(0);
    header()->setSortIndicatorShown(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setItemDelegate(new AppearsInDelegate(this));

    QAction *a;
    a = new QAction(this);
    a->setObjectName("edit_partoutitems");
    a->setIcon(QIcon(":/images/22x22/edit_partoutitems"));
    connect(a, SIGNAL(triggered()), this, SLOT(partOut()));
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

    connect(d->m_resize_timer, SIGNAL(timeout()), this, SLOT(resizeColumns()));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(partOut()));

    languageChange();
    setItem(0, 0);
}

void CAppearsInWidget::languageChange()
{
    findChild<QAction *>("edit_partoutitems")->setText(tr("Part out Item..."));
    findChild<QAction *>("edit_magnify")->setText(tr("View large image..."));
    findChild<QAction *>("edit_bl_catalog")->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *>("edit_bl_priceguide")->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *>("edit_bl_lotsforsale")->setText(tr("Show Lots for Sale on BrickLink..."));
}

CAppearsInWidget::~CAppearsInWidget()
{
    delete d;
}

void CAppearsInWidget::showContextMenu(const QPoint &pos)
{
    if (appearsIn())
        QMenu::exec(actions(), viewport()->mapToGlobal(pos));
}


const BrickLink::Item::AppearsInItem *CAppearsInWidget::appearsIn() const
{
    BrickLink::AppearsInModel *m = qobject_cast<BrickLink::AppearsInModel *>(model());

    if (m && !selectionModel()->selectedIndexes().isEmpty())
        return m->appearsIn(selectionModel()->selectedIndexes().front());
    else
        return 0;
}

void CAppearsInWidget::partOut()
{
    const BrickLink::Item::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        CFrameWork::inst()->fileImportBrickLinkInventory(ai->second);
}

QSize CAppearsInWidget::minimumSizeHint() const
{
    const QFontMetrics &fm = fontMetrics();

    return QSize(fm.width('m') * 20, fm.height() * 6);
}

QSize CAppearsInWidget::sizeHint() const
{
    return minimumSizeHint() * 2;
}

void CAppearsInWidget::setItem(const BrickLink::Item *item, const BrickLink::Color *color)
{
    d->m_item = item;
    d->m_color = color;

    setModel(BrickLink::core()->appearsInModel(d->m_item , d->m_color));

    if (item) {
        d->m_resize_timer->start(100);
    }
    else {
        d->m_resize_timer->stop();
        resizeColumns();
    }
}

void CAppearsInWidget::resizeColumns()
{
    setUpdatesEnabled(false);
    resizeColumnToContents(0);
    resizeColumnToContents(1);
    setUpdatesEnabled(true);
}

void CAppearsInWidget::viewLargeImage()
{
    const BrickLink::Item::AppearsInItem *ai = appearsIn();

    if (ai && ai->second) {
        BrickLink::Picture *lpic = BrickLink::inst()->largePicture(ai->second, true);

        if (lpic) {
            CLargePictureWidget *l = new CLargePictureWidget(lpic, this);
            l->show();
            l->raise();
            l->activateWindow();
            l->setFocus();
        }
    }
}

void CAppearsInWidget::showBLCatalogInfo()
{
    const BrickLink::Item::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        QDesktopServices::openUrl(BrickLink::inst()->url(BrickLink::URL_CatalogInfo, ai->second));
}

void CAppearsInWidget::showBLPriceGuideInfo()
{
    const BrickLink::Item::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        QDesktopServices::openUrl(BrickLink::inst()->url(BrickLink::URL_PriceGuideInfo, ai->second, BrickLink::inst()->color(0)));
}

void CAppearsInWidget::showBLLotsForSale()
{
    const BrickLink::Item::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        QDesktopServices::openUrl(BrickLink::inst()->url(BrickLink::URL_LotsForSale, ai->second, BrickLink::inst()->color(0)));
}

#include "cappearsinwidget.moc"
