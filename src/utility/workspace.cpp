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
#include <QStackedLayout>
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


Workspace::Workspace(QWidget *parent)
    : QWidget(parent)
{
    m_tabs = new QTabWidget();
    m_tabs->setElideMode(Qt::ElideMiddle);
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setMovable(true);
    m_tabs->setTabsClosable(true);
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);

    m_tablist = new QToolButton();
    m_tablist->setIcon(QIcon::fromTheme("tab-duplicate"));
    m_tablist->setAutoRaise(true);
    m_tablist->setPopupMode(QToolButton::InstantPopup);
    m_tablist->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_tablist->setMenu(windowMenu(false, this));
    m_tabs->setCornerWidget(m_tablist, Qt::TopRightCorner);

    m_tabhome = new QToolButton();
    m_tabhome->setIcon(QIcon::fromTheme("go-home"));
    m_tabhome->setAutoRaise(true);
    m_tabhome->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_tabs->setCornerWidget(m_tabhome, Qt::TopLeftCorner);

    m_welcomeWidget = new QWidget();

    m_tabback = new QToolButton(m_welcomeWidget);
    m_tabback->setIcon(QIcon::fromTheme("go-previous"));
    m_tabback->setAutoRaise(true);
    m_tabback->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(m_tabback, &QToolButton::clicked,
            this, [this]() { setActiveWindow(m_tabs->currentWidget()); });

    m_stack = new QStackedLayout(this);
    m_stack->addWidget(m_tabs);
    m_stack->addWidget(m_welcomeWidget);

    connect(m_tabhome, &QToolButton::clicked,
            this, [this]() { m_stack->setCurrentWidget(m_welcomeWidget); });

    connect(m_stack, &QStackedLayout::currentChanged,
            this, [this](int idx) {
        m_welcomeActive = (m_stack->widget(idx) == m_welcomeWidget);
        emit windowActivated(m_welcomeActive ? nullptr : m_tabs->currentWidget());
        if (m_welcomeActive) {
            m_tabback->setGeometry(m_tabhome->geometry());
            m_tabback->setVisible(m_tabs->count() != 0);
            emit welcomeWidgetVisible();
        }
    });

    connect(m_tabs, &QTabWidget::currentChanged,
            this, [this](int idx) {
        emit windowActivated(m_tabs->widget(idx));
    });
    connect(m_tabs, &QTabWidget::tabCloseRequested,
            this, [this](int idx) {
        m_tabs->widget(idx)->close();
    });

    connect(m_tabs->findChild<QStackedWidget *>("qt_tabwidget_stackedwidget"), &QStackedWidget::widgetRemoved,
            this, [this]() {
        int c = windowCount();
        emit windowCountChanged(c);
        if (c == 0)
            m_stack->setCurrentWidget(m_welcomeWidget);
    });

    for (int i = 0; i < 9; ++i) {
        //: Shortcut to activate window 0-9
        auto sc = new QShortcut(tr("Alt+%1").arg(i), this);
        connect(sc, &QShortcut::activated, this, [this, i]() {
            const int j = (i == 0) ? 9 : i - 1;
            if (j < windowCount() && !m_welcomeActive) {
                auto w = m_tabs->widget(j);
                if (activeWindow() != w)
                    setActiveWindow(w);
            }
        });
    }
    languageChange();
    m_stack->setCurrentWidget(m_welcomeWidget);
}

QWidget *Workspace::welcomeWidget() const
{
    return m_welcomeWidget;
}

void Workspace::setWelcomeWidget(QWidget *welcomeWidget)
{
    if (welcomeWidget == m_welcomeWidget)
        return;
    if (!welcomeWidget)
        welcomeWidget = new QWidget();
    if (m_welcomeWidget)
        m_welcomeWidget->removeEventFilter(this);
    m_stack->replaceWidget(m_welcomeWidget, welcomeWidget, Qt::FindDirectChildrenOnly);
    m_tabback->setParent(welcomeWidget);
    delete m_welcomeWidget;
    m_welcomeWidget = welcomeWidget;
    if (m_welcomeWidget)
        m_welcomeWidget->installEventFilter(this);
}

QMenu *Workspace::windowMenu(bool hasShortcut, QWidget *parent)
{
    auto *m = new WindowMenu(this, hasShortcut, parent);
    connect(this, &Workspace::windowCountChanged,
            m, &WindowMenu::checkEnabledStatus);
    return m;
}

void Workspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    int idx = m_tabs->addTab(w, cleanWindowTitle(w));
    m_tabs->setTabToolTip(idx, m_tabs->tabText(idx));

    w->installEventFilter(this);

    emit windowCountChanged(windowCount());
}

void Workspace::languageChange()
{
    m_tablist->setToolTip(tr("Show a list of all open documents"));
    m_tabhome->setToolTip(tr("Go to the Quickstart page"));
    m_tabback->setToolTip(tr("Go back to the current document"));
}

void Workspace::setActiveWindow(QWidget *w)
{
    m_stack->setCurrentWidget(m_tabs);
    m_tabs->setCurrentWidget(w);
}

QVector<QWidget *> Workspace::windowList() const
{
    QVector<QWidget *> res;
    int count = m_tabs->count();
    res.reserve(count);

    for (int i = 0; i < count; ++i)
        res << m_tabs->widget(i);

    return res;
}

QWidget *Workspace::activeWindow() const
{
    return (m_stack->currentWidget() == m_tabs) ? m_tabs->currentWidget() : nullptr;
}

int Workspace::windowCount() const
{
    return m_tabs->count();
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    // handle back keys and mouse buttons
    if ((o == m_welcomeWidget) && m_welcomeActive && (m_tabs->count())) {
        bool goBack = false;

        switch (e->type()) {
        case QEvent::KeyPress: {
            auto ke = static_cast<QKeyEvent *>(e);
            goBack = (ke->key() == Qt::Key_Back) || (ke->key() == Qt::Key_Escape);
            break;
        }
        case QEvent::MouseButtonPress: {
            auto me = static_cast<QMouseEvent *>(e);
            goBack = (me->button() == Qt::BackButton);
            break;
        }
        default:
            break;
        }
        if (goBack) {
            setActiveWindow(m_tabs->currentWidget());
            e->accept();
            return true;
        }
    } else if (QWidget *w = qobject_cast<QWidget *>(o)) {
        switch (e->type()) {
        case QEvent::WindowTitleChange:
        case QEvent::ModifiedChange: {
            int idx = m_tabs->indexOf(w);
            m_tabs->setTabText(idx, cleanWindowTitle(w));
            m_tabs->setTabToolTip(idx, m_tabs->tabText(idx));
            break;
        }
        case QEvent::WindowIconChange:
            m_tabs->setTabIcon(m_tabs->indexOf(w), w->windowIcon());
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

#include "workspace.moc"

#include "moc_workspace.cpp"
