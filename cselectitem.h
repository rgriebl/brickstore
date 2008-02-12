/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CSELECTITEM_H__
#define __CSELECTITEM_H__

#include <QDialog>

#include "bricklink.h"

class QListViewItem;
class QIconViewItem;

class CSelectItemPrivate;


class CSelectItem : public QWidget {
    Q_OBJECT
public:
    CSelectItem(QWidget *parent = 0);

    bool hasExcludeWithoutInventoryFilter() const;
    void setExcludeWithoutInventoryFilter(bool b);

    enum ViewMode {
        ListMode = 0,
        TableMode,
        ThumbsMode
    };

    virtual QSize sizeHint() const;

    const BrickLink::Category *currentCategory() const;
    const BrickLink::ItemType *currentItemType() const;
    const BrickLink::Item *currentItem() const;

    void setCurrentCategory(const BrickLink::Category *cat);
    void setCurrentItemType(const BrickLink::ItemType *it);
    bool setCurrentItem(const BrickLink::Item *item, bool dont_force_category = false);


signals:
    void hasColors(bool);
    void itemSelected(const BrickLink::Item *, bool);

public slots:
    void itemTypeChanged();
    void categoryChanged();
    void itemChanged();

    void showAsList();
    void showAsTable();
    void showAsThumbs();

    void itemConfirmed();

    void findItem();

protected slots:
    void applyFilter();
    void languageChange();

protected:
    virtual void showEvent(QShowEvent *);
    virtual void changeEvent(QEvent *);

private:
    void init();
    bool checkViewMode(ViewMode vm);
    void ensureSelectionVisible();
    void recalcHighlightPalette();

protected:
    CSelectItemPrivate *d;
};

class DSelectItem : public QDialog {
    Q_OBJECT
public:
    explicit DSelectItem(bool only_with_inventory, QWidget *parent = 0);

    void setItemType(const BrickLink::ItemType *);
    void setItem(const BrickLink::Item *);
    //bool setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter );

    const BrickLink::Item *item() const;

    virtual int exec(const QRect &pos = QRect());

private slots:
    void checkItem(const BrickLink::Item *, bool);

private:
    CSelectItem *w_si;

    QPushButton *w_ok;
    QPushButton *w_cancel;
};

#endif
