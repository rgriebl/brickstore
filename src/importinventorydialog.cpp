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
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>

#include "bricklink.h"
#include "config.h"
#include "import.h"
#include "progressdialog.h"
#include "utility.h"

#include "importinventorydialog.h"



ImportInventoryDialog::ImportInventoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_select->setExcludeWithoutInventoryFilter(true);
    auto itId = Config::inst()->value("/Defaults/ImportInventory/ItemType", 'S').value<char>();
    w_select->setCurrentItemType(BrickLink::core()->itemType(itId));
    connect(w_select, &SelectItem::itemSelected,
            this, &ImportInventoryDialog::checkItem);
    w_qty->setValue(1);
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
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

void ImportInventoryDialog::checkItem(const BrickLink::Item *it, bool ok)
{
    bool b = (it) && (w_select->hasExcludeWithoutInventoryFilter() ? it->hasInventory() : true);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(b);

    if (b && ok)
        w_buttons->button(QDialogButtonBox::Ok)->animateClick();
}

#include "moc_importinventorydialog.cpp"
