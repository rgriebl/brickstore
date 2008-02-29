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
#include <QLayout>
#include <QList>
#include <QPushButton>
#include <QHash>
#include <QApplication>
#include <QHeaderView>
#include <QTreeView>
#include <QItemDelegate>

#include "cutility.h"
//#include "clistview.h"
#include "cselectcolor.h"


class ColorDelegate : public QItemDelegate {
public:
    ColorDelegate(QObject *parent = 0)
            : QItemDelegate(parent)
    { }

    virtual void drawDecoration(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QPixmap &pixmap) const
    {
        QStyleOptionViewItemV4 myoption(option);
        myoption.state &= ~QStyle::State_Selected;

        QItemDelegate::drawDecoration(painter, myoption, rect, pixmap);
    }
};



CSelectColor::CSelectColor(QWidget *parent, Qt::WindowFlags f)
        : QWidget(parent, f)
{
    w_colors = new QTreeView(this);
    w_colors->setItemDelegate(new ColorDelegate());
    w_colors->setAlternatingRowColors(true);
    w_colors->setAllColumnsShowFocus(true);
    w_colors->setUniformRowHeights(true);
    w_colors->setRootIsDecorated(false);
    w_colors->setSortingEnabled(true);

    w_colors->setModel(BrickLink::core()->colorModel());

    w_colors->resizeColumnToContents(0);
    w_colors->setFixedWidth(w_colors->columnWidth(0) + 2 * w_colors->frameWidth() + w_colors->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4);

    w_colors->sortByColumn(0);

    setFocusProxy(w_colors);

    connect(w_colors->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(colorChanged()));
    connect(w_colors, SIGNAL(activated(const QModelIndex &)), this, SLOT(colorConfirmed()));

    QBoxLayout *lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->addWidget(w_colors);
}


const BrickLink::Color *CSelectColor::currentColor() const
{
    BrickLink::ColorModel *model = qobject_cast<BrickLink::ColorModel *>(w_colors->model());

    if (model && w_colors->selectionModel()->hasSelection()) {
        QModelIndex idx = w_colors->selectionModel()->selectedIndexes().front();
        return model->color(idx);
    }
    else
        return 0;
}

void CSelectColor::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            setCurrentColor(BrickLink::core()->color(0));
    }
    QWidget::changeEvent(e);
}

void CSelectColor::setCurrentColor(const BrickLink::Color *color)
{
    BrickLink::ColorModel *model = qobject_cast<BrickLink::ColorModel *>(w_colors->model());

    if (model)
        w_colors->selectionModel()->select(model->index(color), QItemSelectionModel::SelectCurrent);
}

void CSelectColor::colorChanged()
{
    emit colorSelected(currentColor(), false);
}

void CSelectColor::colorConfirmed()
{
    emit colorSelected(currentColor(), true);
}

void CSelectColor::showEvent(QShowEvent *)
{
    if (w_colors->selectionModel()->hasSelection())
        w_colors->scrollTo(w_colors->selectionModel()->selectedIndexes().front(), QAbstractItemView::PositionAtCenter);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


DSelectColor::DSelectColor(QWidget *parent, Qt::WindowFlags f)
        : QDialog(parent, f)
{
    w_sc = new CSelectColor(this);
    w_sc->w_colors->setMaximumSize(32767, 32767);    // fixed width ->minimum width in this case

    w_ok = new QPushButton(tr("&OK"), this);
    w_ok->setAutoDefault(true);
    w_ok->setDefault(true);

    w_cancel = new QPushButton(tr("&Cancel"), this);
    w_cancel->setAutoDefault(true);

    QFrame *hline = new QFrame(this);
    hline->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    QBoxLayout *toplay = new QVBoxLayout(this);
    toplay->addWidget(w_sc);
    toplay->addWidget(hline);

    QBoxLayout *butlay = new QHBoxLayout();
    toplay->addLayout(butlay);
    butlay->addStretch(60);
    butlay->addWidget(w_ok, 15);
    butlay->addWidget(w_cancel, 15);

    setSizeGripEnabled(true);
    setMinimumSize(minimumSizeHint());

    connect(w_ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(w_cancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(w_sc, SIGNAL(colorSelected(const BrickLink::Color *, bool)), this, SLOT(checkColor(const BrickLink::Color *, bool)));

    w_ok->setEnabled(false);
    w_sc->setFocus();
}

void DSelectColor::setColor(const BrickLink::Color *col)
{
    w_sc->setCurrentColor(col);
}

const BrickLink::Color *DSelectColor::color() const
{
    return w_sc->currentColor();
}

void DSelectColor::checkColor(const BrickLink::Color *col, bool ok)
{
    w_ok->setEnabled((col));

    if (col && ok)
        w_ok->animateClick();
}

int DSelectColor::exec(const QRect &pos)
{
    if (pos.isValid())
        CUtility::setPopupPos(this, pos);

    return QDialog::exec();
}

