/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __CUNDO_H__
#define __CUNDO_H__

#include <QUndoStack>
#include <QUndoGroup>

class QAction;


class CUndoStack : public QUndoStack {
    Q_OBJECT

public:
    CUndoStack(QObject *parent = 0);

    QAction *createRedoAction(QObject *parent = 0) const;
    QAction *createUndoAction(QObject *parent = 0) const;

public slots:
    void redo(int count);
    void undo(int count);
};

class CUndoGroup : public QUndoGroup {
    Q_OBJECT

public:
    CUndoGroup(QObject *parent = 0);

    QAction *createRedoAction(QObject *parent = 0) const;
    QAction *createUndoAction(QObject *parent = 0) const;

public slots:
    void redo(int count);
    void undo(int count);
};

#endif
