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
#ifndef __CWORKSPACE_H__
#define __CWORKSPACE_H__

#include <QWidget>
#include <QHash>

class QStackedLayout;
class QTabBar;
class QBoxLayout;
class QMenu;
class QToolButton;


class CWorkspace : public QWidget {
    Q_OBJECT

public:
    CWorkspace(QWidget *parent = 0, Qt::WindowFlags f = 0);

    enum TabMode {
        TopTabs,
        BottomTabs
    };

    TabMode tabMode() const;
    void setTabMode(TabMode tm);

    void addWindow(QWidget *w);

    QWidget *activeWindow();
    QList <QWidget *> allWindows();

    QMenu *windowMenu(QWidget *parent = 0, const char *name = 0);

signals:
    void currentChanged(QWidget *);
    void activeWindowTitleChanged(const QString &);

public slots:
    void setCurrentWidget(QWidget *);

protected:
    virtual bool eventFilter(QObject *o, QEvent *e);

private slots:
    void removeWindow(int);
    void currentChangedHelper(int);
    void closeWindow();

private:
    void relayout();
    void updateVisibility();

private:
    TabMode               m_tabmode;
    QTabBar *             m_tabbar;
    QStackedLayout *      m_stacklayout;
    QBoxLayout *          m_verticallayout;
    QBoxLayout *          m_tablayout;
    QToolButton *         m_close;
    QToolButton *         m_list;
};

#endif

