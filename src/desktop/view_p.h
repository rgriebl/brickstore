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

#include <QUndoCommand>
#include <QTableView>
#include <QKeyEvent>


class View;
class DocumentModel;
class HeaderView;


class ColumnChangeWatcher : public QObject
{
    Q_OBJECT
public:
    ColumnChangeWatcher(View *view, HeaderView *header);

    void moveColumn(int logical, int newVisual);
    void resizeColumn(int logical, int newSize);
    void hideColumn(int logical, bool newHidden);

    void enable();
    void disable();

signals:
    void internalColumnMoved(int logical, int oldVisual, int newVisual);
    void internalColumnResized(int logical, int oldSize, int newSize);

private:
    View *m_view;
    HeaderView *m_header;
    bool m_resizing = false;
    bool m_moving = false;
    bool m_hiding = false;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class TableView : public QTableView
{
    Q_OBJECT
public:
    explicit TableView(QWidget *parent = nullptr);

    void editCurrentItem(int column)
    {
        auto idx = currentIndex();
        if (!idx.isValid())
            return;
        idx = idx.siblingAtColumn(column);
        QKeyEvent kp(QEvent::KeyPress, Qt::Key_Execute, Qt::NoModifier);
        edit(idx, AllEditTriggers, &kp);
    }

    using QTableView::initViewItemOption;

protected:
    void keyPressEvent(QKeyEvent *e) override;
};
