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
#pragma once

#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QStackedLayout)
QT_FORWARD_DECLARE_CLASS(QTabWidget)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QSizeGrip)


class Workspace : public QWidget
{
    Q_OBJECT
public:
    Workspace(QWidget *parent = nullptr);

    QWidget *welcomeWidget() const;
    void setWelcomeWidget(QWidget *welcomeWidget);

    void addWindow(QWidget *w);

    QWidget *activeWindow() const;
    int windowCount() const;
    QVector<QWidget *> windowList() const;

    QMenu *windowMenu(bool hasShortcuts = false, QWidget *parent = nullptr);

signals:
    void windowActivated(QWidget *);
    void windowCountChanged(int count);
    void welcomeWidgetVisible();

public slots:
    void setActiveWindow(QWidget *);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    void languageChange();

private:
    QToolButton *m_tabhome;
    QToolButton *m_tabback;
    QToolButton *m_tablist;
    QSizeGrip *m_sizeGrip;
    QStackedLayout *m_stack;
    QTabWidget *m_tabs;
    QWidget *m_welcomeWidget;
    bool m_welcomeActive = false;

};
