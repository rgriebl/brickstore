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
#pragma once

#include <QFrame>
#include <QLabel>

#include "bricklinkfwd.h"

class PictureWidgetPrivate;
class QAction;


class PictureWidget : public QFrame
{
    Q_OBJECT
public:
    PictureWidget(QWidget *parent = nullptr);
    virtual ~PictureWidget();

    void setPicture(BrickLink::Picture *pic);
    BrickLink::Picture *picture() const;

    QSize sizeHint() const override;

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
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    PictureWidgetPrivate *d;
};


class LargePictureWidgetPrivate;

class LargePictureWidget : public QLabel
{
    Q_OBJECT
public:
    LargePictureWidget(BrickLink::Picture *lpic, QWidget *parent);
    virtual ~LargePictureWidget();

protected slots:
    void doUpdate();
    void gotUpdate(BrickLink::Picture *);
    void redraw();
    void languageChange();

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    LargePictureWidgetPrivate *d;
};
