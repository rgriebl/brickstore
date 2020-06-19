/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QTabWidget>
#include <QEvent>
#include <QAction>
#include <QMenu>
#include <QApplication>
#include <QDesktopWidget>
#include <QToolButton>
#include <QPixmap>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include "utility.h"
#include "workspace.h"

Q_DECLARE_METATYPE(QWidget *)


static QString cleanWindowTitle(QWidget *window)
{
    QString s = window->windowTitle();
    s.replace(QLatin1String("[*]"), QLatin1String(window->isWindowModified() ? "*" : ""));
    return s;
}


class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(Workspace *ws, bool shortcut = false, QWidget *parent = 0)
        : QMenu(parent), m_ws(ws), m_shortcut(shortcut)
    {
        connect(this, &QMenu::aboutToShow,
                this, &WindowMenu::buildMenu);
        connect(this, &QMenu::triggered,
                this, &WindowMenu::activateWindow);

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
            QString s = cleanWindowTitle(w);
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


class TabBarSide : public QWidget
{
    Q_OBJECT
public:
    TabBarSide(QTabBar *tabbar, QWidget *parent = nullptr)
        : QWidget(parent), m_tabbar(tabbar)
    { }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QTabBar *m_tabbar;
};

void TabBarSide::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionTabBarBase option;
    option.initFrom(this);
    option.tabBarRect = option.rect;
    if (m_tabbar) {
        option.shape = m_tabbar->shape();
        option.documentMode = m_tabbar->documentMode();

        QStyleOptionTab tabOverlap;
        tabOverlap.shape = m_tabbar->shape();
        int overlap = m_tabbar->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &tabOverlap, m_tabbar);
        option.rect.setTop(option.rect.bottom() - overlap + 1);
        option.rect.setHeight(overlap);
    }
    style()->drawPrimitive(QStyle::PE_FrameTabBarBase, &option, &p, this);
}


class TabBar : public QTabBar
{
    Q_OBJECT
public:
    TabBar(QWidget *parent = nullptr)
        : QTabBar(parent)
    {
        setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    }

signals:
    void countChanged(int);

protected:
    // QTabBar's count property doesn't define a NOTIFY signals, so we have generate one ourselves
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
};

void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    emit countChanged(count());
}

void TabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    emit countChanged(count());
}


class TabBarSideButton : public QToolButton
{
    Q_OBJECT

public:
    TabBarSideButton(QTabBar *tabbar, QWidget *parent = nullptr)
        : QToolButton(parent), m_tabbar(tabbar)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    }

    QSize sizeHint() const override;

private:
    QTabBar *m_tabbar;
};

QSize TabBarSideButton::sizeHint() const
{
    QSize sb = QToolButton::sizeHint();
    QSize st = m_tabbar->sizeHint();
    return QSize(sb.width(), st.height());
}


Workspace::Workspace(QWidget *parent)
    : QWidget(parent)
{
    m_tabbar = new TabBar(this);
    m_tabbar->setElideMode(Qt::ElideMiddle);
    m_tabbar->setDocumentMode(true);
    m_tabbar->setUsesScrollButtons(true);
    m_tabbar->setMovable(true);
    m_tabbar->setTabsClosable(true);
    m_tabbar->setAutoHide(true);

    m_right = new TabBarSide(m_tabbar);
    m_stack = new QStackedLayout();

    TabBarSideButton *tabList = new TabBarSideButton(m_tabbar);
    tabList->setIcon(QIcon(":/images/tab.png")); //QPixmap(tablist_xpm)));
    tabList->setAutoRaise(true);
    tabList->setPopupMode(QToolButton::InstantPopup);
    tabList->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tabList->setToolTip(tr("Show a list of all open documents"));
    tabList->setMenu(windowMenu(false, this));

    QHBoxLayout *rightlay = new QHBoxLayout(m_right);
    rightlay->setMargin(0);
    rightlay->addWidget(tabList);

    QHBoxLayout *tabbox = new QHBoxLayout();
    tabbox->setMargin(0);
    tabbox->setSpacing(0);
    tabbox->addWidget(m_tabbar, 10);
    tabbox->addWidget(m_right);
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addLayout(tabbox);
    layout->addLayout(m_stack, 10);
    setLayout(layout);

    connect(m_tabbar, &QTabBar::currentChanged,
            this, &Workspace::activateTab);
    connect(m_tabbar, &QTabBar::tabCloseRequested,
            this, &Workspace::closeTab);
    connect(m_tabbar, &QTabBar::tabMoved,
            this, &Workspace::moveTab);
    connect(m_stack, &QStackedLayout::widgetRemoved,
            this, &Workspace::removeTab);
}

QMenu *Workspace::windowMenu(bool hasShortcut, QWidget *parent)
{
    WindowMenu *m = new WindowMenu(this, hasShortcut, parent);
    connect(m_tabbar, &TabBar::countChanged,
            m, &WindowMenu::checkEnabledStatus);
    return m;
}

void Workspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    int idx = m_stack->addWidget(w);
    m_tabbar->insertTab(idx, cleanWindowTitle(w));

    w->installEventFilter(this);
}

void Workspace::removeTab(int idx)
{
    m_tabbar->removeTab(idx);
}

void Workspace::moveTab(int from, int to)
{
    m_stack->blockSignals(true);
    QWidget *w = m_stack->widget(from);
    m_stack->removeWidget(w);
    m_stack->insertWidget(to, w);
    m_stack->blockSignals(false);
}

void Workspace::activateTab(int idx)
{
    m_stack->setCurrentIndex(idx);
    emit windowActivated(m_stack->widget(idx));
}

void Workspace::setActiveWindow(QWidget *w)
{
    m_tabbar->setCurrentIndex(m_stack->indexOf(w));
}

QList<QWidget *> Workspace::windowList() const
{
    QList<QWidget *> res;

    for (int i = 0; i < m_stack->count(); i++)
        res << m_stack->widget(i);

    return res;
}

QWidget *Workspace::activeWindow() const
{
    return m_stack->currentWidget();
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    if (QWidget *w = qobject_cast<QWidget *>(o)) {
        switch (e->type()) {
        case QEvent::WindowTitleChange:
        case QEvent::ModifiedChange:
            m_tabbar->setTabText(m_stack->indexOf(w), cleanWindowTitle(w));
            break;
        case QEvent::WindowIconChange:
            m_tabbar->setTabIcon(m_stack->indexOf(w), w->windowIcon());
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(o, e);
}

void Workspace::closeTab(int idx)
{
    m_stack->widget(idx)->close();
}

#include "workspace.moc"
