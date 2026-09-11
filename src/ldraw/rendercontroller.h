// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <utility>
#include <vector>

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QQuaternion>
#include <QtQml/qqmlregistration.h>

#include <QCoro/QCoroTask>

#include "ldraw/part.h"
#include "ldraw/rendergeometry.h"
#include "bricklink/color.h"
#include "bricklink/item.h"
#include "bricklink/qmlapi.h"

QT_FORWARD_DECLARE_CLASS(QTimer)


namespace LDraw {

class RenderController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderController)
    Q_PROPERTY(QColor clearColor READ clearColor WRITE setClearColor NOTIFY clearColorChanged FINAL)
    Q_PROPERTY(QColor edgeColor READ edgeColor NOTIFY edgeColorChanged FINAL)
    Q_PROPERTY(QList<LDraw::QmlRenderGeometry *> surfaces READ surfaces NOTIFY surfacesChanged FINAL)
    Q_PROPERTY(QQuick3DGeometry * lineGeometry READ lineGeometry CONSTANT FINAL)
    Q_PROPERTY(QQuick3DInstancing * lines READ lines CONSTANT FINAL)
    Q_PROPERTY(QVector3D center READ center NOTIFY centerChanged FINAL)
    Q_PROPERTY(float radius READ radius NOTIFY radiusChanged FINAL)
    Q_PROPERTY(bool tumblingAnimationActive READ isTumblingAnimationActive WRITE setTumblingAnimationActive NOTIFY tumblingAnimationActiveChanged FINAL)
    Q_PROPERTY(bool canRender READ canRender NOTIFY canRenderChanged FINAL)

public:
    RenderController(QObject *parent = nullptr);
    ~RenderController() override;

    QList<LDraw::QmlRenderGeometry *> surfaces();
    QQuick3DGeometry *lineGeometry();
    QQuick3DInstancing *lines();
    QQuick3DInstancing *conditionalLines();
    QVector3D center() const;
    float radius() const;

    Q_INVOKABLE QQuaternion rotateArcBall(QPointF pressPos, QPointF mousePos,
                                          QQuaternion pressRotation, QSizeF viewportSize);

    const BrickLink::Item *item() const;
    const BrickLink::Color *color() const;
    Q_INVOKABLE void setItemAndColor(BrickLink::QmlItem item, BrickLink::QmlColor color);
    void setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color);
    bool canRender() const;

    bool isTumblingAnimationActive() const;
    void setTumblingAnimationActive(bool active);

    const QColor &clearColor() const;
    void setClearColor(const QColor &newClearColor);

    // the complement color for LDraw color 24 lines, applied via the line shader
    QColor edgeColor() const;

public slots:
    void resetCamera();

signals:
    void surfacesChanged();
    void linesChanged();
    void itemOrColorChanged();
    void canRenderChanged(bool b);

    void centerChanged();
    void radiusChanged();

    void tumblingAnimationActiveChanged();

    void requestToolTip(const QPointF &pos);

    void qmlResetCamera(); //TODO find something nicer

    void clearColorChanged(const QColor &clearColor);
    void edgeColorChanged(const QColor &edgeColor);

private:
    // surface bucket key: authored color (nullptr = LDraw color 16) plus "render two-sided"
    // (geometry from BFC uncertified or NOCLIP scopes must not be back-face culled)
    using SurfaceKey = std::pair<const BrickLink::Color *, bool>;

    // calculated on a worker thread, so plain data only. The data is color-independent:
    // LDraw color 16 surfaces (color == nullptr) and color 24 lines are resolved against the
    // current model color at material level, so a color change needs no rebuild.
    struct RenderData {
        struct Surface {
            const BrickLink::Color *color = nullptr; // nullptr: inherits the model color
            bool twoSided = false;
            QByteArray vertexData;
            QVector3D boundsMin;
            QVector3D boundsMax;
            QVector3D center;
            float radius = 0;
        };

        QByteArray lineBuffer;
        std::vector<Surface> surfaces;
        QVector3D center;
        float radius = 0;
    };

    // color is only used to pre-warm the texture image cache off the GUI thread
    static RenderData calculateRenderData(const PartRef &part, const BrickLink::Color *color,
                                          bool smooth);
    void regenerate();
    void applyRenderData(RenderData &&data);
    void updateSurfaceColors();

    // baseColor == nullptr means "inherits the model color" (LDraw color 16); cullingEnabled
    // is the accumulated BFC state of the reference chain (false once any ancestor was
    // uncertified or stopped clipping)
    static void fillVertexBuffers(Part *part, const BrickLink::Color *baseColor,
                                  const QMatrix4x4 &matrix, bool inverted, bool cullingEnabled,
                                  QHash<SurfaceKey, QByteArray> &surfaceBuffers,
                                  QByteArray &lineBuffer);
    static QImage generateMaterialTextureImage(const BrickLink::Color *color);
    static QQuick3DTextureData *createTextureData(const BrickLink::Color *color, QmlRenderGeometry *geo);
    static std::vector<std::pair<float, float> > uvMapToNearestPlane(const QVector3D &normal,
                                                                     std::initializer_list<const QVector3D> vectors);

    QList<QmlRenderGeometry *> m_geos;
    QQuick3DGeometry *m_lineGeo = nullptr;
    QmlRenderLineInstancing *m_lines = nullptr;

    PartRef m_part;
    const BrickLink::Item *m_item = nullptr;
    const BrickLink::Color *m_color = nullptr;

    QVector3D m_center;
    float m_radius = 0;
    bool m_tumblingAnimationActive = false;
    QColor m_clearColor;

    Q_DISABLE_COPY_MOVE(RenderController)
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
class QmlVectorRenderGeometry
{
    Q_GADGET
    QML_FOREIGN(QList<LDraw::QmlRenderGeometry *>)
    QML_ANONYMOUS
    QML_SEQUENTIAL_CONTAINER(QmlRenderGeometry *)
};
#endif

} // namespace LDraw
