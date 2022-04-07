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
#include <QtQuick3D/QQuick3DGeometry>

#include "library.h"


namespace LDraw {

class Part;

class RenderGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LDrawRenderGeometry)
    Q_PROPERTY(Color color READ color CONSTANT)
    Q_PROPERTY(QVector3D center READ center CONSTANT)
    Q_PROPERTY(float radius READ radius CONSTANT)

public:
    RenderGeometry(int colorId);

    Color color() const { return m_color; }
    QVector3D center() const { return m_center; }
    void setCenter(const QVector3D &center) { m_center = center; }
    float radius() const { return m_radius; }
    void setRadius(float radius) { m_radius = radius; }

private:
    Color m_color;
    QVector3D m_center;
    float m_radius = 0;
};


class RenderController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LDrawRenderController)
    Q_PROPERTY(QVector<RenderGeometry *> surfaces READ surfaces NOTIFY surfacesChanged)
    Q_PROPERTY(QVector3D center READ center NOTIFY centerChanged)
    Q_PROPERTY(float radius READ radius NOTIFY radiusChanged)
    Q_PROPERTY(QColor color READ color NOTIFY partOrColorChanged)

    Q_PROPERTY(QQuaternion standardRotation READ standardRotation CONSTANT)
    Q_PROPERTY(bool renderLines READ renderLines WRITE setRenderLines NOTIFY renderLinesChanged)
    Q_PROPERTY(bool animationActive READ isAnimationActive WRITE setAnimationActive NOTIFY animationActiveChanged)

    Q_PROPERTY(float aoStrength READ aoStrength WRITE setAoStrength NOTIFY aoStrengthChanged)
    Q_PROPERTY(float aoDistance READ aoDistance WRITE setAoDistance NOTIFY aoDistanceChanged)
    Q_PROPERTY(float aoSoftness READ aoSoftness WRITE setAoSoftness NOTIFY aoSoftnessChanged)
    Q_PROPERTY(float lightProbeExposure READ lightProbeExposure WRITE setLightProbeExposure NOTIFY lightProbeExposureChanged)

    Q_PROPERTY(float plainMetalness READ plainMetalness WRITE setPlainMetalness NOTIFY plainMetalnessChanged)
    Q_PROPERTY(float plainRoughness READ plainRoughness WRITE setPlainRoughness NOTIFY plainRoughnessChanged)
    Q_PROPERTY(float chromeMetalness READ chromeMetalness WRITE setChromeMetalness NOTIFY chromeMetalnessChanged)
    Q_PROPERTY(float chromeRoughness READ chromeRoughness WRITE setChromeRoughness NOTIFY chromeRoughnessChanged)
    Q_PROPERTY(float metalMetalness READ metalMetalness WRITE setMetalMetalness NOTIFY metalMetalnessChanged)
    Q_PROPERTY(float metalRoughness READ metalRoughness WRITE setMetalRoughness NOTIFY metalRoughnessChanged)
    Q_PROPERTY(float pearlescentMetalness READ pearlescentMetalness WRITE setPearlescentMetalness NOTIFY pearlescentMetalnessChanged)
    Q_PROPERTY(float pearlescentRoughness READ pearlescentRoughness WRITE setPearlescentRoughness NOTIFY pearlescentRoughnessChanged)
    Q_PROPERTY(float matteMetallicMetalness READ matteMetallicMetalness WRITE setMatteMetallicMetalness NOTIFY matteMetallicMetalnessChanged)
    Q_PROPERTY(float matteMetallicRoughness READ matteMetallicRoughness WRITE setMatteMetallicRoughness NOTIFY matteMetallicRoughnessChanged)
    Q_PROPERTY(float rubberMetalness READ rubberMetalness WRITE setRubberMetalness NOTIFY rubberMetalnessChanged)
    Q_PROPERTY(float rubberRoughness READ rubberRoughness WRITE setRubberRoughness NOTIFY rubberRoughnessChanged)

    Q_PROPERTY(float tumblingAnimationAngle READ tumblingAnimationAngle WRITE setTumblingAnimationAngle NOTIFY tumblingAnimationAngleChanged)
    Q_PROPERTY(QVector3D tumblingAnimationAxis READ tumblingAnimationAxis WRITE setTumblingAnimationAxis NOTIFY tumblingAnimationAxisChanged)

    Q_PROPERTY(bool showBoundingSpheres READ showBoundingSpheres WRITE setShowBoundingSpheres NOTIFY showBoundingSpheresChanged)

public:
    RenderController(QObject *parent);
    ~RenderController() override;

    QVector<RenderGeometry *> surfaces();
    QVector3D center() const;
    float radius() const;

    QQuaternion standardRotation() const;

    Q_INVOKABLE QQuaternion rotateArcBall(QPointF pressPos, QPointF mousePos,
                                          QQuaternion pressRotation, QSizeF viewportSize);

    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, int colorId = -1);

    bool isAnimationActive() const;
    void setAnimationActive(bool active);

    float aoStrength() const;
    void setAoStrength(float newAoStrength);

    float aoDistance() const;
    void setAoDistance(float newAoDistance);

    float aoSoftness() const;
    void setAoSoftness(float newAoSoftness);

    bool renderLines() const;
    void setRenderLines(bool newRenderLines);

    float lightProbeExposure() const;
    void setLightProbeExposure(float newLightProbeExposure);

    float plainMetalness() const;
    void setPlainMetalness(float newPlainMetalness);

    float plainRoughness() const;
    void setPlainRoughness(float newPlainRoughness);

    float chromeMetalness() const;
    void setChromeMetalness(float newChromeMetalness);

    float chromeRoughness() const;
    void setChromeRoughness(float newChromeRoughness);

    float metalMetalness() const;
    void setMetalMetalness(float newMetalMetalness);

    float metalRoughness() const;
    void setMetalRoughness(float newMetalRoughness);

    float pearlescentMetalness() const;
    void setPearlescentMetalness(float newPearlescentMetalness);

    float pearlescentRoughness() const;
    void setPearlescentRoughness(float newPearlescentRoughness);

    float matteMetallicMetalness() const;
    void setMatteMetallicMetalness(float newMatteMetallicMetalness);

    float matteMetallicRoughness() const;
    void setMatteMetallicRoughness(float newMatteMetallicRoughness);

    float rubberMetalness() const;
    void setRubberMetalness(float newRubberMetalness);

    float rubberRoughness() const;
    void setRubberRoughness(float newRubberRoughness);

    float tumblingAnimationAngle() const;
    void setTumblingAnimationAngle(float newTumblingAnimationAngle);

    QVector3D tumblingAnimationAxis() const;
    void setTumblingAnimationAxis(const QVector3D &newTumblingAnimationAxis);

    bool showBoundingSpheres() const;
    void setShowBoundingSpheres(bool newShowBoundingSpheres);

public slots:
    void resetCamera();

signals:
    void surfacesChanged();
    void partOrColorChanged();

    void centerChanged();
    void radiusChanged();

    void aoStrengthChanged();
    void aoDistanceChanged();
    void aoSoftnessChanged();
    void renderLinesChanged();
    void animationActiveChanged();

    void requestContextMenu(const QPointF &pos);

    void qmlResetCamera(); //TODO find something nicer

    void lightProbeExposureChanged();
    void plainMetalnessChanged();
    void plainRoughnessChanged();
    void chromeMetalnessChanged();
    void chromeRoughnessChanged();
    void metalMetalnessChanged();
    void metalRoughnessChanged();
    void pearlescentMetalnessChanged();
    void pearlescentRoughnessChanged();
    void matteMetallicMetalnessChanged();
    void matteMetallicRoughnessChanged();
    void rubberMetalnessChanged();
    void rubberRoughnessChanged();
    void tumblingAnimationAngleChanged();
    void tumblingAnimationAxisChanged();

    void showBoundingSpheresChanged();

private:
    void updateGeometries();
    void fillVertexBuffers(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix, bool inverted,
                           QHash<int, QByteArray> &surfaceBuffers);

    QVector<RenderGeometry *> m_geos;

    Part *m_part = nullptr;
    int m_colorId = -1;

    QVector3D m_center;
    float m_radius = 0;
    bool m_renderLines = false;
    bool m_animationActive = false;

    float m_aoStrength = 1;
    float m_aoDistance = 0.1;
    float m_aoSoftness = 0.2;
    float m_lightProbeExposure = 0.9;
    float m_plainMetalness = 0;
    float m_plainRoughness = 0.5;
    float m_chromeMetalness = 1;
    float m_chromeRoughness = 0.15;
    float m_metalMetalness = 1;
    float m_metalRoughness = 0.45;
    float m_pearlescentMetalness = 0.5;
    float m_pearlescentRoughness = 0.25;
    float m_matteMetallicMetalness = 0.8;
    float m_matteMetallicRoughness = 0.5;
    float m_rubberMetalness = 0;
    float m_rubberRoughness = 1;
    float m_tumblingAnimationAngle = 0.1;
    QVector3D m_tumblingAnimationAxis = { 0.5, 0.375, 0.25 };
    bool m_showBoundingSpheres = false;
};

} // namespace LDraw

Q_DECLARE_METATYPE(LDraw::RenderGeometry *)
