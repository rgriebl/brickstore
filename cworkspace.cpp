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
#include <stdlib.h>

#include <QLayout>
#include <QStackedLayout>
#include <QTabBar>
#include <QEvent>
#include <QAction>
#include <QMenu>
#include <QApplication>
#include <QDesktopWidget>
#include <QToolButton>
#include <QStylePainter>
#include <QStyleOptionToolButton>

#include "cworkspace.h"

Q_DECLARE_METATYPE(QWidget *)

class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(CWorkspace *ws, bool tabmode, bool shortcut, QWidget *parent)
        : QMenu(parent), m_ws(ws), m_shortcut(shortcut)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
        connect(this, SIGNAL(triggered(QAction *)), this, SLOT(activateWindow(QAction *)));

        QActionGroup *ag = new QActionGroup(this);
        ag->setExclusive(true);

        if (tabmode) {
            m_tabtop = new QAction(tr("Show Tabs at Top"), ag);
            m_tabtop->setCheckable(true);
            m_tabbot = new QAction(tr("Show Tabs at Bottom"), ag);
            m_tabbot->setCheckable(true);
        }
        else {
            m_tabtop = m_tabbot = 0;
        }
    }

private slots:
    void buildMenu()
    {
        clear();
        if (m_tabtop && m_tabbot) {
            addAction(m_tabtop);
            addAction(m_tabbot);

            m_tabtop->setChecked(m_ws->tabMode() == CWorkspace::TopTabs);
            m_tabbot->setChecked(m_ws->tabMode() == CWorkspace::BottomTabs);

            if (m_ws->allWindows().isEmpty())
                return;

            addSeparator();
        }

        QWidget *active = m_ws->activeWindow();

        int i = 0;
        foreach (QWidget *w, m_ws->allWindows()) {
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
        if (!a)
            return;
        else if (a == m_tabtop)
            m_ws->setTabMode(CWorkspace::TopTabs);
        else if (a == m_tabbot)
            m_ws->setTabMode(CWorkspace::BottomTabs);
        else {
            if (QWidget *w = qvariant_cast<QWidget *>(a->data()))
                m_ws->setCurrentWidget(w);
        }
    }

private:
    CWorkspace *m_ws;
    QAction *   m_tabtop;
    QAction *   m_tabbot;
    bool        m_shortcut;
};

class WindowButton : public QToolButton {
    Q_OBJECT

public:
    WindowButton(CWorkspace *ws)
        : QToolButton(ws), m_ws(ws), m_menushown(false)
    {
        m_windows = new WindowMenu(ws, false, false, ws);
        connect(this, SIGNAL(pressed()), this, SLOT(showWindowList()));
    }

private slots:
    void showWindowList()
    {
        QPoint p(0 ,0);
        p = mapToGlobal(p);
        QSize s = m_windows->sizeHint();
        QRect screen = qApp->desktop()->availableGeometry(this);

        if (m_ws->tabMode() == CWorkspace::TopTabs)
            p.ry() += height();
        else if (m_ws->tabMode() == CWorkspace::BottomTabs)
            p.ry() -= s.height();

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
    CWorkspace *m_ws;
    QMenu *m_windows;
    bool m_menushown;
};


QMenu *CWorkspace::windowMenu(QWidget *parent, const char *name)
{
    WindowMenu *wm = new WindowMenu(this, true, true, parent);
    if (name)
        wm->setObjectName(QLatin1String(name));
    return wm;
}


CWorkspace::CWorkspace(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    m_tabmode = TopTabs;

    m_verticallayout = new QVBoxLayout(this);
    m_verticallayout->setContentsMargins(0, 0, 0 , 0);
    m_verticallayout->setSpacing(0);

    m_tabbar = new QTabBar(this);
    m_tabbar->setElideMode(Qt::ElideMiddle);
    m_tabbar->setDrawBase(false);

    m_close = new QToolButton(this);
    m_close->setIcon(QIcon(":/images/tabclose"));
    m_close->setAutoRaise(true);

    m_list = new WindowButton(this);
    m_list->setIcon(QIcon(":/images/tablist"));
    m_list->setAutoRaise(true);

    m_tablayout = new QHBoxLayout();
    m_tablayout->addSpacing(4);
    m_tablayout->addWidget(m_tabbar,10);
    m_tablayout->addSpacing(4);
    m_tablayout->addWidget(m_list);
    m_tablayout->addWidget(m_close);
    m_verticallayout->addLayout(m_tablayout);

    m_stacklayout = new QStackedLayout();
    m_stacklayout->setContentsMargins(0, 0, 0 , 0);
    m_stacklayout->setSpacing(0);
    m_verticallayout->addLayout(m_stacklayout);

    relayout();

    connect(m_close, SIGNAL(clicked()), this, SLOT(closeWindow()));
    connect(m_tabbar, SIGNAL(currentChanged(int)), m_stacklayout, SLOT(setCurrentIndex(int)));
    connect(m_stacklayout, SIGNAL(currentChanged(int)), this, SLOT(currentChangedHelper(int)));
    connect(m_stacklayout, SIGNAL(widgetRemoved(int)), this, SLOT(removeWindow(int)));
}


void CWorkspace::relayout()
{
    switch (m_tabmode) {
    case BottomTabs:
        m_verticallayout->removeItem(m_tablayout);
        m_tabbar->setShape(QTabBar::TriangularSouth);
        m_verticallayout->addLayout(m_tablayout);
        break;

    default:
    case TopTabs:
        m_verticallayout->removeItem(m_stacklayout);
        m_tabbar->setShape(QTabBar::RoundedNorth);
        m_verticallayout->addLayout(m_stacklayout);
        break;
    }

    updateVisibility();
}

void CWorkspace::updateVisibility()
{
    bool b = (m_stacklayout->count() > 1);
    m_list->setShown(b);
    m_close->setShown(b);
    m_tabbar->setShown(b);
}


void CWorkspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    m_tabbar->addTab(w->windowTitle());
    m_stacklayout->addWidget(w);

    w->installEventFilter(this);

    updateVisibility();
}

void CWorkspace::currentChangedHelper(int id)
{
    emit currentChanged(m_stacklayout->widget(id));
}

void CWorkspace::setCurrentWidget(QWidget *w)
{
    m_tabbar->setCurrentIndex(m_stacklayout->indexOf(w));
}

void CWorkspace::removeWindow(int id)
{
    m_tabbar->removeTab(id);
    updateVisibility();

    // QTabBar doesn't emit currentChanged() when the index changes to -1
    if (m_tabbar->currentIndex() == -1)
        currentChangedHelper(-1);
}

CWorkspace::TabMode CWorkspace::tabMode() const
{
    return m_tabmode;
}

void CWorkspace::setTabMode(TabMode tm)
{
    if (tm != m_tabmode) {
        m_tabmode = tm;
        relayout();
    }
}

bool CWorkspace::eventFilter(QObject *o, QEvent *e)
{
    QWidget *w = qobject_cast <QWidget *> (o);

    if (w && (e->type() == QEvent::WindowTitleChange)) {
        m_tabbar->setTabText(m_stacklayout->indexOf(w), w->windowTitle());

        if (w == activeWindow())
            emit activeWindowTitleChanged(w->windowTitle());
    }

    return QWidget::eventFilter(o, e);
}

QList <QWidget *> CWorkspace::allWindows()
{
    QList <QWidget *> res;

    for (int i = 0; i < m_stacklayout->count(); i++)
        res << m_stacklayout->widget(i);

    return res;
}

QWidget *CWorkspace::activeWindow()
{
    return m_stacklayout->currentWidget();
}

void CWorkspace::closeWindow()
{
    QWidget *w = activeWindow();
    if (w)
        w->close();
}

#include "cworkspace.moc"
