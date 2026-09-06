// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>

#include <QtCore/QSize>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QQuaternion>

#include <QCoro/QCoroTask>

namespace BrickLink {
class Color;
class Item;
}


namespace LDraw {

constexpr bool canRenderPartImages = (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0));

struct PartImageOptions
{
    QSize size = { 512, 512 };
    int supersample = 4;                 // render at size * supersample, then area-average down
    qreal margin = 0;                    // percent of each edge to keep free
    QColor background = Qt::transparent;
    bool renderLines = false;
    std::optional<QQuaternion> rotation; // default: RenderSettings::defaultRotation()
};

QCoro::Task<QImage> renderPartImage(const BrickLink::Item *item, const BrickLink::Color *color,
                                    PartImageOptions opts);

} // namespace LDraw
