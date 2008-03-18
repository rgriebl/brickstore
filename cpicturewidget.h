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
#ifndef __CPICTUREWIDGET_H__
#define __CPICTUREWIDGET_H__

#include <QFrame>
#include <QLabel>

#include "bricklinkfwd.h"


class CPictureWidgetPrivate;
class QAction;


class CPictureWidget : public QFrame {
    Q_OBJECT
public:
    CPictureWidget(QWidget *parent = 0, Qt::WindowFlags f = 0);
    virtual ~CPictureWidget();

    void setPicture(BrickLink::Picture *pic);
    BrickLink::Picture *picture() const;

    virtual QSize sizeHint() const;

protected slots:
    void doUpdate();
    void gotUpdate(BrickLink::Picture *);
    void redraw();
    void viewLargeImage();
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();
    void checkContextMenu(bool b);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void paintEvent(QPaintEvent *e);
    virtual void resizeEvent(QResizeEvent *e);

private:
    CPictureWidgetPrivate *d;
};


class CLargePictureWidgetPrivate;

class CLargePictureWidget : public QLabel {
    Q_OBJECT

public:
    CLargePictureWidget(BrickLink::Picture *lpic, QWidget *parent);
    virtual ~CLargePictureWidget();

protected slots:
    void doUpdate();
    void gotUpdate(BrickLink::Picture *);
    void redraw();
    void languageChange();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);

private:
    CLargePictureWidgetPrivate *d;
};

#endif
