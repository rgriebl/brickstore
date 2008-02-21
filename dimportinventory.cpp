/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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
#include <QLineEdit>
#include <QValidator>
#include <QPushButton>
#include <QDateTimeEdit>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QHeaderView>
#include <QAbstractTableModel>
#include <QItemDelegate>
#include <QColorGroup>
#include <QVariant>

#include "cimport.h"
#include "cprogressdialog.h"
#include "cutility.h"

#include "dimportinventory.h"



DImportInventory::DImportInventory(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    w_select->setExcludeWithoutInventoryFilter(true);
    w_select->setCurrentItemType(BrickLink::core()->itemType(CConfig::inst()->value("/Defaults/ImportInventory/ItemType", 'S').toChar().toLatin1()));
    connect(w_select, SIGNAL(itemSelected(const BrickLink::Item *, bool)), this, SLOT(checkItem(const BrickLink::Item *, bool)));
    w_qty->setValue(1);
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
}

bool DImportInventory::setItem(const BrickLink::Item *item)
{
    return w_select->setCurrentItem(item);
}

const BrickLink::Item *DImportInventory::item() const
{
    return w_select->currentItem();
}

int DImportInventory::quantity() const
{
    return qMax(1, w_qty->value());
}

void DImportInventory::checkItem(const BrickLink::Item *it, bool ok)
{
    bool b = (it) && (w_select->hasExcludeWithoutInventoryFilter() ? it->hasInventory() : true);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(b);

    if (b && ok)
        w_buttons->button(QDialogButtonBox::Ok)->animateClick();
}
