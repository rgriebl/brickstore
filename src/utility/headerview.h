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


class HeaderView : public QHeaderView
{
    Q_OBJECT
public:
    HeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);
    
    bool isSectionAvailable(int section) const;
    void setSectionAvailable(int section, bool b);

    bool isSectionInternal(int section) const;
    void setSectionInternal(int section, bool internal);

    void setSectionHidden(int logicalIndex, bool hide);
    void showSection(int logicalIndex) { setSectionHidden(logicalIndex, false); }
    void hideSection(int logicalIndex) { setSectionHidden(logicalIndex, true); }

    void resizeSection(int logicalIndex, int size);

    void setModel(QAbstractItemModel *model) override;

    bool restoreLayout(const QByteArray &config);
    QByteArray saveLayout() const;

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
    QVector<int> m_unavailable;
    QVector<int> m_internal;
    QHash<int, int> m_hiddenSizes;
};
