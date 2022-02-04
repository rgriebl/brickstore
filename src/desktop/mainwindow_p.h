/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include <QProgressDialog>
#include <QMenu>
#include <QLabel>
#include <QDockWidget>
#include <QStyle>
#include <QApplication>
#include <QMouseEvent>
#include <QToolButton>
#include <QTabBar>
#include <QGridLayout>
#include <QShortcut>
#include <QComboBox>
#include <QStackedLayout>
#include <QDebug>

#include "common/config.h"
#include "common/document.h"
#include "common/documentlist.h"
#include "common/recentfiles.h"
#include "utility/utility.h"
#include "mainwindow.h"
#include "view.h"


class RecentMenu : public QMenu
{
    Q_OBJECT
public:
    explicit RecentMenu(QWidget *parent)
        : QMenu(parent)
    {
        connect(this, &QMenu::aboutToShow, this, [this]() {
            clear();

            auto rf = RecentFiles::inst();

            int cnt = rf->count();
            for (int i = 0; i < cnt; ++i) {
                QString fn = rf->data(rf->index(i, 0), RecentFiles::FileNameRole).toString();
                QString dn = rf->data(rf->index(i, 0), RecentFiles::DirNameRole).toString();

                QString s = fn % u" (" % dn % u")";
#if !defined(Q_OS_MACOS)
                if (i < 9)
                    s.prepend(QString("&%1   "_l1).arg(i + 1));
#endif
                addAction(s)->setData(i);
            }
            if (!cnt) {
                addAction(tr("No recent files"))->setEnabled(false);
            } else {
                addSeparator();
                addAction(tr("Clear recent files"))->setData(-1);
            }
        });
        connect(this, &QMenu::triggered, this, [](QAction *a) {
            auto index = a->data().toInt();
            if (index >= 0)
                RecentFiles::inst()->open(index);
            else
                RecentFiles::inst()->clear();
        });
    }

signals:
    void openRecent(const QString &file);
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class WindowMenuAdapter : public QObject
{
    Q_OBJECT
public:
    explicit WindowMenuAdapter(MainWindow *fw, QMenu *menu, bool shortcut = false)
        : QObject(menu)
        , m_fw(fw)
        , m_menu(menu)
        , m_shortcut(shortcut)
    {
        connect(menu, &QMenu::aboutToShow, this, [this]() {
            m_menu->clear();

            int cnt = 0;
            for (int i = 0; i < DocumentList::inst()->count(); ++i) {
                QModelIndex idx = DocumentList::inst()->index(i);
                QString s = idx.data().toString();
                if (m_shortcut && (cnt < 10))
                    s = s % u'&' % QString::number((cnt + 1) % 10) % u"   ";

                QAction *a = m_menu->addAction(s);
                a->setCheckable(true);
                Document *doc = idx.data(Qt::UserRole).value<Document *>();
                a->setData(QVariant::fromValue(doc));
                a->setChecked(m_fw->activeView() && (m_fw->activeView()->document() == doc));
                ++cnt;
            }
            if (!cnt)
                m_menu->addAction(tr("No windows"))->setEnabled(false);
        });

        connect(menu, &QMenu::triggered, this, [](QAction *a) {
            if (a) {
                if (auto *doc = qvariant_cast<Document *>(a->data()))
                    emit doc->requestActivation();
            }
        });
    }

private:
    MainWindow *m_fw;
    QMenu *m_menu;
    bool m_shortcut;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class LoadColumnLayoutMenuAdapter : public QObject
{
    Q_OBJECT

public:
    explicit LoadColumnLayoutMenuAdapter(QMenu *menu)
        : QObject(menu)
        , m_menu(menu)
    {
        connect(menu, &QMenu::aboutToShow, this, [this]() {
            m_menu->clear();

            for (auto cl : Document::columnLayoutCommands()) {
                m_menu->addAction(Document::columnLayoutCommandName(cl))
                        ->setData(Document::columnLayoutCommandId(cl));
            }

            auto ids = Config::inst()->columnLayoutIds();
            if (!ids.isEmpty()) {
                m_menu->addSeparator();

                QMap<int, QString> pos;
                for (const auto &id : ids)
                    pos.insert(Config::inst()->columnLayoutOrder(id), id);

                for (const auto &id : qAsConst(pos))
                    m_menu->addAction(Config::inst()->columnLayoutName(id))->setData(id);
            }
        });
        connect(menu, &QMenu::triggered, this, [this](QAction *a) {
            if (a && !a->data().isNull())
                emit load(a->data().toString());
        });
    }

signals:
    void load(const QString &id);

private:
    QMenu *m_menu;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class FancyDockTitleBar : public QFrame
{
public:
    explicit FancyDockTitleBar(QDockWidget *parent)
        : QFrame(parent)
        , m_gradient(0, 0, 1, 0)
        , m_dock(parent)
    {
        setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Plain));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // hide the bottom frame line
        setContentsMargins(contentsMargins() + QMargins(0, 0, 0, -frameWidth()));
        setAutoFillBackground(true);
        m_gradient.setCoordinateMode(QGradient::ObjectMode);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        m_text = new QLabel();
        if (m_dock) {
            m_text->setText(m_dock->windowTitle());
            connect(m_dock, &QDockWidget::windowTitleChanged,
                    m_text, &QLabel::setText);
        }
        m_text->setIndent(style()->pixelMetric(QStyle::PM_LayoutLeftMargin));
        layout->addWidget(m_text, 1, Qt::AlignLeft | Qt::AlignVCenter);
        m_button = new QToolButton();
        m_button->setIcon(QIcon::fromTheme("overflow-menu"_l1));
        m_button->setAutoRaise(true);
        m_button->setPopupMode(QToolButton::InstantPopup);
        m_button->setProperty("noMenuArrow", true);
        layout->addWidget(m_button, 0, Qt::AlignRight | Qt::AlignVCenter);

        fontChange();
        paletteChange();

        QMetaObject::invokeMethod(this, [this]() {
            m_menu.reset(MainWindow::inst()->createPopupMenu());
            m_button->setMenu(m_menu.get());
        }, Qt::QueuedConnection);
    }

protected:
    void changeEvent(QEvent *e) override
    {
        if (e->type() == QEvent::PaletteChange)
            paletteChange();
        else if (e->type() == QEvent::FontChange)
            fontChange();
        QFrame::changeEvent(e);
    }

    void fontChange()
    {
        QFont f = font();
        f.setBold(true);
        m_text->setFont(f);
    }

    void paletteChange()
    {
        QPalette p = palette();
        QPalette ivp = QApplication::palette("QAbstractItemView");
        QColor high = ivp.color(p.currentColorGroup(), QPalette::Highlight);
        QColor text = ivp.color(p.currentColorGroup(), QPalette::HighlightedText);
        QColor win = QApplication::palette(this).color(QPalette::Window);

        m_gradient.setStops({ { 0, high },
                              { .6, Utility::gradientColor(high, win, 0.5) },
                              { 1, win } });
        p.setBrush(QPalette::Window, m_gradient);
        p.setColor(QPalette::WindowText, text);
        setPalette(p);
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev)
            ev->ignore();
    }
    void mouseMoveEvent(QMouseEvent *ev) override
    {
        if (ev)
            ev->ignore();
    }
    void mouseReleaseEvent(QMouseEvent *ev) override
    {
        if (ev)
            ev->ignore();
    }

private:
    QLinearGradient m_gradient;
    QDockWidget *m_dock;
    QLabel *m_text;
    QToolButton *m_button;
    std::unique_ptr<QMenu> m_menu;
};
