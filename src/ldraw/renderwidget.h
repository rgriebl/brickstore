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

#include <QWidget>
#include <QQuickView>

#include "bricklink/color.h"

QT_FORWARD_DECLARE_CLASS(QQuickItemGrabResult)


namespace LDraw {

class Part;
class RenderController;

class RenderWidget : public QWidget
{
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = nullptr);

    RenderController *controller();

    void setPartAndColor(Part *part, int ldrawColorId);
    void setPartAndColor(Part *part, const BrickLink::Color *color);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    bool isAnimationActive() const;
    void setAnimationActive(bool active);

    bool startGrab();

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

signals:
    void animationActiveChanged();
    void grabFinished(QImage grabbedImage);

private:
    RenderController *m_controller;
    QQuickView *m_window;
    QWidget *m_widget;
    QSharedPointer<QQuickItemGrabResult> m_grabResult;
};

}
