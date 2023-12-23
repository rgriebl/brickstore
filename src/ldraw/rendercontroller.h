// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtGui/QQuaternion>
#include <QtQml/qqmlregistration.h>

#include <QCoro/QCoroTask>

#include "ldraw/rendergeometry.h"
#include "bricklink/color.h"
#include "bricklink/item.h"
#include "bricklink/qmlapi.h"

QT_FORWARD_DECLARE_CLASS(QQuick3DTextureData)
QT_FORWARD_DECLARE_CLASS(QTimer)


namespace LDraw {

class Part;

class RenderController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderController)
    Q_PROPERTY(QColor clearColor READ clearColor WRITE setClearColor NOTIFY clearColorChanged FINAL)
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

    void requestContextMenu(const QPointF &pos);
    void requestToolTip(const QPointF &pos);

    void qmlResetCamera(); //TODO find something nicer

    void clearColorChanged(const QColor &clearColor);

private:
    QCoro::Task<void> updateGeometries();
    static void fillVertexBuffers(Part *part, const BrickLink::Color *modelColor,
                                  const BrickLink::Color *baseColor, const QMatrix4x4 &matrix,
                                  bool inverted, QHash<const BrickLink::Color *, QByteArray> &surfaceBuffers,
                                  QByteArray &lineBuffer);
    static QQuick3DTextureData *generateMaterialTextureData(const BrickLink::Color *color);
    static std::vector<std::pair<float, float> > uvMapToNearestPlane(const QVector3D &normal,
                                                                     std::initializer_list<const QVector3D> vectors);

    QList<QmlRenderGeometry *> m_geos;
    QQuick3DGeometry *m_lineGeo = nullptr;
    QmlRenderLineInstancing *m_lines = nullptr;

    static QHash<const BrickLink::Color *, QImage> s_materialTextureDatas;

    Part *m_part = nullptr;
    const BrickLink::Item *m_item = nullptr;
    const BrickLink::Color *m_color = nullptr;

    QVector3D m_center;
    float m_radius = 0;
    bool m_tumblingAnimationActive = false;
    QColor m_clearColor;

    QTimer *m_updateTimer;
};

//TODO: QTBUG-104744
//class QmlVectorRenderGeometry
//{
//    Q_GADGET
//    QML_FOREIGN(QList<QmlRenderGeometry*>)
//    QML_ANONYMOUS
//    QML_SEQUENTIAL_CONTAINER(QmlRenderGeometry*)
//};

} // namespace LDraw
