/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

class QOpenGLFramebufferObject;

namespace LDraw {

class Part;


class GLRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    GLRenderer(QObject *parent = nullptr);
    virtual ~GLRenderer();

    void cleanup();

    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, int basecolor);
    void setPartAndColor(Part *part, const QColor &color);

    void setXTranslation(qreal t);
    void setYTranslation(qreal t);
    void setZTranslation(qreal t);

    void setXRotation(qreal r);
    void setYRotation(qreal r);
    void setZRotation(qreal r);

    void setZoom(qreal r);

    qreal xTranslation() const  { return m_tx; }
    qreal yTranslation() const  { return m_ty; }
    qreal zTranslation() const  { return m_tz; }

    qreal xRotation() const     { return m_rx; }
    qreal yRotation() const     { return m_ry; }
    qreal zRotation() const     { return m_rz; }

    qreal zoom() const          { return m_zoom; }

    bool isAnimationActive() const;

    void initializeGL(QOpenGLContext *context);
    void resizeGL(QOpenGLContext *context, int w, int h);
    void paintGL(QOpenGLContext *context);

signals:
    void updateNeeded();
    void makeCurrent();
    void doneCurrent();

public slots:
    void startAnimation();
    void stopAnimation();

private slots:
    void animationStep();

private:
    void updateWorldMatrix();
    void recreateVBOs();

    void createSurfacesVBO();

    void createLinesVBO();
    void renderLines(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix, std::vector<float> &buffer);

    enum VBOIndex { Surfaces, Lines, ConditionalLines, Count };
    int m_dirty = 0;

    void renderVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix, int dirty, std::vector<float> *buffers[Count]);

    QTimer *m_animation = nullptr;


    Part *m_part = nullptr;
    int m_color = -1;
    QColor m_baseColor;
    QColor m_edgeColor;

    qreal m_rx = 0, m_ry = 0, m_rz = 0;
    qreal m_tx = 0, m_ty = 0, m_tz = 0;
    qreal m_zoom = 1;
    QVector3D m_center;
    QRect m_viewport;
    QMatrix4x4 m_proj;
    QMatrix4x4 m_camera;
    QMatrix4x4 m_world;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbos[Count];
    int m_vboSizes[Count];

    QOpenGLShaderProgram *m_program = nullptr;
    int m_projMatrixLoc;
    int m_mvMatrixLoc;
    int m_normalMatrixLoc;
    int m_lightPosLoc;
};


class RenderWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = nullptr);
    ~RenderWidget() override;

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


class RenderOffscreenWidget : public QWidget
{
    Q_OBJECT
public:
    RenderOffscreenWidget(QWidget *parent = nullptr);
    ~RenderOffscreenWidget() override;

    Part *part() const  { return m_renderer->part(); }
    int color() const   { return m_renderer->color(); }
    void setPartAndColor(Part *part, const QColor &color)  { m_renderer->setPartAndColor(part, color); }
    void setPartAndColor(Part *part, int basecolor)  { m_renderer->setPartAndColor(part, basecolor); }

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

    void setImageSize(int w, int h);
    QImage renderImage();

    bool isAnimationActive() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

protected slots:
    void slotMakeCurrent();
    void slotDoneCurrent();

private:
    QScopedPointer<QOpenGLWindow> m_dummy;
    QScopedPointer<QOpenGLContext> m_context;
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;
    bool m_initialized = false;
    bool m_resize = false;
    GLRenderer *m_renderer;
    QPoint m_last_pos;
};

}

#endif //!QT_NO_OPENGL

