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

#include "bricklink/color.h"
#include "bricklink/item.h"

QT_FORWARD_DECLARE_CLASS(QQuickItemGrabResult)
QT_FORWARD_DECLARE_CLASS(QQuickView)


namespace LDraw {

class Part;
class RenderController;

class RenderWidget : public QWidget
{
    Q_OBJECT
public:
    RenderWidget(QQmlEngine *engine, QWidget *parent = nullptr);
    ~RenderWidget() override;

    RenderController *controller();

    static bool isGPUSupported();

    void clear();
    void setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color);

    bool canRender() const;

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
    void canRenderChanged(bool b);

protected:
    void changeEvent(QEvent *e) override;

private:
    void paletteChange();
    void languageChange();

    RenderController *m_controller = nullptr;
    QQuickView *m_window = nullptr;
    QWidget *m_widget = nullptr;
    QSharedPointer<QQuickItemGrabResult> m_grabResult;
};

}
