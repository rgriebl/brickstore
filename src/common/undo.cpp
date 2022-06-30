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
#include "undo.h"


UndoStack::UndoStack(QObject *parent)
    : QUndoStack(parent)
{ }

void UndoStack::redoMultiple(int count)
{
    setIndex(index() + count);
}

void UndoStack::undoMultiple(int count)
{
    setIndex(index() - count);
}

void UndoStack::endMacro(const QString &str)
{
    int idx = index();
    QUndoStack::endMacro();
    if (index() == (idx+1)) {
        const_cast<QUndoCommand *>(command(idx))->setText(str);
        emit undoTextChanged(str);
    }
}


UndoGroup::UndoGroup(QObject *parent)
    : QUndoGroup(parent)
{ }

void UndoGroup::redoMultiple(int count)
{
    if (QUndoStack *st = activeStack())
        st->setIndex(st->index() + count);
}

void UndoGroup::undoMultiple(int count)
{
    if (QUndoStack *st = activeStack())
        st->setIndex(st->index() - count);
}


#include "moc_undo.cpp"
