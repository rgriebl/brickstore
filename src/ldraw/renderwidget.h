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

#include <QOpenGLWidget>
#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QVector3D>
#include <QScopedPointer>

#include <vector>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

#ifdef MessageBox
#undef MessageBox
#endif

QT_FORWARD_DECLARE_CLASS(QOpenGLFramebufferObject)

namespace LDraw {

class Part;


class GLRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    GLRenderer(QObject *parent = nullptr);
    virtual ~GLRenderer() override;

    void cleanup();

    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, int basecolor);
    void setPartAndColor(Part *part, const QColor &color);

    void setXTranslation(float t);
    void setYTranslation(float t);
    void setZTranslation(float t);

    void setXRotation(float r);
    void setYRotation(float r);
    void setZRotation(float r);

    void setZoom(float r);

    float xTranslation() const  { return m_tx; }
    float yTranslation() const  { return m_ty; }
    float zTranslation() const  { return m_tz; }

    float xRotation() const     { return m_rx; }
    float yRotation() const     { return m_ry; }
    float zRotation() const     { return m_rz; }

    float zoom() const          { return m_zoom; }

    bool isAnimationActive() const;

    void initializeGL(QOpenGLContext *context);
    void resizeGL(QOpenGLContext *context, int w, int h);
    void paintGL(QOpenGLContext *context);

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

    void renderLines(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix, std::vector<float> &buffer);

    enum VBOIndex {
        VBO_Surfaces,
        VBO_Lines,
        VBO_ConditionalLines,
        VBO_Count
    };
    enum VBOFields {
        VBO_Offset_Vertex = 0,
        VBO_Size_Vertex   = 3,  // QVector3D
        VBO_Offset_Normal = (VBO_Offset_Vertex + VBO_Size_Vertex),
        VBO_Size_Normal   = 3,  // QVector3D
        VBO_Offset_Color  = (VBO_Offset_Normal + VBO_Size_Normal),
        VBO_Size_Color    = 4,  // RGBA

        VBO_Stride        = (VBO_Offset_Color + VBO_Size_Color)
    };
    int m_dirty = 0;

    void renderVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix, bool inverted,
                    int dirty, std::vector<float> *buffers[VBO_Count]);

    QTimer *m_animation = nullptr;
    bool m_coreProfile = false;

    Part *m_part = nullptr;
    int m_color = -1;
    QColor m_baseColor;
    QColor m_edgeColor;
    QColor m_clearColor;

    float m_rx = 0, m_ry = 0, m_rz = 0;
    float m_tx = 0, m_ty = 0, m_tz = 0;
    float m_zoom = 1;
    QVector3D m_center;
    float m_radius = 0;
    QRect m_viewport;
    QMatrix4x4 m_proj;
    QMatrix4x4 m_view;
    QMatrix4x4 m_model;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbos[VBO_Count];
    int m_vboSizes[VBO_Count];

    QOpenGLShaderProgram *m_program = nullptr;
    int m_projMatrixLoc;
    int m_modelMatrixLoc;
    int m_viewMatrixLoc;
    int m_normalMatrixLoc;
    int m_lightPosLoc;
    int m_cameraPosLoc;
};


class RenderWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = nullptr);
    ~RenderWidget() override;

    void setClearColor(const QColor &color);

    Part *part() const  { return m_renderer->part(); }
    int color() const   { return m_renderer->color(); }
    void setPartAndColor(Part *part, const QColor &color)  { m_renderer->setPartAndColor(part, color); }
    void setPartAndColor(Part *part, int basecolor)  { m_renderer->setPartAndColor(part, basecolor); }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    bool isAnimationActive() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

protected slots:
    void slotMakeCurrent();
    void slotDoneCurrent();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    GLRenderer *m_renderer;
    QPoint m_last_pos;
};


class RenderWindow : public QOpenGLWindow
{
    Q_OBJECT
public:
    RenderWindow();
    ~RenderWindow() override;

    void setClearColor(const QColor &color);

    Part *part() const  { return m_renderer->part(); }
    int color() const   { return m_renderer->color(); }
    void setPartAndColor(Part *part, const QColor &color)  { m_renderer->setPartAndColor(part, color); }
    void setPartAndColor(Part *part, int basecolor)  { m_renderer->setPartAndColor(part, basecolor); }

    bool isAnimationActive() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

protected slots:
    void slotMakeCurrent();
    void slotDoneCurrent();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    bool event(QEvent *e) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    GLRenderer *m_renderer;
    QPoint m_last_pos;
};

}

#endif //!QT_NO_OPENGL

