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

#include <qtguiglobal.h>

#if !defined(QT_NO_OPENGL)

#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QSurfaceFormat>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QScopedPointer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QColor>

#include <vector>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)
QT_FORWARD_DECLARE_CLASS(QOpenGLFramebufferObject)

#ifdef MessageBox
#undef MessageBox
#endif


namespace LDraw {

class Part;


class GLRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    GLRenderer(QObject *parent = nullptr);
    virtual ~GLRenderer() override;

    static void adjustSurfaceFormat(QSurfaceFormat &format);

    void cleanup();

    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, int basecolor);
    void setPartAndColor(Part *part, const QColor &color);

    void resetTransformation();

    void setTranslation(const QVector3D &translation);
    void setRotation(const QQuaternion &rotation);
    void setZoom(float r);

    QVector3D translation() const  { return m_translation; }
    QQuaternion rotation() const   { return m_rotation; }
    float zoom() const             { return m_zoom; }

    bool isAnimationActive() const;

    void initializeGL(QOpenGLContext *context);
    void resizeGL(QOpenGLContext *context, int w, int h);
    void paintGL(QOpenGLContext *context);

    void handleMouseEvent(QMouseEvent *e);
    void handleWheelEvent(QWheelEvent *e);

signals:
    void updateNeeded();
    void makeCurrent();
    void doneCurrent();

public slots:
    void setClearColor(const QColor &color);
    void startAnimation();
    void stopAnimation();

private slots:
    void animationStep();

private:
    void updateProjectionMatrix();
    void updateWorldMatrix();
    void recreateVBOs();

    enum VBOIndex {
        VBO_Surfaces,
        VBO_TransparentSurfaces,
        VBO_TransparentIndexes,
        VBO_Lines,
        VBO_ConditionalLines,
        VBO_Count
    };

    void fillVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix, bool inverted,
                  std::vector<float> *buffers[VBO_Count]);

    void rotateArcBall();

    QTimer *m_animation = nullptr;
    bool m_coreProfile = false;
    bool m_dirty = false;

    bool m_resortTransparentSurfaces = false;
    std::vector<QVector3D> m_transparentCenters;

    Part *m_part = nullptr;
    int m_color = -1;
    QColor m_baseColor;
    QColor m_edgeColor;
    QColor m_clearColor;

    QVector3D m_translation;
    QQuaternion m_rotation;
    float m_zoom = 1;
    QVector3D m_center;
    float m_radius = 0;
    QRect m_viewport;
    QMatrix4x4 m_proj;
    QMatrix4x4 m_view;
    QMatrix4x4 m_model;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer *m_vbos[VBO_Count];

    QOpenGLShaderProgram *m_surfaceShader = nullptr;
    QOpenGLShaderProgram *m_conditionalShader = nullptr;
    QOpenGLShaderProgram *m_lineShader = nullptr;

    bool m_arcBallActive = false;
    QPoint m_arcBallMousePos;
    QPoint m_arcBallPressPos;
    QQuaternion m_arcBallPressRotation;
};

}

#endif //!QT_NO_OPENGL
