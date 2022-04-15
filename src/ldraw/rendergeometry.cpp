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
#include <QtQuick3D/QQuick3DTextureData>

#include "ldraw/rendergeometry.h"
#include "bricklink/core.h"


namespace LDraw {

QmlRenderGeometry::QmlRenderGeometry(const BrickLink::Color *color)
    : QQuick3DGeometry()
    , m_color(color)
{ }

QmlRenderLineInstancing::QmlRenderLineInstancing()
{
    markDirty();
}

QByteArray QmlRenderLineInstancing::getInstanceBuffer(int *instanceCount)
{
    *instanceCount = m_buffer.size() / sizeof(InstanceTableEntry);
    return m_buffer;
}

void QmlRenderLineInstancing::clear()
{
    m_buffer.clear();
    markDirty();
}

void QmlRenderLineInstancing::setBuffer(const QByteArray &ba)
{
    m_buffer = ba;
    markDirty();
}


//static QVector4D sRGBToLinear(const QColor &c)
//{
//    const QVector3D rgb(c.redF(), c.greenF(), c.blueF());
//    const float C1 = 0.305306011f;
//    const QVector3D C2(0.682171111f, 0.682171111f, 0.682171111f);
//    const QVector3D C3(0.012522878f, 0.012522878f, 0.012522878f);
//    return QVector4D(rgb * (rgb * (rgb * C1 + C2) + C3), c.alphaF());
//}


void QmlRenderLineInstancing::addLineToBuffer(QByteArray &buffer, const QColor &c,
                                              const QVector3D &p0, const QVector3D &p1)
{
    QQuick3DInstancing::InstanceTableEntry entry { { p0, 0 },
                                                   { p1, 0 },
                                                   { },
                                                   QVector4D { c.redF(), c.greenF(), c.blueF(), c.alphaF() },
                                                   { } };
    buffer.append(reinterpret_cast<const char *>(&entry), sizeof(entry));
}

void QmlRenderLineInstancing::addConditionalLineToBuffer(QByteArray &buffer, const QColor &c,
                                                         const QVector3D &p0, const QVector3D &p1,
                                                         const QVector3D &p2, const QVector3D &p3)
{
    QQuick3DInstancing::InstanceTableEntry entry { { p0, 0 },
                                                   { p1, 0 },
                                                   { p2, 0 },
                                                   QVector4D { c.redF(), c.greenF(), c.blueF(), c.alphaF() },
                                                   { p3, 1 /* is conditional */ } };
    buffer.append(reinterpret_cast<const char *>(&entry), sizeof(entry));
}

} // namespace LDraw

#include "moc_rendergeometry.cpp"
