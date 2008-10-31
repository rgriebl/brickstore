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


//#define USE_CALL_LIST 1

LDraw::RenderWidget::RenderWidget(QWidget *parent)
: QGLWidget(QGLFormat(QGL::SampleBuffers | QGL::Rgba | QGL::AlphaChannel), parent), m_part(0), m_color(-1), m_dl_part(0), m_initialized(false), m_zoom(1)
{
    memset(&m_rx, 0, sizeof(qreal) * 3);
    //setAutoFillBackground(true);
}

LDraw::RenderWidget::~RenderWidget()
{
    setPartAndColor(0, -1);
}

QSize LDraw::RenderWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}
        
QSize LDraw::RenderWidget::sizeHint() const
{
    return QSize(400, 400);
}
                
                

void LDraw::RenderWidget::setPartAndColor(Part *part, int color)
{
    m_part = part;
    m_color = color;
    
    if (m_initialized) {
        resizeGL(width(), height());
        create_display_list();
        updateGL();
    }
}

LDraw::Part *LDraw::RenderWidget::part() const
{
    return m_part;
}

int LDraw::RenderWidget::color() const
{
    return m_color;
}


void LDraw::RenderWidget::setXRotation(qreal r)
{
    if (qAbs(m_rx - r) > 0.00001f) {
        m_rx = r;
        update();
    }
}

void LDraw::RenderWidget::setYRotation(qreal r)
{
    if (qAbs(m_ry - r) > 0.00001f) {
        m_ry = r;
        updateGL();
    }
}

void LDraw::RenderWidget::setZRotation(qreal r)
{
    if (qAbs(m_rz - r) > 0.00001f) {
        m_rz = r;
        updateGL();
    }
}

void LDraw::RenderWidget::setZoom(qreal z)
{
    if (qAbs(m_zoom - z) > 0.00001f) {
        m_zoom = z;
        updateGL();
    }    
}

void LDraw::RenderWidget::initializeGL()
{
    qWarning("initGL");

    qglClearColor(Qt::transparent);
    
    glMatrixMode(GL_MODELVIEW);
    create_display_list();
    
    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glLineWidth(1.5);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    m_initialized = true;
}

void LDraw::RenderWidget::resizeGL(int w, int h)
{

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
}

void LDraw::RenderWidget::paintGL()
{
    if (!m_part || m_color <= 0)
        return;

    //qWarning("paintGL - rotate: %.f / %.f / %.f", m_rx, m_ry, m_rz);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslated(0.0, 0.0, 0.0);
    glScaled(m_zoom, m_zoom, m_zoom);
    glRotated(m_rx, 1.0, 0.0, 0.0);
    glRotated(m_ry, 0.0, 1.0, 0.0);
    glRotated(m_rz, 0.0, 0.0, 1.0);

//    glEnable(GL_LIGHTING);
//    glEnable(GL_LIGHT0);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);

#if USE_CALL_LIST
    glCallList(m_dl_part);
#else
    render_surfaces_visit(m_part, m_color);
#endif
    glDisable(GL_POLYGON_OFFSET_FILL);

    glDepthMask(GL_FALSE);
    render_lines_visit(m_part, m_color);
    render_condlines_visit(m_part, m_color);
    glDepthMask(GL_TRUE);
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
        setXRotation(m_rx + dy / 2);
        setYRotation(m_ry + dx / 2);
    } 
    else if ((event->buttons() & Qt::LeftButton) && (event->modifiers() == Qt::ShiftModifier)) {
        setXRotation(m_rx + dy / 2);
        setZRotation(m_rz + dx / 2);
    }
    else if ((event->buttons() & Qt::LeftButton) && (event->modifiers() == Qt::ControlModifier)) {
        setZoom(m_zoom * (dy < 0 ? 0.9 : 1.1));
    }
    m_last_pos = event->pos();
}

void LDraw::RenderWidget::create_display_list()
{
    if (m_dl_part) {
        makeCurrent();
        glDeleteLists(m_dl_part, 1);
        m_dl_part = 0;
    }

#if USE_CALL_LIST
    if (m_part) {
        m_dl_part = glGenLists(1);
        glNewList(m_dl_part, GL_COMPILE);
            render_surfaces_visit(m_part, m_color);
            render_lines_visit(m_part, m_color);
        glEndList();
    }
#endif
}

static inline void glVertex(const vector_t &v)
{
    glVertex3fv(v);
}

static inline void glMultMatrix(const matrix_t &m)
{
    glMultMatrixf(m);
}

void LDraw::RenderWidget::render_condlines_visit(Part *part, int ldraw_basecolor)
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

void LDraw::RenderWidget::render_surfaces_visit(Part *part, int ldraw_basecolor)
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

            qglColor(core()->color(te->color(), ldraw_basecolor));
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
            
            qglColor(core()->color(qe->color(), ldraw_basecolor));
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
            
            render_surfaces_visit(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());
            
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

void LDraw::RenderWidget::renderOptimizedLines(int s, int e, const QVector<linebuffer> &buffer, int mode, int ldraw_basecolor)
{
    if (e < s)
        return;

    int color = -1;

    glBegin(mode);
    for (int i = s; i <= e; ++i) {
        if (buffer[i].color != color) {
            color = buffer[i].color;
            qglColor(core()->color(color, ldraw_basecolor));
        }

        if (i == s || mode == GL_LINES)
            glVertex(buffer[i].v[0]);
        glVertex(buffer[i].v[1]);
    }
    glEnd(); return;
    if (mode == GL_LINE_LOOP || mode == GL_LINE_STRIP) {
        glBegin(GL_POINTS);
            for (int i = s; i <= e; ++i) {
                if (buffer[i].color != color) {
                    color = buffer[i].color;
                    qglColor(core()->color(color, ldraw_basecolor));
                }

                glVertex(buffer[i].v[0]);
                if (mode == GL_LINE_STRIP && i == e)
                    glVertex(buffer[i].v[1]);
            }
        glEnd();
    }
}

void LDraw::RenderWidget::optimizeLines(const QVector<linebuffer> &buffer, int ldraw_basecolor)
{
    bool strip = false;
    int pos0 = 0;

    for (int i = 1; i < buffer.size(); ++i) {
        bool strip_to_last = (buffer[i-1].v[1] == buffer[i].v[0]);

        if (!strip && strip_to_last) {
            renderOptimizedLines(pos0, i-2, buffer, GL_LINES, ldraw_basecolor);
            pos0 = i - 1;
            strip = true;
        } else if (strip && strip_to_last && (buffer[pos0].v[0] == buffer[i].v[1])) {
            renderOptimizedLines(pos0, i, buffer, GL_LINE_LOOP, ldraw_basecolor);
            pos0 = i + 1;
            strip = false;
        } else if (strip && !strip_to_last) {
            renderOptimizedLines(pos0, i-1, buffer, GL_LINE_STRIP, ldraw_basecolor);
            pos0 = i;
            strip = false;
        }
    }
    if (pos0 < buffer.size())
        renderOptimizedLines(pos0, buffer.size() - 1, buffer, strip ? GL_LINE_STRIP : GL_LINES, ldraw_basecolor);
}


void LDraw::RenderWidget::render_lines_visit(Part *part, int ldraw_basecolor)
{
    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;

    QVector<linebuffer> buffer;

//    int began = -1;
    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        switch (e->type()) {
        case Element::Line: {
            const LineElement *le = static_cast<const LineElement *>(e);
            //const vector_t *v = le->points();
            linebuffer lb = { le->points(), le->color() };

            buffer.append(lb);

            /*
            qglColor(core()->color(le->color(), ldraw_basecolor));
            if (began != GL_LINES) {
                if (began >= 0)
                    glEnd();
                glBegin(GL_LINES);
                began = GL_LINES;
            }
            glVertex(v[0]);
            glVertex(v[1]);
            */
            break;
        }
        case Element::Part: {
            const PartElement *pe = static_cast<const PartElement *>(e);
/*
            if (began >= 0) {
                glEnd();
                began = -1;
            }*/
            //optimizeLines(buffer, ldraw_basecolor);
            //buffer.clear();

            glPushMatrix();
            glMultMatrix(pe->matrix());

            render_lines_visit(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());

            glPopMatrix();
            break;
        }
        default: {
            break;
        }
        }
    }
/*    if (began >= 0)
        glEnd();*/
    optimizeLines(buffer, ldraw_basecolor);
}


