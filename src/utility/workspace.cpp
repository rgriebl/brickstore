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
    WindowMenu(Workspace *ws, bool shortcut, QWidget *parent)
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
    WindowButton(Workspace *ws)
        : QToolButton(ws), m_ws(ws), m_menushown(false)
    {
        m_windows = new WindowMenu(ws, false, ws);
        connect(this, SIGNAL(pressed()), this, SLOT(showWindowList()));
    }

private slots:
    void showWindowList()
    {
        QPoint p(0 ,0);
        p = mapToGlobal(p);
        QSize s = m_windows->sizeHint();
        QRect screen = qApp->desktop()->availableGeometry(this);

        p.ry() += height();

        if (p.ry() + s.height() > screen.height())
            p.ry() = screen.height() - s.height();
        if (p.ry() < screen.y())
            p.ry() = screen.y();
        if (p.rx() + s.width() > screen.width())
            p.rx() = screen.width() - s.width();
        if (p.rx() < screen.x())
            p.rx() = screen.x();

        m_menushown = true;
        setDown(true);
        repaint();

        m_windows->exec(p);

        m_menushown = false;
        if (isDown())
            setDown(false);
        else
            repaint();
    }

protected:
    virtual void paintEvent(QPaintEvent *)
    {
        QStylePainter p(this);
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        if (m_menushown)
            opt.state |= QStyle::State_Sunken;
        p.drawComplexControl(QStyle::CC_ToolButton, opt);
    }

private:
    Workspace *m_ws;
    QMenu *m_windows;
    bool m_menushown;
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
        setShown(count() > 1);
    }
};


QMenu *Workspace::windowMenu(QWidget *parent, const char *name)
{
    WindowMenu *wm = new WindowMenu(this, true, parent);
    if (name)
        wm->setObjectName(QLatin1String(name));
    return wm;
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
    m_list->setToolButtonStyle(Qt::ToolButtonIconOnly);
    
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
