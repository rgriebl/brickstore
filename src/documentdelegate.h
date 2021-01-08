/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include <QItemDelegate>
#include <QPointer>
#include <QVector>
#include <QColor>
#include <QHash>
#include <QCache>

#include "document.h"

class SelectItemDialog;
class SelectColorDialog;

QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QTableView)


class DocumentDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    DocumentDelegate(Document *doc, DocumentProxyModel *view, QTableView *table);

    void setReadOnly(bool ro);
    bool isReadOnly() const;

    int defaultItemHeight(const QWidget *w = nullptr) const;

    QSize sizeHint(const QStyleOptionViewItem &option1, const QModelIndex &idx) const override;
    void paint(QPainter *p, const QStyleOptionViewItem &option1, const QModelIndex &idx) const override;
    bool editorEvent(QEvent *e, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &idx) override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

protected:
    bool nonInlineEdit(QEvent *e, Document::Item *it, const QStyleOptionViewItem &option, const QModelIndex &idx);

    QIcon::Mode iconMode(QStyle::State state) const;
    QIcon::State iconState(QStyle::State state) const;

    static QColor shadeColor(int idx, qreal alpha = 0);

protected:
    Document *m_doc;
    DocumentProxyModel *m_view;
    QTableView *m_table;
    SelectItemDialog * m_select_item = nullptr;
    SelectColorDialog *m_select_color = nullptr;
    mutable QPointer<QLineEdit> m_lineedit;
    bool m_read_only = false;
    mutable QList<quint64> m_elided;

    static QVector<QColor> s_shades;
    static QHash<int, QIcon> s_status_icons;
    static QCache<quint64, QPixmap> s_tag_cache;
    static QCache<int, QPixmap> s_stripe_cache;

    static void clearCaches();
};
