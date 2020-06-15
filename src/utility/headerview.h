/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#ifndef HEADERVIEW_H
#define HEADERVIEW_H

#include <QHeaderView>

class HeaderView : public QHeaderView {
    Q_OBJECT
public:
    HeaderView(Qt::Orientation orientation, QWidget *parent = 0);
    
    bool isSectionAvailable(int section) const;
    void setSectionAvailable(int section, bool b);
    int availableSectionCount() const;

    void setModel(QAbstractItemModel*);

protected:
    bool viewportEvent(QEvent *e);

private:
    void showMenu(const QPoint &pos);

private slots:
    void sectionsRemoved(const QModelIndex&, int, int);

private:
    QList<int> m_unavailable;
};

#endif
