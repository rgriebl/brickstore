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
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QShortcut>
#include <QDesktopServices>

#include "bricklink.h"
#include "config.h"
#include "progressdialog.h"
#include "framework.h"
#include "importinventorydialog.h"


ImportInventoryDialog::ImportInventoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_select->setExcludeWithoutInventoryFilter(true);
    connect(w_select, &SelectItem::itemSelected,
            this, &ImportInventoryDialog::checkItem);
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/Geometry")).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/SelectItem"))
            .toByteArray();
    if (!w_select->restoreState(ba)) {
        w_select->restoreState(SelectItem::defaultState());
        w_select->setCurrentItemType(BrickLink::core()->itemType('S'));
    }

    ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/Details"))
            .toByteArray();
    restoreState(ba);

    if (QAction *a = FrameWork::inst()->findAction("bricklink_catalog")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select->currentItem();
            if (item)
                QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_CatalogInfo, item));
        });
    }
    if (QAction *a = FrameWork::inst()->findAction("bricklink_priceguide")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select->currentItem();
            if (item && !item->itemType()->hasColors())
                QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo, item));
        });
    }
    if (QAction *a = FrameWork::inst()->findAction("bricklink_lotsforsale")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select->currentItem();
            if (item && !item->itemType()->hasColors())
                QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_LotsForSale, item));
        });
    }
}

ImportInventoryDialog::~ImportInventoryDialog()
{
    Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Geometry", saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportInventoryDialog/SelectItem", w_select->saveState());
    Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Details", saveState());
}

bool ImportInventoryDialog::setItem(const BrickLink::Item *item)
{
    return w_select->setCurrentItem(item);
}

const BrickLink::Item *ImportInventoryDialog::item() const
{
    return w_select->currentItem();
}

int ImportInventoryDialog::quantity() const
{
    return qMax(1, w_qty->value());
}

BrickLink::Condition ImportInventoryDialog::condition() const
{
    return w_condition_used->isChecked() ? BrickLink::Condition::Used : BrickLink::Condition::New;
}

void ImportInventoryDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    activateWindow();
    w_select->setFocus();
}

QSize ImportInventoryDialog::sizeHint() const
{
    QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance("m") * 120, fm.height() * 30);
}

QByteArray ImportInventoryDialog::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("II") << qint32(1)
       << w_condition_new->isChecked()
       << w_qty->value();
    return ba;
}

bool ImportInventoryDialog::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version != 1))
        return false;

    bool isNew;
    int qty;

    ds >> isNew >> qty;

    if (ds.status() != QDataStream::Ok)
        return false;

    w_condition_new->setChecked(isNew);
    w_condition_used->setChecked(!isNew);
    w_qty->setValue(qty);
    return true;
}

void ImportInventoryDialog::checkItem(const BrickLink::Item *it, bool ok)
{
    bool b = (it) && (w_select->hasExcludeWithoutInventoryFilter() ? it->hasInventory() : true);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(b);

    if (b && ok)
        w_buttons->button(QDialogButtonBox::Ok)->animateClick();
}

#include "moc_importinventorydialog.cpp"
