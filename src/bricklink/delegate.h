// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QStyledItemDelegate>

#include "bricklink/global.h"
#include "desktop/betteritemdelegate.h"


namespace BrickLink {

class ItemDelegate : public BetterItemDelegate
{
    Q_OBJECT
public:
    ItemDelegate(Options options = None, QAbstractItemView *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

public slots:
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
};


class ItemThumbsDelegate : public BrickLink::ItemDelegate
{
    Q_OBJECT
public:
    ItemThumbsDelegate(double initialZoom, QAbstractItemView *view = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    double zoomFactor() const;
    void setZoomFactor(double zoom);

signals:
    void zoomFactorChanged(double newZoom);

private:
    double m_zoom;
    QAbstractItemView *m_view;
};


class ToolTip : public QObject
{
    Q_OBJECT
public:
    static ToolTip *inst();

    bool show(const Item *item, const Color *color, const Category *category,
              const QPoint &globalPos, QWidget *parent);

private slots:
    void pictureUpdated(BrickLink::Picture *pic);

private:
    QString createItemToolTip(const Item *item, Picture *pic) const;
    QString createColorToolTip(const Color *color) const;
    QString createCategoryToolTip(const Category *category) const;

    static QString yearSpan(int from, int to);

    static ToolTip *s_inst;
    Picture *m_tooltip_pic = nullptr;
};

} // namespace BrickLink
