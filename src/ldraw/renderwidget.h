// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QWidget>

#include "bricklink/color.h"
#include "bricklink/item.h"

QT_FORWARD_DECLARE_CLASS(QQuickItemGrabResult)
QT_FORWARD_DECLARE_CLASS(QQuickView)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)


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
