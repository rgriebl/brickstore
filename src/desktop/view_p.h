// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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

    void editCurrentItem(int column);

    using QTableView::initViewItemOption;

protected:
    void changeEvent(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

private:
    void updateMinimumSectionSize();
};
