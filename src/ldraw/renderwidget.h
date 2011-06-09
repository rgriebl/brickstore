/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __LDRAW_RENDERWIDGET_GL_H__
#define __LDRAW_RENDERWIDGET_GL_H__

#include <qglobal.h>

#if !defined(QT_NO_OPENGL)

#include <QGLWidget>
#include <QVector3D>

#ifdef MessageBox
#undef MessageBox
#endif

class QGLFramebufferObject;


namespace LDraw {

class Part;

class GLRenderer : public QObject {
    Q_OBJECT

public:
    GLRenderer(QObject *parent = 0);
    virtual ~GLRenderer();

    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, int basecolor);

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

    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

signals:
    void updateNeeded();
    void makeCurrent();

public slots:
    void startAnimation();
    void stopAnimation();

private slots:
    void animationStep();

private:
    void createSurfacesList();
    void renderSurfaces(Part *part, int ldraw_basecolor);
    void renderLines(Part *part, int ldraw_basecolor);

    struct linebuffer {
        const QVector3D *v;
        int color;
    };

    void renderLineBuffer(const QVector<linebuffer> &buffer, int ldraw_basecolor);
    void renderLineBufferSegment(const QVector<linebuffer> &buffer, int s, int e, int mode, int ldraw_basecolor);

    Part *m_part;
    int m_color;
    qreal m_rx, m_ry, m_rz;
    qreal m_tx, m_ty, m_tz;
    qreal m_zoom;
    GLuint m_surfaces_list;
    bool m_initialized;
    bool m_resized;
    QSize m_size;
    QVector3D m_center;
    QTimer *m_animation;
};

class RenderWidget : public QGLWidget {
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = 0);
    virtual ~RenderWidget();

    Part *part() const  { return m_renderer->part(); }
    int color() const   { return m_renderer->color(); }
    void setPartAndColor(Part *part, int basecolor)  { m_renderer->setPartAndColor(part, basecolor); }

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    bool isAnimationActive() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

protected slots:
    void slotMakeCurrent();

protected:
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void wheelEvent(QWheelEvent *e);

    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

private:
    GLRenderer *m_renderer;
    QPoint m_last_pos;
};


class RenderOffscreenWidget : public QWidget {
    Q_OBJECT
public:
    RenderOffscreenWidget(QWidget *parent = 0);
    virtual ~RenderOffscreenWidget();

    Part *part() const  { return m_renderer->part(); }
    int color() const   { return m_renderer->color(); }
    void setPartAndColor(Part *part, int basecolor)  { m_renderer->setPartAndColor(part, basecolor); }

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;

    void setImageSize(int w, int h);
    QImage renderImage();

    bool isAnimationActive() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

protected:
    void resizeEvent(QResizeEvent *e);
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void wheelEvent(QWheelEvent *e);

protected slots:
    void slotMakeCurrent();

private:
    QGLWidget *m_dummy;
    QGLFramebufferObject *m_fbo;
    bool m_initialized;
    bool m_resize;
    GLRenderer *m_renderer;
    QPoint m_last_pos;
};

}

#endif //!QT_NO_OPENGL

#endif
