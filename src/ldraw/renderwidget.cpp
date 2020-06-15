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
#include <qglobal.h>

#if !defined(QT_NO_OPENGL)

#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_2_1>
#include <QPainter>
#include <QTimer>
#include <QMatrix4x4>

#include <cfloat>
#include <cmath>

#include "ldraw.h"
#include "renderwidget.h"


static inline void qMultMatrix(QOpenGLFunctions_2_1 *gl, const QMatrix4x4 &mat)
{
    if (sizeof(qreal) == sizeof(GLfloat))
        gl->glMultMatrixf((GLfloat*) mat.constData());
#ifndef QT_OPENGL_ES
    else if (sizeof(qreal) == sizeof(GLdouble))
        gl->glMultMatrixd((GLdouble*) mat.constData());
#endif
    else
    {
        GLfloat fmat[16];
        float const *r = mat.constData();
        for (int i = 0; i < 16; ++i)
            fmat[i] = r[i];
        gl->glMultMatrixf(fmat);
    }
}

static inline QMatrix4x4 qMatrixFromGL(QOpenGLFunctions_2_1 *gl, int param)
{
    QMatrix4x4 mat;
    if (sizeof(qreal) == sizeof(GLfloat)) {
        gl->glGetFloatv(param, (GLfloat*) mat.data());
    } else {
        GLfloat fmat[16];
        gl->glGetFloatv(param, fmat);

        float *r = mat.data();
        for (int i = 0; i < 16; ++i)
            r[i] = fmat[i];
    }
    mat.optimize();
    return mat;
}

static inline void qVertex(QOpenGLFunctions_2_1 *gl, int count, const QVector3D *v)
{
    while (count--) {
        if (sizeof(qreal) == sizeof(GLfloat))
            gl->glVertex3f(v->x(), v->y(), v->z());
        else
            gl->glVertex3d(v->x(), v->y(), v->z());
        v++;
    }
}


LDraw::GLRenderer::GLRenderer(QObject *parent)
    : QObject(parent), m_part(0), m_color(-1), m_zoom(1), m_surfaces_list(0), m_initialized(false), m_resized(false)
{
    memset(&m_rx, 0, sizeof(qreal) * 3);
    memset(&m_tx, 0, sizeof(qreal) * 3);

    m_animation = new QTimer(this);
    m_animation->setInterval(50);

    connect(m_animation, SIGNAL(timeout()), this, SLOT(animationStep()));
}

LDraw::GLRenderer::~GLRenderer()
{
    setPartAndColor(0, -1);
}

void LDraw::GLRenderer::setPartAndColor(Part *part, int color)
{
    m_part = part;
    m_color = color;

    if (m_initialized && m_resized) {
        resizeGL(glCurrentContext, m_size.width(), m_size.height());
        createSurfacesList();
        updateNeeded();
    }
}

LDraw::Part *LDraw::GLRenderer::part() const
{
    return m_part;
}

int LDraw::GLRenderer::color() const
{
    return m_color;
}


void LDraw::GLRenderer::setXRotation(qreal r)
{
    if (qAbs(m_rx - r) > 0.00001f) {
        m_rx = r;
        updateNeeded();
    }
}

void LDraw::GLRenderer::setYRotation(qreal r)
{
    if (qAbs(m_ry - r) > 0.00001f) {
        m_ry = r;
        updateNeeded();
    }
}

void LDraw::GLRenderer::setZRotation(qreal r)
{
    if (qAbs(m_rz - r) > 0.00001f) {
        m_rz = r;
        updateNeeded();
    }
}

void LDraw::GLRenderer::setXTranslation(qreal t)
{
    if (qAbs(m_tx - t) > 0.00001f) {
        m_tx = t;
        updateNeeded();
    }
}

void LDraw::GLRenderer::setYTranslation(qreal t)
{
    if (qAbs(m_ty - t) > 0.00001f) {
        m_ty = t;
        updateNeeded();
    }
}

void LDraw::GLRenderer::setZTranslation(qreal t)
{
    if (qAbs(m_tz - t) > 0.00001f) {
        m_tz = t;
        updateNeeded();
    }
}

void LDraw::GLRenderer::setZoom(qreal z)
{
    if (qAbs(m_zoom - z) > 0.00001f) {
        m_zoom = z;
        updateNeeded();
    }
}

void LDraw::GLRenderer::initializeGL(QOpenGLContext *context)
{
    glCurrentContext = nullptr;
    gl = context->versionFunctions<QOpenGLFunctions_2_1>();
    if (!gl) {
         qWarning() << "Could not obtain required OpenGL context version";
         m_initialized = false;
         return;
     }
    glCurrentContext = context;

    gl->glMatrixMode(GL_MODELVIEW);
    createSurfacesList();

    gl->glShadeModel(GL_FLAT);
    gl->glEnable(GL_DEPTH_TEST);

    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gl->glLineWidth(1.5);
    gl->glEnable(GL_LINE_SMOOTH);
    gl->glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

//    glClearColor(0, 0, 0, 0); // transparent
    gl->glClearColor(255, 255, 255, 255); // white
}

void LDraw::GLRenderer::resizeGL(QOpenGLContext *context, int w, int h)
{
    if (!glCurrentContext || !gl || (context != glCurrentContext))
        return;

    int side = qMin(w, h);
    gl->glViewport((w - side) / 2, (h - side) / 2, side, side);

    gl->glMatrixMode(GL_PROJECTION);
    gl->glLoadIdentity();

    qreal radius = 1.0;

    if (m_part) {
        QVector3D vmin, vmax;

        if (m_part->boundingBox(vmin, vmax)) {
            //qWarning("resizeGL - ortho: [%.f ..  %.f] x [%.f .. %.f] x [%.f .. %.f]", vmin.x(), vmax.x(), vmin.y(), vmax.y(), vmin.z(), vmax.z());

            m_center = (vmin + vmax) / 2;
            radius = (vmax - vmin).length() / 2;

            //qWarning(" --> center: (%.f, %.f, %.f)  radius: %.f", m_center.x(), m_center.y(), m_center.z(), radius);
        }
    }

    gl->glOrtho(m_center.x() - radius, m_center.x() + radius,
            m_center.y() - radius, m_center.y() + radius,
            -m_center.z() - 2 * radius, -m_center.z() + 2 * radius);
    gl->glMatrixMode(GL_MODELVIEW);

    m_resized = true;
    m_size = QSize(w, h);
}

void LDraw::GLRenderer::paintGL(QOpenGLContext *context)
{
    if (!glCurrentContext || !gl || (context != glCurrentContext)
            || !m_resized || m_size.isEmpty() || !m_part || m_color < 0) {
        return;
    }

    //qWarning("paintGL - rotate: %.f / %.f / %.f", m_rx, m_ry, m_rz);

    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->glLoadIdentity();
    gl->glTranslated(m_tx, m_ty, m_tz);
    gl->glTranslated(m_center.x(), m_center.y(), m_center.z());
    gl->glRotated(m_rx, 1.0, 0.0, 0.0);
    gl->glRotated(m_ry, 0.0, 1.0, 0.0);
    gl->glRotated(m_rz, 0.0, 0.0, 1.0);
    gl->glTranslated(-m_center.x(), -m_center.y(), -m_center.z());
    gl->glScaled(m_zoom, m_zoom, m_zoom);

//    glEnable(GL_LIGHTING);
//    glEnable(GL_LIGHT0);

    gl->glEnable(GL_POLYGON_OFFSET_FILL);
    gl->glPolygonOffset(1, 1);

    gl->glCallList(m_surfaces_list);
    //renderSurfaces(m_part, m_color);

    gl->glDisable(GL_POLYGON_OFFSET_FILL);
    gl->glDepthMask(GL_FALSE);

    renderLines(m_part, m_color);

    gl->glDepthMask(GL_TRUE);
}


void LDraw::GLRenderer::createSurfacesList()
{
    if (m_surfaces_list) {
        makeCurrent();
        gl->glDeleteLists(m_surfaces_list, 1);
        m_surfaces_list = 0;
    }

    if (m_part) {
        m_surfaces_list = gl->glGenLists(1);
        gl->glNewList(m_surfaces_list, GL_COMPILE);
            renderSurfaces(m_part, m_color);
        gl->glEndList();
    }
}


void LDraw::GLRenderer::renderSurfaces(Part *part, int ldraw_basecolor)
{
    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;

    int began = -1;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        switch (e->type()) {
        case Element::Triangle: {
            const TriangleElement *te = static_cast<const TriangleElement *>(e);

            QColor col = core()->color(te->color(), ldraw_basecolor);
            gl->glColor4f(col.redF(), col.greenF(), col.blueF(), col.alphaF());
            if (began != GL_TRIANGLES) {
                if (began >= 0)
                    gl->glEnd();
                gl->glBegin(GL_TRIANGLES);
                began = GL_TRIANGLES;
            }
            qVertex(gl, 3, te->points());
            break;
        }
        case Element::Quad: {
            const QuadElement *qe = static_cast<const QuadElement *>(e);

            QColor col = core()->color(qe->color(), ldraw_basecolor);
            gl->glColor4f(col.redF(), col.greenF(), col.blueF(), col.alphaF());
            if (began != GL_QUADS) {
                if (began >= 0)
                    gl->glEnd();
                gl->glBegin(GL_QUADS);
                began = GL_QUADS;
            }
            qVertex(gl, 4, qe->points());
            break;
        }
        case Element::Part: {
            const PartElement *pe = static_cast<const PartElement *>(e);

            if (began >= 0) {
                gl->glEnd();
                began = -1;
            }
            gl->glPushMatrix();
            qMultMatrix(gl, pe->matrix());

            renderSurfaces(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());

            gl->glPopMatrix();
            break;
        }
        default: {
            break;
        }
        }
    }
    if (began >= 0)
        gl->glEnd();
}

void LDraw::GLRenderer::renderLines(Part *part, int ldraw_basecolor)
{
    QMatrix4x4 proj_movi;
    GLint view[4];
    bool proj_movi_init = false;

    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;

    QVector<linebuffer> buffer;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        switch (e->type()) {
        case Element::Line: {
            const LineElement *le = static_cast<const LineElement *>(e);

            linebuffer lb = { le->points(), le->color() };
            buffer.append(lb);
            break;
        }
        case Element::CondLine: {
            const CondLineElement *le = static_cast<const CondLineElement *>(e);
            const QVector3D *v = le->points();

            if (!proj_movi_init) {
                gl->glGetIntegerv(GL_VIEWPORT, view);
                proj_movi = qMatrixFromGL(gl, GL_PROJECTION_MATRIX) * qMatrixFromGL(gl, GL_MODELVIEW_MATRIX);
                proj_movi_init = true;
            }
            QVector3D pv[4];

            for (int i = 0; i < 4; i++) {
                // gluProject(v[i][0], v[i][1], v[i][2], movi, proj, view, &pv[i][0], &pv[i][1], &pv[i][2]);
                // would do the job, but this is double only...

                pv[i] = proj_movi * v[i];
                pv[i].setX(view[0] + view[2] * (pv[i].x() + 1) / 2);
                pv[i].setY(view[1] + view[3] * (pv[i].y() + 1) / 2);
                pv[i].setZ(0); // (pv[i][2] + 1) / 2;
            }

            QVector3D line_norm = QVector3D::crossProduct(pv[1] - pv[0], QVector3D(0, 0, -1));

            if ((QVector3D::dotProduct(line_norm, pv[0] - pv[2]) < 0) ==
                (QVector3D::dotProduct(line_norm, pv[0] - pv[3]) < 0)) {
                linebuffer lb = { le->points(), le->color() };
                buffer.append(lb);
            }
            break;
        }
        case Element::Part: {
            const PartElement *pe = static_cast<const PartElement *>(e);
            gl->glPushMatrix();
            qMultMatrix(gl, pe->matrix());

            renderLines(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());

            gl->glPopMatrix();
            break;
        }
        default: {
            break;
        }
        }
    }
    renderLineBuffer(buffer, ldraw_basecolor);
}

void LDraw::GLRenderer::renderLineBuffer(const QVector<linebuffer> &buffer, int ldraw_basecolor)
{
    bool strip = false;
    int pos0 = 0;

    for (int i = 1; i < buffer.size(); ++i) {
        bool strip_to_last = (buffer[i-1].v[1] == buffer[i].v[0]);

        if (!strip && strip_to_last) {
            renderLineBufferSegment(buffer, pos0, i-2, GL_LINES, ldraw_basecolor);
            pos0 = i - 1;
            strip = true;
        } else if (strip && strip_to_last && (buffer[pos0].v[0] == buffer[i].v[1])) {
            renderLineBufferSegment(buffer, pos0, i, GL_LINE_LOOP, ldraw_basecolor);
            pos0 = i + 1;
            strip = false;
        } else if (strip && !strip_to_last) {
            renderLineBufferSegment(buffer, pos0, i-1, GL_LINE_STRIP, ldraw_basecolor);
            pos0 = i;
            strip = false;
        }
    }
    if (pos0 < buffer.size())
        renderLineBufferSegment(buffer, pos0, buffer.size() - 1, strip ? GL_LINE_STRIP : GL_LINES, ldraw_basecolor);
}

void LDraw::GLRenderer::renderLineBufferSegment(const QVector<linebuffer> &buffer, int s, int e, int mode, int ldraw_basecolor)
{
    if (e < s)
        return;

    int color = -1;

    gl->glBegin(mode);
    for (int i = s; i <= e; ++i) {
        if (buffer[i].color != color) {
            color = buffer[i].color;
            QColor col = core()->color(color, ldraw_basecolor);
            gl->glColor4f(col.redF(), col.greenF(), col.blueF(), col.alphaF());
        }

        if (i == s || mode == GL_LINES)
            qVertex(gl, 1, &buffer[i].v[0]);
        qVertex(gl, 1, &buffer[i].v[1]);
    }
    gl->glEnd();
}



#if 0
void LDraw::GLRenderer::render_condlines_visit(Part *part, int ldraw_basecolor)
{
    QMatrix4x4 proj_movi;
    bool proj_movi_init = false;

    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;

    int began = -1;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        switch (e->type()) {
        case Element::CondLine: {
            const CondLineElement *le = static_cast<const CondLineElement *>(e);
            const QVector3D *v = le->points();
            int view[4];
            glGetIntegerv(GL_VIEWPORT, view);

            if (!proj_movi_init) {
                proj_movi = qMatrixFromGL(GL_MODELVIEW_MATRIX) * qMatrixFromGL(GL_PROJECTION_MATRIX);
                proj_movi_init = true;
            }
            QVector3D pv[4];

            for (int i = 0; i < 4; i++) {
                // gluProject(v[i][0], v[i][1], v[i][2], movi, proj, view, &pv[i][0], &pv[i][1], &pv[i][2]);
                // would do the job, but this is double only...

                pv[i] = proj_movi * v[i];
                pv[i].setX(view[0] + view[2] * (pv[i].x() + 1) / 2);
                pv[i].setY(view[1] + view[3] * (pv[i].y() + 1) / 2);
                pv[i].setZ(0); // (pv[i][2] + 1) / 2;
            }

            QVector3D line_norm = QVector3D::crossProduct(pv[1] - pv[0], QVector3D(0, 0, -1));
            if ((QVector3D::dotProduct(line_norm, pv[0] - pv[2]) < 0) ==
                (QVector3D::dotProduct(line_norm, pv[0] - pv[3]) < 0)) {
                qglColor(core()->color(le->color(), ldraw_basecolor));

                if (began != GL_LINES) {
                    if (began >= 0)
                        glEnd();
                    glBegin(GL_LINES);
                    began = GL_LINES;
                }
                qVertex(2, v);
            }
            break;
        }
        case Element::Part: {
            const PartElement *pe = static_cast<const PartElement *>(e);

            if (began >= 0) {
                glEnd();
                began = -1;
            }
            glPushMatrix();
            glMultMatrix(pe->matrix());

            render_condlines_visit(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());

            glPopMatrix();
            break;
        }
        default: {
            break;
        }
        }
    }
    if (began >= 0)
        glEnd();
}

#endif















LDraw::RenderWidget::RenderWidget(QWidget *parent)
    : QOpenGLWidget(/*QGLFormat(QGL::SampleBuffers | QGL::Rgba | QGL::AlphaChannel),*/ parent)
{
    m_renderer = new GLRenderer(this);

    resetCamera();

    connect(m_renderer, SIGNAL(makeCurrent()), this, SLOT(slotMakeCurrent()));
    connect(m_renderer, SIGNAL(updateNeeded()), this, SLOT(updateGL()));

    setCursor(Qt::OpenHandCursor);
}

LDraw::RenderWidget::~RenderWidget()
{
}

QSize LDraw::RenderWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize LDraw::RenderWidget::sizeHint() const
{
    return QSize(400, 400);
}

void LDraw::RenderWidget::mousePressEvent(QMouseEvent *e)
{
    m_last_pos = e->pos();
    setCursor(Qt::ClosedHandCursor);
}

void LDraw::RenderWidget::mouseReleaseEvent(QMouseEvent *)
{
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWidget::mouseMoveEvent(QMouseEvent *e)
{
    qreal dx = e->x() - m_last_pos.x();
    qreal dy = e->y() - m_last_pos.y();

    if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::NoModifier)) {
        m_renderer->setXRotation(m_renderer->xRotation() + dy / 2);
        m_renderer->setYRotation(m_renderer->yRotation() + dx / 2);
    }
    else if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::ControlModifier)) {
        m_renderer->setXRotation(m_renderer->xRotation() + dy / 2);
        m_renderer->setZRotation(m_renderer->zRotation() + dx / 2);
    }
    else if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::ShiftModifier)) {
        m_renderer->setXTranslation(m_renderer->xTranslation() + dx);
        m_renderer->setYTranslation(m_renderer->yTranslation() - dy);
    }
    else if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::AltModifier)) {
        m_renderer->setZoom(m_renderer->zoom() * (dy < 0 ? 0.9 : 1.1));
    }
    m_last_pos = e->pos();
}

void LDraw::RenderWidget::wheelEvent(QWheelEvent *e)
{
    qreal d = 1.0 + (e->delta() / 1200.0);
    m_renderer->setZoom(m_renderer->zoom() * d);
}

void LDraw::RenderWidget::slotMakeCurrent()
{
    makeCurrent();
}

void LDraw::RenderWidget::initializeGL()
{
    m_renderer->initializeGL(context());
}

void LDraw::RenderWidget::resizeGL(int w, int h)
{
    m_renderer->resizeGL(context(), w, h);
}

void LDraw::RenderWidget::paintGL()
{
    m_renderer->paintGL(context());
}


















LDraw::RenderOffscreenWidget::RenderOffscreenWidget(QWidget *parent)
    : QWidget(parent), m_fbo(0), m_initialized(false), m_resize(false)
{
    //m_dummy = new QGLWidget(QGLFormat(QGL::SampleBuffers | QGL::Rgba | QGL::AlphaChannel));

    if (!QOpenGLFramebufferObject::hasOpenGLFramebufferObjects())
        qWarning("no OpenGL FBO extension available");

    m_renderer = new GLRenderer(this);

    resetCamera();

    connect(m_renderer, SIGNAL(makeCurrent()), this, SLOT(slotMakeCurrent()));
    connect(m_renderer, SIGNAL(updateNeeded()), this, SLOT(update()));

    setCursor(Qt::OpenHandCursor);
}

LDraw::RenderOffscreenWidget::~RenderOffscreenWidget()
{
    //delete m_dummy;
}

QSize LDraw::RenderOffscreenWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize LDraw::RenderOffscreenWidget::sizeHint() const
{
    return QSize(400, 400);
}

void LDraw::RenderOffscreenWidget::resizeEvent(QResizeEvent *)
{
    setImageSize(width(), height());
}

void LDraw::RenderOffscreenWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawImage(0, 0, renderImage());
}

void LDraw::RenderOffscreenWidget::mousePressEvent(QMouseEvent *e)
{
    m_last_pos = e->pos();
    setCursor(Qt::ClosedHandCursor);
}

void LDraw::RenderOffscreenWidget::mouseReleaseEvent(QMouseEvent *)
{
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderOffscreenWidget::mouseMoveEvent(QMouseEvent *e)
{
    qreal dx = e->x() - m_last_pos.x();
    qreal dy = e->y() - m_last_pos.y();

    if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::NoModifier)) {
        m_renderer->setXRotation(m_renderer->xRotation() + dy / 2);
        m_renderer->setYRotation(m_renderer->yRotation() + dx / 2);
    }
    else if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::ControlModifier)) {
        m_renderer->setXRotation(m_renderer->xRotation() + dy / 2);
        m_renderer->setZRotation(m_renderer->zRotation() + dx / 2);
    }
    else if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::ShiftModifier)) {
        m_renderer->setXTranslation(m_renderer->xTranslation() + dx);
        m_renderer->setYTranslation(m_renderer->yTranslation() - dy);
    }
    else if ((e->buttons() & Qt::LeftButton) && (e->modifiers() == Qt::AltModifier)) {
        m_renderer->setZoom(m_renderer->zoom() * (dy < 0 ? 0.9 : 1.1));
    }
    m_last_pos = e->pos();
}

void LDraw::RenderOffscreenWidget::wheelEvent(QWheelEvent *e)
{
    qreal d = 1.0 + (e->delta() / 1200.0);
    m_renderer->setZoom(m_renderer->zoom() * d);
}

void LDraw::RenderOffscreenWidget::slotMakeCurrent()
{
    m_dummy->makeCurrent();
}

void LDraw::RenderOffscreenWidget::setImageSize(int w, int h)
{
    QSize s(w, h);

    if (!m_fbo || s != m_fbo->size()) {
        m_dummy->makeCurrent();
        delete m_fbo;
        m_fbo = new QOpenGLFramebufferObject(s/*, QGLFramebufferObject::Depth*/);
        m_resize = true;
    }
}

QImage LDraw::RenderOffscreenWidget::renderImage()
{
    if (!m_fbo)
        return QImage();

    m_dummy->makeCurrent();
    m_fbo->bind();
    if (!m_initialized) {
        m_renderer->initializeGL(QOpenGLContext::currentContext());
//        m_dummy->qglClearColor(QColor(255, 255, 255, 0));
        m_initialized = true;
    }

    if (m_resize) {
        m_renderer->resizeGL(QOpenGLContext::currentContext(), m_fbo->size().width(), m_fbo->size().height());
        m_resize = false;
    }
    m_renderer->paintGL(QOpenGLContext::currentContext());
    m_fbo->release();
    //m_context->doneCurrent();
    return m_fbo->toImage();
}

void LDraw::GLRenderer::startAnimation()
{
    if (!m_animation->isActive())
        m_animation->start();
}

void LDraw::GLRenderer::stopAnimation()
{
    if (m_animation->isActive())
        m_animation->stop();
}

bool LDraw::GLRenderer::isAnimationActive() const
{
    return m_animation->isActive();
}

void LDraw::GLRenderer::animationStep()
{
    setXRotation(xRotation()+1.5);
    setYRotation(yRotation()+1);
    setZRotation(zRotation()+0.75);
}

void LDraw::RenderOffscreenWidget::resetCamera()
{
    m_renderer->setXRotation(-180+30);
    m_renderer->setYRotation(45);
    m_renderer->setZRotation(0);

    m_renderer->setXTranslation(0);
    m_renderer->setYTranslation(0);
    m_renderer->setZTranslation(0);

    m_renderer->setZoom(1);
}

void LDraw::RenderWidget::resetCamera()
{
    m_renderer->setXRotation(-180+30);
    m_renderer->setYRotation(45);
    m_renderer->setZRotation(0);

    m_renderer->setXTranslation(0);
    m_renderer->setYTranslation(0);
    m_renderer->setZTranslation(0);

    m_renderer->setZoom(1);
}

void LDraw::RenderWidget::startAnimation()
{
    m_renderer->startAnimation();
}

void LDraw::RenderWidget::stopAnimation()
{
    m_renderer->stopAnimation();
}

void LDraw::RenderOffscreenWidget::startAnimation()
{
    m_renderer->startAnimation();
}

void LDraw::RenderOffscreenWidget::stopAnimation()
{
    m_renderer->stopAnimation();
}

#endif // !QT_NO_OPENGL
