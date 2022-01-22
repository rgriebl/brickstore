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

#include <QItemDelegate>
#include <QPointer>
#include <QVector>
#include <QColor>
#include <QHash>
#include <QSet>
#include <QCache>

#include "common/documentmodel.h"

class SelectItemDialog;
class SelectColorDialog;

QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QTextLayout)

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
typedef uint qHashResult;
#else
typedef size_t qHashResult;
#endif


class DocumentDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    DocumentDelegate(QTableView *table);

    void setReadOnly(bool ro);
    bool isReadOnly() const;

    int defaultItemHeight(const QWidget *w = nullptr) const;

    QSize sizeHint(const QStyleOptionViewItem &option1, const QModelIndex &idx) const override;
    void paint(QPainter *p, const QStyleOptionViewItem &option1, const QModelIndex &idx) const override;
    bool editorEvent(QEvent *e, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &idx) override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void destroyEditor(QWidget *editor, const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void languageChange();

    static QString displayData(const QModelIndex &idx, const QVariant &display, bool toolTip);

protected:
    bool nonInlineEdit(QEvent *e, const QStyleOptionViewItem &option, const QModelIndex &idx);
    void setModelDataInternal(const QVariant &value, QAbstractItemModel *model,
                              const QModelIndex &index) const;

    static QColor shadeColor(int idx, qreal alpha = 0);

protected:
    QTableView *m_table;
    QPointer<SelectItemDialog> m_select_item;
    QPointer<SelectColorDialog> m_select_color;
    mutable QPointer<QLineEdit> m_lineedit;
    bool m_read_only = false;
    mutable QSet<quint64> m_elided;

    static QVector<QColor> s_shades;

    struct TextLayoutCacheKey {
        QString text;
        QSize size;
        uint fontSize;

        bool operator==(const TextLayoutCacheKey &other) const
        {
            return (text == other.text) && (size == other.size) && (fontSize == other.fontSize);
        }
    };
    friend qHashResult qHash(const DocumentDelegate::TextLayoutCacheKey &key, qHashResult seed);
    static QCache<TextLayoutCacheKey, QTextLayout> s_textLayoutCache;
};
