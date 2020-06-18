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
#pragma once

#include <QDialog>
#include "bricklinkfwd.h"
#include "ui_selectitemdialog.h"


class SelectItemDialog : public QDialog, private Ui::SelectItemDialog
{
    Q_OBJECT
public:
    SelectItemDialog(bool only_with_inventory, QWidget *parent = nullptr);

    void setItemType(const BrickLink::ItemType *);
    void setItem(const BrickLink::Item *);
    const BrickLink::Item *item() const;

    int execAtPosition(const QRect &pos = QRect());

protected:
    void showEvent(QShowEvent *) override;

private slots:
    void checkItem(const BrickLink::Item *, bool);
};
