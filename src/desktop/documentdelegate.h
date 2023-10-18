// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QItemDelegate>
#include <QPointer>
#include <QVector>
#include <QColor>
#include <QHash>
#include <QSet>
#include <QCache>


class SelectItemDialog;
class SelectColorDialog;

QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QTextLayout)


class DocumentDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    DocumentDelegate(QTableView *table);

    void setReadOnly(bool ro);
    bool isReadOnly() const;

    static int defaultItemHeight(const QWidget *w = nullptr);

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

protected:
    bool nonInlineEdit(QEvent *e, const QStyleOptionViewItem &option, const QModelIndex &idx);
    void setModelDataInternal(const QVariant &value, QAbstractItemModel *model,
                              const QModelIndex &index) const;

    static QColor shadeColor(int idx, float alpha = 0);

protected:
    QTableView *m_table;
    QPointer<SelectItemDialog> m_select_item;
    QPointer<SelectColorDialog> m_select_color;
    mutable QPointer<QLineEdit> m_lineedit;
    bool m_read_only = false;
    mutable QSet<quint64> m_elided;

    struct TextLayoutCacheKey {
        QString text;
        QSize size;
        uint fontSize;

        bool operator==(const TextLayoutCacheKey &other) const = default;
    };
    friend size_t qHash(const DocumentDelegate::TextLayoutCacheKey &key, size_t seed);
    static QCache<TextLayoutCacheKey, QTextLayout> s_textLayoutCache;
    static bool s_textLayoutCacheClearConnected;
};
