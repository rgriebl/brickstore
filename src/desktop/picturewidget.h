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

#include <QFrame>
#include <QImage>
#include <QIcon>

#include "bricklink/global.h"

namespace LDraw {
class Part;
class RenderWidget;
}
QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QStackedWidget)


class PictureWidget : public QFrame
{
    Q_OBJECT

public:
    PictureWidget(QWidget *parent = nullptr);
    ~PictureWidget() override;

    void setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color = nullptr);

    bool prefer3D() const;
    void setPrefer3D(bool b);

protected slots:
    void showImage();
    void stackSwitch();
    void languageChange();
    void paletteChange();

protected:
    void changeEvent(QEvent *e) override;
    bool event(QEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private:
    void updateButtons();

    const BrickLink::Item * m_item = nullptr;
    const BrickLink::Color *m_color = nullptr;
    BrickLink::Picture *m_pic = nullptr;
    QLabel *w_text = nullptr;
    QLabel *w_image = nullptr;
    QImage m_image;
    bool m_prefer3D = false;
    LDraw::RenderWidget *w_ldraw = nullptr;
    QToolButton *w_2d = nullptr;
    QToolButton *w_3d = nullptr;
    QToolButton *w_reloadRescale = nullptr;
    QIcon m_rescaleIcon;
    QIcon m_reloadIcon;
    QMenu *m_contextMenu = nullptr;
    QStackedWidget *w_stack;
    QSize m_currentImageSize;

    QAction *m_renderSettings;
    QAction *m_copyImage;
    QAction *m_saveImageAs;
    QAction *m_blCatalog;
    QAction *m_blPriceGuide;
    QAction *m_blLotsForSale;
};
