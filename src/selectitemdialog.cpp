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
#include <QPushButton>

#include "selectitemdialog.h"
#include "selectitem.h"
#include "utility.h"
#include "bricklink.h"


SelectItemDialog::SelectItemDialog(bool only_with_inventory, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    w_si->setExcludeWithoutInventoryFilter(only_with_inventory);

    connect(w_si, &SelectItem::itemSelected,
            this, &SelectItemDialog::checkItem);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_si);
}

void SelectItemDialog::setItemType(const BrickLink::ItemType *itt)
{
    w_si->setCurrentItemType(itt);
}

void SelectItemDialog::setItem(const BrickLink::Item *item)
{
    w_si->setCurrentItem(item);
}

const BrickLink::Item *SelectItemDialog::item() const
{
    return w_si->currentItem();
}

void SelectItemDialog::checkItem(const BrickLink::Item *item, bool ok)
{
    bool b = w_si->hasExcludeWithoutInventoryFilter() ? item->hasInventory() : true;

    QPushButton *p = w_buttons->button(QDialogButtonBox::Ok);
    p->setEnabled(item && b);

    if (item && b && ok)
        p->animateClick();
}

int SelectItemDialog::execAtPosition(const QRect &pos)
{
    if (pos.isValid())
        Utility::setPopupPos(this, pos);

    return QDialog::exec();
}

void SelectItemDialog::showEvent(QShowEvent *)
{
    activateWindow();
    w_si->setFocus();
}
