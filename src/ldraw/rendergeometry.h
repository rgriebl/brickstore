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

#include <QtGui/QColor>
#include <QtGui/QVector3D>
#include <QQmlEngine>
#include <QtQuick3D/QQuick3DGeometry>
#include <QtQuick3D/QQuick3DInstancing>

#include "bricklink/color.h"

QT_FORWARD_DECLARE_CLASS(QQuick3DTextureData)


namespace LDraw {

class Part;

class QmlRenderGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderGeometry)
    QML_UNCREATABLE("")
    Q_PROPERTY(QColor color READ color CONSTANT FINAL)
    Q_PROPERTY(float luminance READ luminance CONSTANT FINAL)
    Q_PROPERTY(bool isChrome READ isChrome CONSTANT FINAL)
    Q_PROPERTY(bool isMetallic READ isMetallic CONSTANT FINAL)
    Q_PROPERTY(bool isPearl READ isPearl CONSTANT FINAL)
    Q_PROPERTY(QQuick3DTextureData *textureData READ textureData CONSTANT FINAL)
    Q_PROPERTY(QVector3D center READ center CONSTANT FINAL)
    Q_PROPERTY(float radius READ radius CONSTANT FINAL)

public:
    QmlRenderGeometry(const BrickLink::Color *color);

    QColor color() const        { return m_color->ldrawColor(); }
    float luminance() const     { return m_color->luminance(); }
    bool isChrome() const       { return m_color->isChrome(); }
    bool isMetallic() const     { return m_color->isMetallic(); }
    bool isPearl() const        { return m_color->isPearl(); }
    QQuick3DTextureData *textureData() const     { return m_texture; }
    void setTextureData(QQuick3DTextureData *td) { m_texture = td; }
    QVector3D center() const                     { return m_center; }
    void setCenter(const QVector3D &center)      { m_center = center; }
    float radius() const                         { return m_radius; }
    void setRadius(float radius)                 { m_radius = radius; }

private:
    const BrickLink::Color *m_color;
    QQuick3DTextureData *m_texture = nullptr;
    QVector3D m_center;
    float m_radius = 0;
};

class QmlRenderLineInstancing : public QQuick3DInstancing
{
    Q_OBJECT

public:
    QmlRenderLineInstancing();
    QByteArray getInstanceBuffer(int *instanceCount) override;

    void clear();
    void setBuffer(const QByteArray &ba);

    static void addLineToBuffer(QByteArray &buffer, const QColor &c, const QVector3D &p0,
                                const QVector3D &p1);
    static void addConditionalLineToBuffer(QByteArray &buffer, const QColor &c, const QVector3D &p0,
                                           const QVector3D &p1, const QVector3D &p2, const QVector3D &p3);

private:
    QByteArray m_buffer;
};

} // namespace LDraw

Q_DECLARE_METATYPE(LDraw::QmlRenderGeometry *)
