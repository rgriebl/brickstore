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

#include <QStackedWidget>
#include <QLabel>

#include "window.h"
#include "document.h"
#include "appearsinwidget.h"
#include "priceguidewidget.h"
#include "picturewidget.h"

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QDockWidget)


class TaskPriceGuideWidget : public PriceGuideWidget
{
    Q_OBJECT

public:
    TaskPriceGuideWidget(QWidget *parent);

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);
    void currencyUpdate(const QString &ccode);
    virtual void topLevelChanged(bool);
    virtual void dockLocationChanged(Qt::DockWidgetArea);

    void setPrice(double p);

protected:
    bool event(QEvent *e) override;
    void fixParentDockWindow();

private:
    QPointer<Window> m_win;
    QDockWidget *m_dock;
    QTimer m_delayTimer;
    Document::ItemList m_selection;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskInfoWidget : public QStackedWidget
{
    Q_OBJECT

public:
    TaskInfoWidget(QWidget *parent);

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);
    void currencyUpdate();

    void languageChange();
    void refresh();

protected:
    void changeEvent(QEvent *e) override;

private:
    void delayedSelectionUpdate(const Document::ItemList &list);

    QLabel *m_text;
    PictureWidget *m_pic;
    QPointer<Window> m_win;
    QTimer m_delayTimer;
    Document::ItemList m_selection;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class TaskAppearsInWidget : public AppearsInWidget
{
    Q_OBJECT

public:
    TaskAppearsInWidget(QWidget *parent);

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);

private:
    QPointer<Window> m_win;
    QTimer m_delayTimer;
    Document::ItemList m_selection;
};
