/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __TASKWIDGETS_H__
#define __TASKWIDGETS_H__

#include <QStackedWidget>
#include <QLabel>

#include "window.h"
#include "appearsinwidget.h"
#include "priceguidewidget.h"
#include "picturewidget.h"

class QLabel;
class QDockWidget;


class TaskLinksWidget : public QLabel {
    Q_OBJECT

public:
    TaskLinksWidget(QWidget *parent);

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);

    void languageChange();
    void linkActivate(const QString &url);
    void linkHover(const QString &url);

private:
    QPointer<Window> m_win;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskPriceGuideWidget : public PriceGuideWidget {
    Q_OBJECT

public:
    TaskPriceGuideWidget(QWidget *parent);

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);
    virtual void topLevelChanged(bool);
    virtual void dockLocationChanged(Qt::DockWidgetArea);

    void setPrice(double p);

protected:
    virtual bool event(QEvent *e);
    void fixParentDockWindow();

private:
    QPointer<Window> m_win;
    QDockWidget *m_dock;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskInfoWidget : public QStackedWidget {
    Q_OBJECT

public:
    TaskInfoWidget(QWidget *parent);

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);

    void languageChange();
    void refresh();

private:
    QLabel *        m_text;
    PictureWidget *m_pic;
    QPointer<Window> m_win;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class TaskAppearsInWidget : public AppearsInWidget {
    Q_OBJECT

public:
    TaskAppearsInWidget(QWidget *parent);

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;

protected slots:
    void windowUpdate(Window *win);
    void selectionUpdate(const Document::ItemList &list);

private:
    QPointer<Window> m_win;
};

#endif
