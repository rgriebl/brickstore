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

#include <QFrame>
#include <QLabel>
#include <QScopedPointer>

#include "bricklinkfwd.h"

class PictureWidgetPrivate;
class QAction;


class PictureWidget : public QFrame
{
    Q_OBJECT
public:
    PictureWidget(QWidget *parent = nullptr);
    ~PictureWidget() override;

    void setPicture(BrickLink::Picture *pic);
    BrickLink::Picture *picture() const;

    QSize sizeHint() const override;

protected slots:
    void doUpdate();
    void gotUpdate(BrickLink::Picture *);
    void redraw();
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;
    bool event(QEvent *e) override;



private:
    QScopedPointer<PictureWidgetPrivate> d;
};
