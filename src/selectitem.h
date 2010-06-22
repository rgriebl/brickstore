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
#ifndef __SELECTITEM_H__
#define __SELECTITEM_H__

#include <QDialog>

#include "bricklinkfwd.h"

class QListViewItem;
class QIconViewItem;

class SelectItemPrivate;


class SelectItem : public QWidget {
    Q_OBJECT
public:
    SelectItem(QWidget *parent = 0);

    bool hasExcludeWithoutInventoryFilter() const;
    void setExcludeWithoutInventoryFilter(bool b);

    virtual QSize sizeHint() const;

    const BrickLink::Category *currentCategory() const;
    const BrickLink::ItemType *currentItemType() const;
    const BrickLink::Item *currentItem() const;

    bool setCurrentCategory(const BrickLink::Category *cat);
    bool setCurrentItemType(const BrickLink::ItemType *it);
    bool setCurrentItem(const BrickLink::Item *item, bool force_items_category = false);


signals:
    void hasColors(bool);
    void itemSelected(const BrickLink::Item *, bool);

public slots:
    void itemTypeChanged();
    void categoryChanged();
    void itemChanged();

    void itemConfirmed();

    void findItem();

protected slots:
    void applyFilter();
    void languageChange();
    void showContextMenu(const QPoint &);
    void setViewMode(int);

protected:
    virtual void showEvent(QShowEvent *);
    virtual bool eventFilter(QObject *o, QEvent *e);

private:
    void init();
    void ensureSelectionVisible();

protected:
    SelectItemPrivate *d;
};



#endif
