/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include "bricklink/global.h"
#include "common/eventfilter.h"

QT_FORWARD_DECLARE_CLASS(QStringListModel)


class SelectItemPrivate;

class SelectItem : public QWidget
{
    Q_OBJECT
public:
    SelectItem(QWidget *parent = nullptr);
    ~SelectItem() override;

    bool hasExcludeWithoutInventoryFilter() const;
    void setExcludeWithoutInventoryFilter(bool b);

    const BrickLink::Color *colorFilter() const;
    void setColorFilter(const BrickLink::Color *color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    const BrickLink::Category *currentCategory() const;
    const BrickLink::ItemType *currentItemType() const;
    const BrickLink::Item *currentItem() const;

    void setCurrentCategory(const BrickLink::Category *cat);
    bool setCurrentItemType(const BrickLink::ItemType *it);
    bool setCurrentItem(const BrickLink::Item *item, bool force_items_category = false);

    double zoomFactor() const;
    void setZoomFactor(double zoom);

    void clearFilter();
    void setFilterFavoritesModel(QStringListModel *model);

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);
    static QByteArray defaultState();

signals:
    void hasColors(bool);
    void hasSubConditions(bool);
    void itemSelected(const BrickLink::Item *, bool confirmed);
    void showInColor(const BrickLink::Color *color);

public slots:
    void itemConfirmed();

protected slots:
    void applyFilter();
    void languageChange();
    void showContextMenu(const QPoint &);
    void setViewMode(int);

protected:
    void showEvent(QShowEvent *) override;
    void changeEvent(QEvent *e) override;

private:
    void init();
    void ensureSelectionVisible();
    void sortItems(int section, Qt::SortOrder order);
    EventFilter::Result zoomFilter(QObject *o, QEvent *e);

protected:
    std::unique_ptr<SelectItemPrivate> d;
};
