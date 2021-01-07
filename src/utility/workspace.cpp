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
#include <cstdlib>

#include <QLayout>
#include <QStackedWidget>
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
#include <QPaintEvent>
#include <QTextDocument>
#include <QShortcut>
#include <QDebug>

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
    explicit WindowMenu(Workspace *ws, bool shortcut = false, QWidget *parent = nullptr)
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
        const auto wl = m_ws->windowList();
        for (auto *w : wl) {
            QString s = cleanWindowTitle(w);
            if (m_shortcut && (i < 10))
                s.prepend(QString("&%1   ").arg((i + 1) % 10));

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
            if (auto *w = qvariant_cast<QWidget *>(a->data()))
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
    explicit TabBarSide(QTabBar *tabbar, QWidget *parent = nullptr)
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
    explicit TabBarSideButton(QTabBar *tabbar, QWidget *parent = nullptr)
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
    return { sb.width(), st.height() };
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

    m_right = new TabBarSide(m_tabbar);
    m_windowStack = new QStackedWidget();

    m_tablist = new TabBarSideButton(m_tabbar);
    m_tablist->setIcon(QIcon(":/images/tab.png"));
    m_tablist->setAutoRaise(true);
    m_tablist->setPopupMode(QToolButton::InstantPopup);
    m_tablist->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_tablist->setMenu(windowMenu(false, this));

    auto *rightlay = new QHBoxLayout(m_right);
    rightlay->setMargin(0);
    rightlay->addWidget(m_tablist);

    auto *tabbox = new QHBoxLayout();
    tabbox->setMargin(0);
    tabbox->setSpacing(0);
    tabbox->addWidget(m_tabbar, 10);
    tabbox->addWidget(m_right);
    auto *layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addLayout(tabbox);
    layout->addWidget(m_windowStack, 10);
    setLayout(layout);

    connect(m_tabbar, &QTabBar::currentChanged,
            this, &Workspace::activateTab);
    connect(m_tabbar, &QTabBar::tabCloseRequested,
            this, &Workspace::closeTab);
    connect(m_tabbar, &QTabBar::tabMoved,
            this, &Workspace::moveTab);
    connect(m_windowStack, &QStackedWidget::widgetRemoved,
            this, &Workspace::removeTab);

    connect(this, &Workspace::windowCountChanged,
            this, [this](int count) {
        m_windowStack->setVisible(count != 0);
        m_welcomeWidget->setVisible(count == 0);
        m_tabbar->setVisible(count > 1);
        m_right->setVisible(count > 1);
    });

    for (int i = 0; i < 9; ++i) {
        //: Shortcut to activate window 0-9
        auto sc = new QShortcut(tr("Alt+%1").arg(i), this);
        connect(sc, &QShortcut::activated, this, [this, i]() {
            int j = (i == 0) ? 9 : i - 1;
            if (m_windowStack->count() > j) {
                auto w = m_windowStack->widget(j);
                if (activeWindow() != w)
                    setActiveWindow(w);
            }
        });
    }
}

QWidget *Workspace::welcomeWidget() const
{
    return m_welcomeWidget;
}

void Workspace::setWelcomeWidget(QWidget *welcomeWidget)
{
    if (welcomeWidget == m_welcomeWidget)
        return;
    if (m_welcomeWidget) {
        layout()->removeWidget(m_welcomeWidget);
        delete m_welcomeWidget;
    }
    m_welcomeWidget = welcomeWidget;
    m_welcomeWidget->setParent(this);
    m_welcomeWidget->setVisible(windowCount() == 0);
    m_windowStack->setVisible(windowCount() != 0);
    static_cast<QVBoxLayout *>(layout())->addWidget(m_welcomeWidget, 10);
}

QMenu *Workspace::windowMenu(bool hasShortcut, QWidget *parent)
{
    auto *m = new WindowMenu(this, hasShortcut, parent);
    connect(m_tabbar, &TabBar::countChanged,
            m, &WindowMenu::checkEnabledStatus);
    return m;
}

void Workspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    int idx = m_windowStack->addWidget(w);
    m_tabbar->insertTab(idx, cleanWindowTitle(w));

    w->installEventFilter(this);

    emit windowCountChanged(windowCount());
}

void Workspace::removeTab(int idx)
{
    m_tabbar->removeTab(idx);
    emit windowCountChanged(windowCount());
}

void Workspace::languageChange()
{
    m_tablist->setToolTip(tr("Show a list of all open documents"));
}

void Workspace::moveTab(int from, int to)
{
    m_windowStack->blockSignals(true);
    QWidget *w = m_windowStack->widget(from);
    m_windowStack->removeWidget(w);
    m_windowStack->insertWidget(to, w);
    m_windowStack->blockSignals(false);
}

void Workspace::activateTab(int idx)
{
    m_windowStack->setCurrentIndex(idx);
    emit windowActivated(m_windowStack->widget(idx));
}

void Workspace::setActiveWindow(QWidget *w)
{
    m_tabbar->setCurrentIndex(m_windowStack->indexOf(w));
}

QList<QWidget *> Workspace::windowList() const
{
    QList<QWidget *> res;
    int count = m_windowStack->count();
    res.reserve(count);

    for (int i = 0; i < count; i++)
        res << m_windowStack->widget(i);

    return res;
}

QWidget *Workspace::activeWindow() const
{
    return m_windowStack->currentWidget();
}

int Workspace::windowCount() const
{
    return m_windowStack->count();
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    if (QWidget *w = qobject_cast<QWidget *>(o)) {
        switch (e->type()) {
        case QEvent::WindowTitleChange:
        case QEvent::ModifiedChange:
            m_tabbar->setTabText(m_windowStack->indexOf(w), cleanWindowTitle(w));
            break;
        case QEvent::WindowIconChange:
            m_tabbar->setTabIcon(m_windowStack->indexOf(w), w->windowIcon());
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(o, e);
}

void Workspace::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

void Workspace::closeTab(int idx)
{
    m_windowStack->widget(idx)->close();
}

#include "workspace.moc"

#include "moc_workspace.cpp"
