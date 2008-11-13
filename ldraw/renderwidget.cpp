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
#include <QMouseEvent>

#include <cfloat>
#include <cmath>

#include "ldraw.h"
#include "renderwidget.h"
#include "matrix_t.h"


static inline void glVertex(const vector_t &v)
{
    glVertex3fv(v);
}

static inline void glMultMatrix(const matrix_t &m)
{
    glMultMatrixf(m);
}



LDraw::GLRenderer::GLRenderer()
    : m_part(0), m_color(-1), m_zoom(1), m_surfaces_list(0), m_initialized(false), m_resized(false)
{
    memset(&m_rx, 0, sizeof(qreal) * 3);
    memset(&m_tx, 0, sizeof(qreal) * 3);
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
        resizeGL(m_size.width(), m_size.height());
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

void LDraw::GLRenderer::initializeGL()
{
    glMatrixMode(GL_MODELVIEW);
    createSurfacesList();

    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(1.5);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glClearColor(0, 0, 0, 0); // transparent

    m_initialized = true;
}

void LDraw::GLRenderer::resizeGL(int w, int h)
{
    if (!m_initialized)
        return;

    int side = qMin(w, h);
    glViewport((w - side) / 2, (h - side) / 2, side, side);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    qreal radius = 1.0;
    vector_t center;

    if (m_part) {
        vector_t vmin, vmax;

        if (m_part->boundingBox(vmin, vmax)) {
            qWarning("resizeGL - ortho: [%.f ..  %.f] x [%.f .. %.f] x [%.f .. %.f]", vmin[0], vmax[0], vmin[1], vmax[1], vmin[2], vmax[2]);

            radius = (vmax - vmin).length(); //qMax(vmin.length(), vmax.length());
            center = vmin + (vmax - vmin) / 2;
        }
    }

    glOrtho(center[0]-radius, center[0]+radius, center[1]-radius, center[1]+radius, -1.0-radius, 1.0+radius);
    glMatrixMode(GL_MODELVIEW);

    m_resized = true;
    m_size = QSize(w, h);
}

void LDraw::GLRenderer::paintGL()
{
    if (!m_initialized || !m_resized || m_size.isEmpty() || !m_part || m_color <= 0)
        return;

    //qWarning("paintGL - rotate: %.f / %.f / %.f", m_rx, m_ry, m_rz);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslated(m_tx, m_ty, m_tz);
    glRotated(m_rx, 1.0, 0.0, 0.0);
    glRotated(m_ry, 0.0, 1.0, 0.0);
    glRotated(m_rz, 0.0, 0.0, 1.0);
    glScaled(m_zoom, m_zoom, m_zoom);

//    glEnable(GL_LIGHTING);
//    glEnable(GL_LIGHT0);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);

    glCallList(m_surfaces_list);
    //renderSurfaces(m_part, m_color);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_FALSE);

    renderLines(m_part, m_color);

    glDepthMask(GL_TRUE);
}


void LDraw::GLRenderer::createSurfacesList()
{
    if (m_surfaces_list) {
        makeCurrent();
        glDeleteLists(m_surfaces_list, 1);
        m_surfaces_list = 0;
    }

    if (m_part) {
        m_surfaces_list = glGenLists(1);
        glNewList(m_surfaces_list, GL_COMPILE);
            renderSurfaces(m_part, m_color);
        glEndList();
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
            const vector_t *v = te->points();

            QColor col = core()->color(te->color(), ldraw_basecolor);
            glColor4f(col.redF(), col.greenF(), col.blueF(), col.alphaF());
            if (began != GL_TRIANGLES) {
                if (began >= 0)
                    glEnd();
                glBegin(GL_TRIANGLES);
                began = GL_TRIANGLES;
            }
            glVertex(v[0]);
            glVertex(v[1]);
            glVertex(v[2]);
            break;
        }
        case Element::Quad: {
            const QuadElement *qe = static_cast<const QuadElement *>(e);
            const vector_t *v = qe->points();

            QColor col = core()->color(qe->color(), ldraw_basecolor);
            glColor4f(col.redF(), col.greenF(), col.blueF(), col.alphaF());
            if (began != GL_QUADS) {
                if (began >= 0)
                    glEnd();
                glBegin(GL_QUADS);
                began = GL_QUADS;
            }
            glVertex(v[0]);
            glVertex(v[1]);
            glVertex(v[2]);
            glVertex(v[3]);
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

            renderSurfaces(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());

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

void LDraw::GLRenderer::renderLines(Part *part, int ldraw_basecolor)
{
    matrix_t proj_movi;
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
            const vector_t *v = le->points();
            int view[4];
            glGetIntegerv(GL_VIEWPORT, view);

            if (!proj_movi_init) {
                proj_movi = matrix_t::fromGL(GL_MODELVIEW_MATRIX) * matrix_t::fromGL(GL_PROJECTION_MATRIX);
                proj_movi_init = true;
            }
            vector_t pv[4];

            for (int i = 0; i < 4; i++) {
                // gluProject(v[i][0], v[i][1], v[i][2], movi, proj, view, &pv[i][0], &pv[i][1], &pv[i][2]);
                // would do the job, but this is double only...

                pv[i] = proj_movi * v[i];
                pv[i][0] = view[0] + view[2] * (pv[i][0] + 1) / 2;
                pv[i][1] = view[1] + view[3] * (pv[i][1] + 1) / 2;
                pv[i][2] = 0; // (pv[i][2] + 1) / 2;
            }

            vector_t line_norm = (pv[1] - pv[0]) % vector_t(0, 0, -1);

            if ((line_norm * (pv[0] - pv[2]) < 0) == (line_norm * (pv[0] - pv[3]) < 0)) {
                linebuffer lb = { le->points(), le->color() };
                buffer.append(lb);
            }
            break;
        }
        case Element::Part: {
            const PartElement *pe = static_cast<const PartElement *>(e);
            glPushMatrix();
            glMultMatrix(pe->matrix());

            renderLines(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());

            glPopMatrix();
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

    glBegin(mode);
    for (int i = s; i <= e; ++i) {
        if (buffer[i].color != color) {
            color = buffer[i].color;
            QColor col = core()->color(color, ldraw_basecolor);
            glColor4f(col.redF(), col.greenF(), col.blueF(), col.alphaF());
        }

        if (i == s || mode == GL_LINES)
            glVertex(buffer[i].v[0]);
        glVertex(buffer[i].v[1]);
    }
    glEnd();
}



#if 0
void LDraw::GLRenderer::render_condlines_visit(Part *part, int ldraw_basecolor)
{
    matrix_t proj_movi;
    bool proj_movi_init = false;

    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;

    int began = -1;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        switch (e->type()) {
        case Element::CondLine: {
            const CondLineElement *le = static_cast<const CondLineElement *>(e);
            const vector_t *v = le->points();
            int view[4];
            glGetIntegerv(GL_VIEWPORT, view);

            if (!proj_movi_init) {
                proj_movi = matrix_t::fromGL(GL_MODELVIEW_MATRIX) * matrix_t::fromGL(GL_PROJECTION_MATRIX);
                proj_movi_init = true;
            }
            vector_t pv[4];

            for (int i = 0; i < 4; i++) {
                // gluProject(v[i][0], v[i][1], v[i][2], movi, proj, view, &pv[i][0], &pv[i][1], &pv[i][2]);
                // would do the job, but this is double only...

                pv[i] = proj_movi * v[i];
                pv[i][0] = view[0] + view[2] * (pv[i][0] + 1) / 2;
                pv[i][1] = view[1] + view[3] * (pv[i][1] + 1) / 2;
                pv[i][2] = 0; // (pv[i][2] + 1) / 2;
            }

            vector_t line_norm = (pv[1] - pv[0]) % vector_t(0, 0, -1);
            if ((line_norm * (pv[0] - pv[2]) < 0) == (line_norm * (pv[0] - pv[3]) < 0)) {
                qglColor(core()->color(le->color(), ldraw_basecolor));

                if (began != GL_LINES) {
                    if (began >= 0)
                        glEnd();
                    glBegin(GL_LINES);
                    began = GL_LINES;
                }
                glVertex(v[0]);
                glVertex(v[1]);
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
    : QGLWidget(QGLFormat(QGL::SampleBuffers | QGL::Rgba | QGL::AlphaChannel), parent), GLRenderer()
{
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

void LDraw::RenderWidget::mousePressEvent(QMouseEvent *event)
{
    m_last_pos = event->pos();
}

void LDraw::RenderWidget::mouseMoveEvent(QMouseEvent *event)
{
    qreal dx = event->x() - m_last_pos.x();
    qreal dy = event->y() - m_last_pos.y();

    if ((event->buttons() & Qt::LeftButton) && (event->modifiers() == Qt::NoModifier)) {
        setXRotation(xRotation() + dy / 2);
        setYRotation(yRotation() + dx / 2);
    }
    else if ((event->buttons() & Qt::LeftButton) && (event->modifiers() == Qt::ShiftModifier)) {
        setXRotation(xRotation() + dy / 2);
        setZRotation(zRotation() + dx / 2);
    }
    else if ((event->buttons() & Qt::LeftButton) && (event->modifiers() == Qt::ControlModifier)) {
        setZoom(zoom() * (dy < 0 ? 0.9 : 1.1));
    }
    m_last_pos = event->pos();
}

void LDraw::RenderWidget::makeCurrent()
{
    QGLWidget::makeCurrent();
}

void LDraw::RenderWidget::updateNeeded()
{
    updateGL();
}

