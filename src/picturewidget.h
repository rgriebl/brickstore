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
#include <QImage>

#include "bricklinkfwd.h"

namespace LDraw {
class Part;
class RenderWidget;
}
QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QLabel)


class PictureWidget : public QFrame
{
    Q_OBJECT
public:
    PictureWidget(QWidget *parent = nullptr);
    ~PictureWidget() override;

    void setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color = nullptr);

protected slots:
    void doUpdate();
    void pictureWasUpdated(BrickLink::Picture *);
    void redraw();
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;
    bool event(QEvent *e) override;

private:
    const BrickLink::Item * m_item = nullptr;
    const BrickLink::Color *m_color = nullptr;
    BrickLink::Picture *m_pic = nullptr;
    QLabel *w_text;
    QLabel *w_image;
#if !defined(QT_NO_OPENGL)
    LDraw::RenderWidget *w_ldraw = nullptr;
    LDraw::Part *m_part = nullptr;
    int m_colorId = 0;
#endif
    QImage m_image;
};
