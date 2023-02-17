// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "bricklink/global.h"

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

protected:
    std::unique_ptr<SelectItemPrivate> d;
};
