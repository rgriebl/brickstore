/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

#define USE_WORKSPACE

#include <QWidget>
#include <QHash>
#include <QMdiArea>

class QStackedLayout;
class QTabBar;
class QBoxLayout;
class QMenu;
class QToolButton;


class Workspace : public QWidget {
    Q_OBJECT

public:
    Workspace(QWidget *parent = 0, Qt::WindowFlags f = 0);

    QTabWidget::TabPosition tabPosition () const;
    void setTabPosition(QTabWidget::TabPosition position);

    QMdiArea::ViewMode viewMode () const;
    void setViewMode(QMdiArea::ViewMode mode);

    void addWindow(QWidget *w);

    QWidget *activeWindow() const;
    QList <QWidget *> windowList() const;

    QMenu *windowMenu(QWidget *parent = 0, const char *name = 0);

signals:
    void windowActivated(QWidget *);
//    void activeWindowTitleChanged(const QString &);

public slots:
    void setActiveWindow(QWidget *);

protected:
    virtual bool eventFilter(QObject *o, QEvent *e);

private slots:
    void closeWindow(int idx);

#ifdef USE_WORKSPACE
private slots:
    void removeWindow(int);
    void currentChangedHelper(int);
    void closeWindow();
    void tabMoved(int from, int to);

private:
    void relayout();
    void updateVisibility();

private:
    QTabWidget::TabPosition  m_tabpos;
    QTabBar *             m_tabbar;
    QStackedLayout *      m_stacklayout;
    QBoxLayout *          m_verticallayout;
    QBoxLayout *          m_tablayout;
    QToolButton *         m_close;
    QToolButton *         m_list;
#else
private slots:
    void windowActivatedMdi(QMdiSubWindow *);


private:
    QMdiArea *            m_mdi;
#endif
};



#endif

