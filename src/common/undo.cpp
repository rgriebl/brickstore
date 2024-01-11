// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
