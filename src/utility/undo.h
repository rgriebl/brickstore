/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __UNDO_H__
#define __UNDO_H__

#include <QUndoStack>
#include <QUndoGroup>

class QAction;


class UndoStack : public QUndoStack {
    Q_OBJECT

public:
    UndoStack(QObject *parent = 0);

    QAction *createRedoAction(QObject *parent = 0) const;
    QAction *createUndoAction(QObject *parent = 0) const;

    // workaround as long as I haven't add that to Qt (4.6 hopefully)
    void endMacro(const QString &str);

public slots:
    void redo(int count);
    void undo(int count);
};

class UndoGroup : public QUndoGroup {
    Q_OBJECT

public:
    UndoGroup(QObject *parent = 0);

    QAction *createRedoAction(QObject *parent = 0) const;
    QAction *createUndoAction(QObject *parent = 0) const;

public slots:
    void redo(int count);
    void undo(int count);
};

#endif
