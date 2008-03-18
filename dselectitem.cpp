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
#include <QPushButton>

#include "dselectitem.h"
#include "cselectitem.h"
#include "utility.h"
#include "bricklink.h"


DSelectItem::DSelectItem(bool only_with_inventory, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);
    w_si->setExcludeWithoutInventoryFilter(only_with_inventory);

    connect(w_si, SIGNAL(itemSelected(const BrickLink::Item *, bool)), this, SLOT(checkItem(const BrickLink::Item *, bool)));

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_si);
}

void DSelectItem::setItemType(const BrickLink::ItemType *itt)
{
    w_si->setCurrentItemType(itt);
}

void DSelectItem::setItem(const BrickLink::Item *item)
{
    w_si->setCurrentItem(item);
}

const BrickLink::Item *DSelectItem::item() const
{
    return w_si->currentItem();
}

void DSelectItem::checkItem(const BrickLink::Item *item, bool ok)
{
    bool b = w_si->hasExcludeWithoutInventoryFilter() ? item->hasInventory() : true;

    QPushButton *p = w_buttons->button(QDialogButtonBox::Ok);
    p->setEnabled(item && b);

    if (item && b && ok)
        p->animateClick();
}

int DSelectItem::exec(const QRect &pos)
{
    if (pos.isValid())
        Utility::setPopupPos(this, pos);

    return QDialog::exec();
}

void DSelectItem::showEvent(QShowEvent *)
{
    activateWindow();
    w_si->setFocus();
}
