/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include <QGLWidget>

#include "vector_t.h"

#ifdef MessageBox
#undef MessageBox
#endif


namespace LDraw {

class Part;

class GLRenderer {
public:
    GLRenderer();
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

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

    virtual void makeCurrent() = 0;
    virtual void updateNeeded() = 0;

private:
    void createSurfacesList();
    void renderSurfaces(Part *part, int ldraw_basecolor);
    void renderLines(Part *part, int ldraw_basecolor);

    struct linebuffer {
        const vector_t *v;
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
};

class RenderWidget : public QGLWidget, public GLRenderer {
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = 0);
    virtual ~RenderWidget();

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;

    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);

protected:
    virtual void makeCurrent();
    virtual void updateNeeded();

private:
    QPoint m_last_pos;
};

}

#endif
