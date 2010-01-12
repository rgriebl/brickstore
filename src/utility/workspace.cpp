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
#include <cstdlib>

#include <QLayout>
#include <QTabBar>
#include <QTabWidget>
#include <QEvent>
#include <QAction>
#include <QMenu>
#include <QApplication>
#include <QDesktopWidget>
#include <QToolButton>
#include <QPixmap>

#include "workspace.h"


Q_DECLARE_METATYPE(QWidget *)

/* XPM */
static const char * const tablist_xpm[] = {
"12 12 2 1",
"  c None",
"# c #000000",
"            ",
#if !defined(Q_OS_MACX)
"  ########  ",
#else
"############",
#endif
" ########## ",
" ########## ",
" ########## ",
" ########## ",
" ########## ",
#if !defined(Q_OS_MACX)
"############",
#else
"  ########  ",
#endif
"            ",
"            ",
"            ",
"            "};


class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(Workspace *ws, bool shortcut = false, QWidget *parent = 0)
        : QMenu(parent), m_ws(ws), m_shortcut(shortcut)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
        connect(this, SIGNAL(triggered(QAction *)), this, SLOT(activateWindow(QAction *)));

        setEnabled(false);
    }

public slots:
    void checkEnabledStatus(int tabCount)
    {
        setEnabled(tabCount > 1);
    }

private slots:
    void buildMenu()
    {
        clear();
        QWidget *active = m_ws->activeWindow();

        int i = 0;
        foreach (QWidget *w, m_ws->windowList()) {
            QString s = w->windowTitle();
            if (m_shortcut && i < 10)
                s.prepend(QString("&%1   ").arg((i+1)%10));

            QAction *a = addAction(s);
            a->setCheckable(true);
            QVariant v;
            v.setValue(w);
            a->setData(v);
            a->setChecked(w == active);
            i++;
        }
    }

    void activateWindow(QAction *a)
    {
        if (a) {
            if (QWidget *w = qvariant_cast<QWidget *>(a->data()))
                m_ws->setActiveWindow(w);
        }
    }

private:
    Workspace *m_ws;
    bool       m_shortcut;
};


class TabWidget : public QTabWidget {
    Q_OBJECT
public:
    TabWidget(QWidget *tabListWidget, QWidget *parent = 0)
        : QTabWidget(parent), tabList(tabListWidget)
    {
        tabBar()->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
        updateVisibility();
    }
    
signals:
    void tabCountChanged(int);

protected:
    void tabInserted(int index)
    {
        QTabWidget::tabInserted(index);
        updateVisibility();
        emit tabCountChanged(count());
    }

    void tabRemoved(int index)
    {
        QTabWidget::tabRemoved(index);
        updateVisibility();
        emit tabCountChanged(count());
    }

private:
    void updateVisibility()
    {
        bool b = (count() > 1);
        tabBar()->setShown(b);
        setCornerWidget(b ? tabList : 0, Qt::TopRightCorner);
        tabList->setShown(b);
    }

    QWidget *tabList;
};


Workspace::Workspace(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    QToolButton *tabList = new QToolButton(this);
    tabList->setIcon(QPixmap(tablist_xpm));
    tabList->setAutoRaise(true);
    tabList->setPopupMode(QToolButton::InstantPopup);
    tabList->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tabList->setToolTip(tr("Show a list of all open documents"));

    m_tabwidget = new TabWidget(tabList, this);
    m_tabwidget->setElideMode(Qt::ElideMiddle);
    m_tabwidget->setDocumentMode(true);
    m_tabwidget->setUsesScrollButtons(true);
    m_tabwidget->setMovable(true);
    m_tabwidget->setTabsClosable(true);

    tabList->setMenu(windowMenu(false, this));

    connect(m_tabwidget, SIGNAL(currentChanged(int)), this, SLOT(currentChangedHelper(int)));
    connect(m_tabwidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeWindow(int)));
}

QMenu *Workspace::windowMenu(bool hasShortcut, QWidget *parent)
{
    WindowMenu *m = new WindowMenu(this, hasShortcut, parent);
    connect(m_tabwidget, SIGNAL(tabCountChanged(int)), m, SLOT(checkEnabledStatus(int)));
    return m;
}


void Workspace::resizeEvent(QResizeEvent *)
{
    m_tabwidget->setGeometry(0, 0, width(), height());
}


void Workspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    int idx = m_tabwidget->addTab(w, w->windowTitle());
    if (!w->windowIcon().isNull())
        m_tabwidget->setTabIcon(idx, w->windowIcon());

    w->installEventFilter(this);
}

void Workspace::currentChangedHelper(int id)
{
    emit windowActivated(m_tabwidget->widget(id));
}

void Workspace::setActiveWindow(QWidget *w)
{
    m_tabwidget->setCurrentWidget(w);
}

QList<QWidget *> Workspace::windowList() const
{
    QList<QWidget *> res;

    for (int i = 0; i < m_tabwidget->count(); i++)
        res << m_tabwidget->widget(i);

    return res;
}

QWidget *Workspace::activeWindow() const
{
    return m_tabwidget->currentWidget();
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    QWidget *w = qobject_cast<QWidget *>(o);

    if (w && (e->type() == QEvent::WindowTitleChange))
        m_tabwidget->setTabText(m_tabwidget->indexOf(w), w->windowTitle());
    if (w && (e->type() == QEvent::WindowIconChange))
        m_tabwidget->setTabIcon(m_tabwidget->indexOf(w), w->windowIcon());

    return QWidget::eventFilter(o, e);
}

void Workspace::closeWindow(int idx)
{
    m_tabwidget->widget(idx)->close();
}

#include "workspace.moc"
