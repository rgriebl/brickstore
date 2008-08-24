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
#ifndef __DSELECTITEM_H__
#define __DSELECTITEM_H__

#include <QDialog>
#include "ui_selectitem.h"

class BrickLink::Item;

class DSelectItem : public QDialog, private Ui::SelectItem {
    Q_OBJECT

public:
    DSelectItem(bool only_with_inventory, QWidget *parent = 0, Qt::WindowFlags f = 0);

    void setItemType(const BrickLink::ItemType *);
    void setItem(const BrickLink::Item *);
    const BrickLink::Item *item() const;

    virtual int exec(const QRect &pos = QRect());

protected:
    virtual void showEvent(QShowEvent *);

private slots:
    void checkItem(const BrickLink::Item *, bool);
};

#endif
