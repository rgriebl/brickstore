// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QUndoStack>
#include <QUndoGroup>

QT_FORWARD_DECLARE_CLASS(QAction)


class UndoStack : public QUndoStack
{
    Q_OBJECT

public:
    UndoStack(QObject *parent = nullptr);

    // workaround as long as I haven't added that to Qt
    void endMacro(const QString &str);

public slots:
    void redoMultiple(int count);
    void undoMultiple(int count);
};


class UndoGroup : public QUndoGroup
{
    Q_OBJECT

public:
    UndoGroup(QObject *parent = nullptr);

public slots:
    void redoMultiple(int count);
    void undoMultiple(int count);
};
