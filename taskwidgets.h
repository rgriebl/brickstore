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

#include "document.h"
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
    void documentUpdate(Document *doc);
    void selectionUpdate(const Document::ItemList &list);

    void languageChange();
    void linkActivate(const QString &url);
    void linkHover(const QString &url);

private:
    QPointer<Document> m_doc;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskPriceGuideWidget : public PriceGuideWidget {
    Q_OBJECT

public:
    TaskPriceGuideWidget(QWidget *parent);

protected slots:
    void documentUpdate(Document *doc);
    void selectionUpdate(const Document::ItemList &list);
    virtual void dockChanged();

    void setPrice(money_t p);

protected:
    virtual bool event(QEvent *e);
    void fixParentDockWindow();

private:
    QPointer<Document> m_doc;
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
    void documentUpdate(Document *doc);
    void selectionUpdate(const Document::ItemList &list);

    void languageChange();
    void refresh();

private:
    QLabel *        m_text;
    PictureWidget *m_pic;
    QPointer<Document> m_doc;
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
    void documentUpdate(Document *doc);
    void selectionUpdate(const Document::ItemList &list);

private:
    QPointer<Document> m_doc;
};

#endif
