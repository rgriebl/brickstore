// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHeaderView>

QT_FORWARD_DECLARE_CLASS(QStyle)


class HeaderView : public QHeaderView
{
    Q_OBJECT

public:
    HeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);
    
    void setSectionHidden(int logicalIndex, bool hide);
    void showSection(int logicalIndex) { setSectionHidden(logicalIndex, false); }
    void hideSection(int logicalIndex) { setSectionHidden(logicalIndex, true); }

    void resizeSection(int logicalIndex, int size);

    void setModel(QAbstractItemModel *model) override;

    QVector<QPair<int, Qt::SortOrder>> sortColumns() const;
    void setSortColumns(const QVector<QPair<int, Qt::SortOrder>> &sortColumns);
    bool isSorted() const;
    void setSorted(bool b);

    QVector<int> visualColumnOrder() const;

    bool isConfigurable() const;
    void setConfigurable(bool configurable);

signals:
    void sortColumnsChanged(const QVector<QPair<int, Qt::SortOrder>> &sortColumns);
    void isSortedChanged(bool b);
    void visualColumnOrderChanged(const QVector<int> &newOrder);

protected:
    bool viewportEvent(QEvent *e) override;

private:
    void showMenu(const QPoint &pos);

    using QHeaderView::setSectionHidden;
    using QHeaderView::showSection;
    using QHeaderView::hideSection;
    using QHeaderView::resizeSection;

private slots:
    void sectionsRemoved(const QModelIndex &parent, int logicalFirst, int logicalLast);

private:
    QHash<int, int> m_hiddenSizes;
    QVector<QPair<int, Qt::SortOrder>> m_sortColumns;
    bool m_isSorted = false;
    bool m_isConfigurable = false;
};
