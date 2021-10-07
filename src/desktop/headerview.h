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

#include <QHeaderView>

QT_FORWARD_DECLARE_CLASS(QStyle)


class HeaderView : public QHeaderView
{
    Q_OBJECT
public:
    HeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~HeaderView() override;
    
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

private:
    bool restoreLayout(const QByteArray &config);
    QByteArray saveLayout() const;

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
    QStyle *m_proxyStyle;
    QHash<int, int> m_hiddenSizes;
    QVector<QPair<int, Qt::SortOrder>> m_sortColumns;
    bool m_isSorted = false;
};
