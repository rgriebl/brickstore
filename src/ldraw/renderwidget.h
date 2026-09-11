// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>

#include <QWidget>
#include <QtGui/QQuaternion>

#include "bricklink/color.h"
#include "bricklink/item.h"

QT_FORWARD_DECLARE_CLASS(QQuickView)
QT_FORWARD_DECLARE_CLASS(QQuickWidget)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)


namespace LDraw {

class Part;
class RenderController;

class RenderWidget : public QWidget
{
    Q_OBJECT
public:
    RenderWidget(QQmlEngine *engine, QWidget *parent = nullptr);

    RenderController *controller();

    static bool isGPUSupported();

    void clear();
    void setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color);

    bool canRender() const;

    bool isAnimationActive() const;
    void setAnimationActive(bool active);

    std::optional<QQuaternion> modelRotation() const;
    bool renderLines() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

signals:
    void animationActiveChanged();
    void canRenderChanged(bool b);

protected:
    void changeEvent(QEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void paletteChange();
    void languageChange();

    RenderController *m_controller = nullptr;
    std::unique_ptr<QQuickWidget> m_widget;
};

}
