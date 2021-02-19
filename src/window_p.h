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

#include <QUndoCommand>
#include <QFrame>
#include <QTableView>

QT_FORWARD_DECLARE_CLASS(QProgressDialog)
QT_FORWARD_DECLARE_CLASS(QWidget)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QLabel)

class Window;
class Document;
class HeaderView;

enum {
    CID_Column = 0x0001000,
};


class ColumnChangeWatcher : public QObject
{
    Q_OBJECT
public:
    ColumnChangeWatcher(Window *window, HeaderView *header);

    void moveColumn(int logical, int oldVisual, int newVisual);
    void resizeColumn(int logical, int oldSize, int newSize);

    void enable();
    void disable();

    QString columnTitle(int logical) const;

signals:
    void internalColumnMoved(int logical, int oldVisual, int newVisual);
    void internalColumnResized(int logical, int oldSize, int newSize);

private:
    Window *m_window;
    HeaderView *m_header;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class ColumnCmd : public QUndoCommand
{
public:
    enum class Type { Move, Resize };

    ColumnCmd(ColumnChangeWatcher *ccw, bool alreadyDone, Type type, int logical, int oldValue, int newValue);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    ColumnChangeWatcher *m_ccw;
    bool m_skipFirstRedo;
    Type m_type;
    int m_logical;
    int m_oldValue;
    int m_newValue;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class WindowProgressHelper : public QObject
{
public:
    WindowProgressHelper(QProgressDialog *pd);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class WindowProgress
{
public:
    explicit WindowProgress(QWidget *w, const QString &title = QString(), int total = 0);
    ~WindowProgress();

    void stop();
    void step(int d = 1);
    void setOverrideCursor();
    void restoreOverrideCursor();

private:
    QWidget * m_w;
    bool      m_upd_enabled : 1;
    bool      m_reenabled   : 1;
    int       m_counter = 0;
    int       m_lastTick = 0;
    QProgressDialog *m_progress;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class TableView : public QTableView
{
    Q_OBJECT
public:
    explicit TableView(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *e) override;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class CheckColorTabBar : QTabBar
{
public:
    CheckColorTabBar();
    QColor color() const;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class StatusBar : public QFrame
{
    Q_OBJECT
public:
    StatusBar(Window *window);

    void updateCurrencyRates();
    void documentCurrencyChanged(const QString &ccode);
    void changeDocumentCurrency(QAction *a);
    void updateDifferenceMode();
    void updateStatistics();

protected:
    void languageChange();
    void paletteChange();
    void changeEvent(QEvent *e) override;

private:
    Window *m_window;
    Document *m_doc;
    QToolButton *m_differenceMode;
    QToolButton *m_order;
    QWidget *m_errorsSeparator;
    QToolButton *m_errors;
    QLabel *m_weight;
    QLabel *m_count;
    QLabel *m_value;
    QLabel *m_profit;
    QToolButton *m_currency;
};
