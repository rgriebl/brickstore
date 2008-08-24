
#include <QMouseEvent>

#include <cfloat>
#include <cmath>

#include "ldraw.h"
#include "renderwidget.h"
#include "matrix_t.h"


#define USE_CALL_LIST 1

LDraw::RenderWidget::RenderWidget(QWidget *parent)
    : QGLWidget(parent), m_part(0), m_color(-1), m_dl_part(0), m_initialized(false), m_zoom(1)
{
    memset(&m_rx, 0, sizeof(qreal) * 3);
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
    
    if (m_initialized)
        create_display_list();
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
        updateGL();
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

    qglClearColor(Qt::white);
    
    glMatrixMode(GL_MODELVIEW);
    create_display_list();
    
    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    m_initialized = true;
}

void LDraw::RenderWidget::resizeGL(int w, int h)
{

    int side = qMin(w, h);
    glViewport((w - side) / 2, (h - side) / 2, side, side);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    qreal r = 1.0;
    
    if (m_part) {
        vector_t vmin, vmax;
        
        if (m_part->boundingBox(vmin, vmax)) {
            qWarning("resizeGL - ortho: [%.f ..  %.f] x [%.f .. %.f] x [%.f .. %.f]", vmin[0], vmax[0], vmin[1], vmax[1], vmin[2], vmax[2]);
            
            r = qMax(vmin.length(), vmax.length());
        }
    }
    
    glOrtho(-r, r, -r, r, -r, r);
    glMatrixMode(GL_MODELVIEW);
    glLineWidth(1.5);
//    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
//    glEnable(GL_LINE_SMOOTH);
}

void LDraw::RenderWidget::paintGL()
{
    //qWarning("paintGL - rotate: %.f / %.f / %.f", m_rx, m_ry, m_rz);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslated(0.0, 0.0, 0.0);
    glScaled(m_zoom, m_zoom, m_zoom);
    glRotated(m_rx, 1.0, 0.0, 0.0);
    glRotated(m_ry, 0.0, 1.0, 0.0);
    glRotated(m_rz, 0.0, 0.0, 1.0);

#if USE_CALL_LIST
    glCallList(m_dl_part);
//    render_dynamic_visit(m_part, m_color);  
#else
    render_complete_visit(m_part, m_color);
#endif
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
            render_static_visit(m_part, m_color);
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

void LDraw::RenderWidget::render_complete_visit(Part *part, int ldraw_basecolor)
{
    render_static_visit(part, ldraw_basecolor);
    render_dynamic_visit(part, ldraw_basecolor);
}

void LDraw::RenderWidget::render_dynamic_visit(Part *part, int ldraw_basecolor)
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
            
            render_dynamic_visit(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());
            
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

void LDraw::RenderWidget::render_static_visit(Part *part, int ldraw_basecolor)
{
    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;
    
    int began = -1;    

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;
        
        switch (e->type()) {
        case Element::Line: {
            const LineElement *le = static_cast<const LineElement *>(e);
            const vector_t *v = le->points();

            qglColor(core()->color(le->color(), ldraw_basecolor));
            if (began != GL_LINES) {  
                if (began >= 0)
                    glEnd();
                glBegin(GL_LINES);
                began = GL_LINES;
            }
            glVertex(v[0]);
            glVertex(v[1]);
            break;
        }
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
            
            render_static_visit(pe->part(), pe->color() == 16 ? ldraw_basecolor : pe->color());
            
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


