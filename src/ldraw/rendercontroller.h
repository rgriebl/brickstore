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

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtGui/QQuaternion>

#include "ldraw/rendergeometry.h"
#include "bricklink/color.h"
#include "bricklink/item.h"

QT_FORWARD_DECLARE_CLASS(QQuick3DTextureData)


namespace LDraw {

class Part;

class RenderController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LDrawRenderController)
    Q_PROPERTY(QColor clearColor READ clearColor WRITE setClearColor NOTIFY clearColorChanged)
    Q_PROPERTY(QVector<QmlRenderGeometry *> surfaces READ surfaces NOTIFY surfacesChanged)
    Q_PROPERTY(QQuick3DGeometry * lineGeometry READ lineGeometry CONSTANT)
    Q_PROPERTY(QQuick3DInstancing * lines READ lines CONSTANT)
    Q_PROPERTY(QVector3D center READ center NOTIFY centerChanged)
    Q_PROPERTY(float radius READ radius NOTIFY radiusChanged)
    Q_PROPERTY(bool tumblingAnimationActive READ isTumblingAnimationActive WRITE setTumblingAnimationActive NOTIFY tumblingAnimationActiveChanged)

public:
    RenderController(QObject *parent = nullptr);
    ~RenderController() override;

    QVector<QmlRenderGeometry *> surfaces();
    QQuick3DGeometry *lineGeometry();
    QQuick3DInstancing *lines();
    QQuick3DInstancing *conditionalLines();
    QVector3D center() const;
    float radius() const;

    Q_INVOKABLE QQuaternion rotateArcBall(QPointF pressPos, QPointF mousePos,
                                          QQuaternion pressRotation, QSizeF viewportSize);

    Part *part() const;
    const BrickLink::Color *color() const;
    Q_INVOKABLE void setPartAndColor(BrickLink::QmlItem item, BrickLink::QmlColor color);
    void setPartAndColor(Part *part, const BrickLink::Color *color);
    void setPartAndColor(Part *part, int ldrawColorId);

    bool isTumblingAnimationActive() const;
    void setTumblingAnimationActive(bool active);

    const QColor &clearColor() const;
    void setClearColor(const QColor &newClearColor);

public slots:
    void resetCamera();

signals:
    void surfacesChanged();
    void linesChanged();
    void partOrColorChanged();

    void centerChanged();
    void radiusChanged();

    void tumblingAnimationActiveChanged();

    void requestContextMenu(const QPointF &pos);
    void requestToolTip(const QPointF &pos);

    void qmlResetCamera(); //TODO find something nicer

    void clearColorChanged(const QColor &clearColor);

private:
    void updateGeometries();
    void fillVertexBuffers(Part *part, const BrickLink::Color *baseColor, const QMatrix4x4 &matrix,
                           bool inverted, QHash<const BrickLink::Color *, QByteArray> &surfaceBuffers,
                           QByteArray &lineBuffer);
    QQuick3DTextureData *generateMaterialTextureData(const BrickLink::Color *color) const;

    QVector<QmlRenderGeometry *> m_geos;
    QQuick3DGeometry *m_lineGeo = nullptr;
    QmlRenderLineInstancing *m_lines = nullptr;

    static QHash<const BrickLink::Color *, QQuick3DTextureData *> s_materialTextureDatas;

    Part *m_part = nullptr;
    const BrickLink::Color *m_color = nullptr;

    QVector3D m_center;
    float m_radius = 0;
    bool m_tumblingAnimationActive = false;
    QColor m_clearColor;
};

} // namespace LDraw
