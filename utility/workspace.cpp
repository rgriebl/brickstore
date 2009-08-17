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

#include "workspace.h"

#ifdef USE_WORKSPACE

///////////////////////////////////////////////////////////////////////////////////////////////////
// TAB SPECIFIC PARTS /////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


Q_DECLARE_METATYPE(QWidget *)

namespace {

class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(Workspace *ws, bool tabpos, bool shortcut, QWidget *parent)
        : QMenu(parent), m_ws(ws), m_shortcut(shortcut)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
        connect(this, SIGNAL(triggered(QAction *)), this, SLOT(activateWindow(QAction *)));

        QActionGroup *ag = new QActionGroup(this);
        ag->setExclusive(true);

        if (tabpos) {
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

            m_tabtop->setChecked(m_ws->tabPosition() == QTabWidget::North);
            m_tabbot->setChecked(m_ws->tabPosition() == QTabWidget::South);

            if (m_ws->windowList().isEmpty())
                return;

            addSeparator();
        }

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
        if (!a)
            return;
        else if (a == m_tabtop)
            m_ws->setTabPosition(QTabWidget::North);
        else if (a == m_tabbot)
            m_ws->setTabPosition(QTabWidget::South);
        else {
            if (QWidget *w = qvariant_cast<QWidget *>(a->data()))
                m_ws->setActiveWindow(w);
        }
    }

private:
    Workspace *m_ws;
    QAction *   m_tabtop;
    QAction *   m_tabbot;
    bool        m_shortcut;
};

}

class WindowButton : public QToolButton {
    Q_OBJECT

public:
    WindowButton(Workspace *ws)
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

        if (m_ws->tabPosition() == QTabWidget::North)
            p.ry() += height();
        else if (m_ws->tabPosition() == QTabWidget::South)
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
    Workspace *m_ws;
    QMenu *m_windows;
    bool m_menushown;
};


QMenu *Workspace::windowMenu(QWidget *parent, const char *name)
{
    WindowMenu *wm = new WindowMenu(this, true, true, parent);
    if (name)
        wm->setObjectName(QLatin1String(name));
    return wm;
}


Workspace::Workspace(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    m_tabpos = QTabWidget::North;

    m_verticallayout = new QVBoxLayout(this);
    m_verticallayout->setObjectName(QLatin1String("MDI vertical layout"));
    m_verticallayout->setContentsMargins(0, 0, 0 , 0);
    m_verticallayout->setSpacing(0);

    m_tabbar = new QTabBar(this);
    m_tabbar->setElideMode(Qt::ElideMiddle);
#if defined(Q_WS_MAC)
    m_tabbar->setDrawBase(true);
#endif
#if QT_VERSION >= 0x040500
    m_tabbar->setDocumentMode(true);
    m_tabbar->setMovable(true);
    m_tabbar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabbar->setTabsClosable(true);
#endif
    m_close = new QToolButton(this);
    m_close->setIcon(QIcon(":/images/tabclose"));
    m_close->setAutoRaise(true);

    m_list = new WindowButton(this);
    m_list->setIcon(QIcon(":/images/tablist"));
    m_list->setAutoRaise(true);

    m_tablayout = new QHBoxLayout();
    m_tablayout->setObjectName(QLatin1String("MDI tab layout"));
    m_verticallayout->addLayout(m_tablayout);

    m_tablayout->addSpacing(4);
    m_tablayout->addWidget(m_tabbar,10);
    m_tablayout->addSpacing(4);
    m_tablayout->addWidget(m_list);
    m_tablayout->addWidget(m_close);

    m_stacklayout = new QStackedLayout();
    m_stacklayout->setObjectName(QLatin1String("MDI stack layout"));
    m_verticallayout->addLayout(m_stacklayout);

    m_stacklayout->setContentsMargins(0, 0, 0 , 0);
    m_stacklayout->setSpacing(0);

    relayout();

    connect(m_close, SIGNAL(clicked()), this, SLOT(closeWindow()));
    connect(m_tabbar, SIGNAL(currentChanged(int)), m_stacklayout, SLOT(setCurrentIndex(int)));
    connect(m_tabbar, SIGNAL(tabCloseRequested(int)), this, SLOT(closeWindow(int)));
    connect(m_tabbar, SIGNAL(tabMoved(int, int)), this, SLOT(tabMoved(int, int)));
    connect(m_stacklayout, SIGNAL(currentChanged(int)), this, SLOT(currentChangedHelper(int)));
    connect(m_stacklayout, SIGNAL(widgetRemoved(int)), this, SLOT(removeWindow(int)));
}

void Workspace::relayout()
{
    switch (m_tabpos) {
    case QTabWidget::South:
        m_verticallayout->removeItem(m_tablayout);
        m_tablayout->setParent(0);
        m_tabbar->setShape(QTabBar::TriangularSouth);
        m_verticallayout->addLayout(m_tablayout);
        break;

    default:
    case QTabWidget::North:
        m_verticallayout->removeItem(m_stacklayout);
        m_stacklayout->setParent(0);
        m_tabbar->setShape(QTabBar::RoundedNorth);
        m_verticallayout->addLayout(m_stacklayout);
        break;
    }

    updateVisibility();
}

void Workspace::updateVisibility()
{
    bool b = (m_stacklayout->count() > 1);
    m_list->setShown(b);
    m_close->setShown(b);
    m_tabbar->setShown(b);
}


void Workspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    int idx = m_tabbar->addTab(w->windowTitle());
    if (!w->windowIcon().isNull())
        m_tabbar->setTabIcon(idx, w->windowIcon());
    m_stacklayout->addWidget(w);

    w->installEventFilter(this);

    updateVisibility();
}

void Workspace::currentChangedHelper(int id)
{
    emit windowActivated(m_stacklayout->widget(id));
}

void Workspace::setActiveWindow(QWidget *w)
{
    m_tabbar->setCurrentIndex(m_stacklayout->indexOf(w));
}



QTabWidget::TabPosition Workspace::tabPosition () const
{
    return m_tabpos;
}

void Workspace::setTabPosition(QTabWidget::TabPosition tp)
{
    if (tp != QTabWidget::North && tp != QTabWidget::South)
        return;

     if (tp != m_tabpos) {
        m_tabpos = tp;
        relayout();
    }
}

QMdiArea::ViewMode Workspace::viewMode () const
{
    return QMdiArea::TabbedView; //TODO: harcoded ATM
}

void Workspace::setViewMode(QMdiArea::ViewMode /*mode*/)
{
    qWarning("Workspace::setViewMode is not supported ATM");
}


QList <QWidget *> Workspace::windowList() const
{
    QList <QWidget *> res;

    for (int i = 0; i < m_stacklayout->count(); i++)
        res << m_stacklayout->widget(i);

    return res;
}

QWidget *Workspace::activeWindow() const
{
    return m_stacklayout->currentWidget();
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    QWidget *w = qobject_cast <QWidget *> (o);

    if (w && (e->type() == QEvent::WindowTitleChange)) {
        m_tabbar->setTabText(m_stacklayout->indexOf(w), w->windowTitle());

//        if (w == activeWindow())
//            emit activeWindowTitleChanged(w->windowTitle());
    }

    if (w && (e->type() == QEvent::WindowIconChange)) {
        m_tabbar->setTabIcon(m_stacklayout->indexOf(w), w->windowIcon());
    }

    return QWidget::eventFilter(o, e);
}

void Workspace::removeWindow(int id)
{
    m_tabbar->removeTab(id);
    updateVisibility();

    // QTabBar doesn't emit currentChanged() when the index changes to -1
    if (m_tabbar->currentIndex() == -1)
        currentChangedHelper(-1);
}

void Workspace::closeWindow()
{
    QWidget *w = activeWindow();
    if (w)
        w->close();
}

void Workspace::closeWindow(int idx)
{
    QWidget *w = windowList()[idx];
    if (w)
        w->close();
}

void Workspace::tabMoved(int from, int to)
{
    QWidget *w = m_stacklayout->widget(from);

    if (w) {
        bool blocked = m_stacklayout->blockSignals(true);
        m_stacklayout->removeWidget(w);
        m_stacklayout->insertWidget(to, w);
        m_stacklayout->blockSignals(blocked);
    }
}

#else // USE_WORKSPACE

///////////////////////////////////////////////////////////////////////////////////////////////////
// MDI SPECIFIC PARTS /////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <QMdiSubWindow>
#include <QStyleFactory>

Q_DECLARE_METATYPE(QMdiSubWindow *)

namespace {

class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(QMdiArea *mdi, QWidget *parent)
        : QMenu(parent), m_mdi(mdi)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
        connect(this, SIGNAL(triggered(QAction *)), this, SLOT(activateWindow(QAction *)));

        QActionGroup *ag = new QActionGroup(this);
        ag->setExclusive(true);

        m_subwin = new QAction(tr("Show &Sub Windows"), ag);
        m_subwin->setCheckable(true);
        m_tabtop = new QAction(tr("Show Tabs at &Top"), ag);
        m_tabtop->setCheckable(true);
        m_tabbot = new QAction(tr("Show Tabs at &Bottom"), ag);
        m_tabbot->setCheckable(true);
        m_cascade = new QAction(tr("&Cascade"), ag);
        m_tile = new QAction(tr("T&ile"), ag);
    }

private slots:
    void buildMenu()
    {
        clear();
        addAction(m_subwin);
        addAction(m_tabtop);
        addAction(m_tabbot);


        if (m_mdi->viewMode() == QMdiArea::SubWindowView)
            m_subwin->setChecked(true);
        else if (m_mdi->tabPosition() == QTabWidget::North)
            m_tabtop->setChecked(true);
        else
            m_tabbot->setChecked(true);

        QList<QMdiSubWindow *> subw = m_mdi->subWindowList();

        if (subw.isEmpty()){
            return;
        }
        else if (m_subwin->isChecked()) {
            addSeparator();
            addAction(m_cascade);
            addAction(m_tile);
        }

        addSeparator();

        QMdiSubWindow *active = m_mdi->currentSubWindow();

        int i = 0;
        foreach (QMdiSubWindow *w, subw) {
            QString s = w->windowTitle();
            if (i < 10)
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
        if (a == m_subwin) {
            m_mdi->setViewMode(QMdiArea::SubWindowView);
        }
        if (a == m_tabtop) {
            m_mdi->setViewMode(QMdiArea::TabbedView);
            m_mdi->findChildren<QTabBar *>().first()->setStyle(qApp->style());
            m_mdi->setTabPosition(QTabWidget::North);
            m_mdi->setTabShape(QTabWidget::Rounded);
        }
        else if (a == m_tabbot) {
            m_mdi->setViewMode(QMdiArea::TabbedView);
            m_mdi->findChildren<QTabBar *>().first()->setStyle(QStyleFactory::create("windows"));
            m_mdi->setTabPosition(QTabWidget::South);
            m_mdi->setTabShape(QTabWidget::Triangular);
        }
        else if (a == m_cascade) {
            m_mdi->cascadeSubWindows();
        }
        else if (a == m_tile) {
            m_mdi->tileSubWindows();
        }
        else if (a) {
            if (QMdiSubWindow *w = qvariant_cast<QMdiSubWindow *>(a->data()))
                m_mdi->setActiveSubWindow(w);
        }
    }

private:
    QMdiArea *m_mdi;
    QAction * m_subwin;
    QAction * m_tabtop;
    QAction * m_tabbot;
    QAction * m_cascade;
    QAction * m_tile;
};

} // namespace

Workspace::Workspace(QWidget *parent, Qt::WindowFlags f)
{
    m_mdi = new QMdiArea(this);
    m_mdi->setDocumentMode(true);
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_mdi);

    connect(m_mdi, SIGNAL(subWindowActivated(QMdiSubWindow *)), this, SLOT(windowActivatedMdi(QMdiSubWindow *)));

    m_mdi->installEventFilter(this);
}

QTabWidget::TabPosition Workspace::tabPosition () const
{
    return m_mdi->tabPosition();
}

void Workspace::setTabPosition(QTabWidget::TabPosition position)
{
    m_mdi->setTabPosition(position);
    m_mdi->setTabShape(position == QTabWidget::South ? QTabWidget::Triangular : QTabWidget::Rounded);
}

QMdiArea::ViewMode Workspace::viewMode () const
{
    return m_mdi->viewMode();
}

void Workspace::setViewMode(QMdiArea::ViewMode mode)
{
    m_mdi->setViewMode(mode);
}

void Workspace::addWindow(QWidget *w)
{
    m_mdi->addSubWindow(w);
}

QWidget *Workspace::activeWindow() const
{
    return m_mdi->activeSubWindow();
}

QMenu *Workspace::windowMenu(QWidget *parent, const char *name)
{
    return new WindowMenu(m_mdi, this);
}


void Workspace::setActiveWindow(QWidget *w)
{
    foreach (QMdiSubWindow *sw, m_mdi->subWindowList()) {
        if (sw->widget() == w)
            m_mdi->setActiveSubWindow(sw);
    }
}

QList <QWidget *> Workspace::windowList() const
{
    QList<QWidget *> wl;
    foreach (QMdiSubWindow *sw, m_mdi->subWindowList())
        wl << sw->widget();
    return wl;
}

void Workspace::windowActivatedMdi(QMdiSubWindow *sw)
{
    emit windowActivated(sw ? sw->widget() : 0);
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_mdi && e->type() == QEvent::ChildPolished) {
        QChildEvent *ce = static_cast<QChildEvent *>(e);

        if (QTabBar *tb = qobject_cast<QTabBar *>(ce->child())) {
            tb->setElideMode(Qt::ElideMiddle);
            //tb->setDocumentMode(true);
            //tb->setMovable(true);
            //tb->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
            tb->setTabsClosable(true);

            connect(tb, SIGNAL(tabCloseRequested(int)), this, SLOT(closeWindow(int)));
        }
    }
    return QWidget::eventFilter(o, e);
}

void Workspace::closeWindow(int idx)
{
    m_mdi->subWindowList()[idx]->close();
}

#endif // USE_WORKSPACE

#include "workspace.moc"
