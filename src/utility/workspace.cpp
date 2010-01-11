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
#include <QStylePainter>
#include <QStyleOptionToolButton>

#include "workspace.h"


Q_DECLARE_METATYPE(QWidget *)

namespace {

class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(Workspace *ws, bool shortcut = false, QWidget *parent = 0)
        : QMenu(parent), m_ws(ws), m_shortcut(shortcut)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
        connect(this, SIGNAL(triggered(QAction *)), this, SLOT(activateWindow(QAction *)));

        QActionGroup *ag = new QActionGroup(this);
        ag->setExclusive(true);
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

}

class WindowButton : public QToolButton {
    Q_OBJECT

public:
    WindowButton(QWidget *parent = 0)
        : QToolButton(parent)
    { }

protected:
    virtual void paintEvent(QPaintEvent *)
    {
        // do not draw the 'v' indicator as it doesn't make sense here
        QStylePainter p(this);
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.features &= ~QStyleOptionToolButton::HasMenu;
        p.drawComplexControl(QStyle::CC_ToolButton, opt);
    }
};

class TabWidget : public QTabWidget {
    Q_OBJECT
public:
    TabWidget(QWidget *parent = 0)
        : QTabWidget(parent)
    {
        tabBar()->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
        updateVisibility();
    }
    
protected:
    void tabInserted(int index)
    {
        QTabWidget::tabInserted(index);
        updateVisibility();
    }

    void tabRemoved(int index)
    {
        QTabWidget::tabRemoved(index);
        updateVisibility();
    }

private: 
    void updateVisibility()
    {
        bool b = (count() > 1);
        tabBar()->setShown(b);
        if (QWidget *w = cornerWidget(Qt::TopRightCorner))
            w->setShown(b);
        if (QWidget *w = cornerWidget(Qt::TopLeftCorner))
            w->setShown(b);
    }
};


QMenu *Workspace::windowMenu()
{
    return new WindowMenu(this, true);
}


Workspace::Workspace(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    m_tabwidget = new TabWidget(this);
    m_tabwidget->setElideMode(Qt::ElideMiddle);
    m_tabwidget->setDocumentMode(true);
    m_tabwidget->setUsesScrollButtons(true);
    m_tabwidget->setMovable(true);
    m_tabwidget->setTabsClosable(true);
   
    m_list = new WindowButton(this);
    m_list->setIcon(QIcon(":/images/tablist"));
    m_list->setAutoRaise(true);
    m_list->setMenu(new WindowMenu(this));
    m_list->setPopupMode(QToolButton::InstantPopup);
    m_list->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_list->setToolTip(tr("Show a list of all open documents"));
    
    m_tabwidget->setCornerWidget(m_list, Qt::TopRightCorner);

    connect(m_tabwidget, SIGNAL(currentChanged(int)), this, SLOT(currentChangedHelper(int)));
    connect(m_tabwidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeWindow(int)));
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
