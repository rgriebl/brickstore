/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include <qtguiglobal.h>

#if !defined(QT_NO_OPENGL)

#include <QMouseEvent>
#include <QOpenGLWindow>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QTimer>
#include <QMatrix4x4>

#include <cfloat>
#include <cmath>

#include "ldraw.h"
#include "renderwidget.h"
#include "shaders.h"


LDraw::GLRenderer::GLRenderer(QObject *parent)
    : QObject(parent)
{
    m_animation = new QTimer(this);
    m_animation->setInterval(1000/60);

    connect(m_animation, &QTimer::timeout, this, &GLRenderer::animationStep);
}

LDraw::GLRenderer::~GLRenderer()
{
    setPartAndColor(nullptr, -1);
}

void LDraw::GLRenderer::cleanup()
{
    emit makeCurrent();
    for (int i = 0; i < Count; ++i)
        m_vbos[i].destroy();
    delete m_program;
    m_program = nullptr;
    emit doneCurrent();
}

void LDraw::GLRenderer::setPartAndColor(Part *part, const QColor &color)
{
    m_baseColor = color;

    {
        // calculate a contrasting edge color
        qreal h, s, v, a;
        color.getHsvF(&h, &s, &v, &a);

        v += qreal(0.5) * ((v < qreal(0.5)) ? qreal(1) : qreal(-1));
        v = qBound(qreal(0), v, qreal(1));
        m_edgeColor = QColor::fromHsvF(h, s, v, a);
    }

    setPartAndColor(part, -1);
}

void LDraw::GLRenderer::setPartAndColor(LDraw::Part *part, int basecolor)
{
    m_part = part;
    m_color = basecolor;
    m_dirty = (1 << Surfaces) | (1 << Lines) | (1 << ConditionalLines);

    m_proj.setToIdentity();
    m_center = { };
    m_radius = 1.0;

    if (m_part) {
        QVector3D vmin, vmax;

        if (m_part->boundingBox(vmin, vmax)) {
            m_center = (vmin + vmax) / 2;
            m_radius = (vmax - vmin).length() / 2;
        }
    }

    updateProjectionMatrix();
    updateWorldMatrix();

    emit updateNeeded();
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
    if (!qFuzzyCompare(m_rx, r)) {
        m_rx = r;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setYRotation(qreal r)
{
    if (!qFuzzyCompare(m_ry, r)) {
        m_ry = r;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setZRotation(qreal r)
{
    if (!qFuzzyCompare(m_rz, r)) {
        m_rz = r;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setXTranslation(qreal t)
{
    if (!qFuzzyCompare(m_tx, t)) {
        m_tx = t;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setYTranslation(qreal t)
{
    if (!qFuzzyCompare(m_ty, t)) {
        m_ty = t;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setZTranslation(qreal t)
{
    if (!qFuzzyCompare(m_tz, t)) {
        m_tz = t;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setZoom(qreal z)
{
    if (!qFuzzyCompare(m_zoom, z)) {
        m_zoom = z;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::initializeGL(QOpenGLContext *context)
{
    connect(context, &QOpenGLContext::aboutToBeDestroyed, this, &GLRenderer::cleanup);

    initializeOpenGLFunctions();
    glClearColor(.5, .5, .5, 0);

    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSourceSimple);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSourceSimple);
    m_program->bindAttributeLocation("vertex", 0);
    m_program->bindAttributeLocation("color", 1);
    m_program->link();

    m_program->bind();
    m_projMatrixLoc = m_program->uniformLocation("projMatrix");
    m_mvMatrixLoc = m_program->uniformLocation("mvMatrix");

    // Create a vertex array object. In OpenGL ES 2.0 and OpenGL 2.x
    // implementations this is optional and support may not be present
    // at all. Nonetheless the below code works in all cases and makes
    // sure there is a VAO when one is needed.
    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    // Setup our vertex buffer object.

    for (int i = 0; i < 3; ++i)
        m_vbos[i].create();

    if (m_part && m_dirty)
        recreateVBOs();

    // Our camera never changes in this example.
    m_camera.setToIdentity();
    m_camera.translate(0, 0, -1);

    m_program->release();
}

void LDraw::GLRenderer::recreateVBOs()
{
    if (!m_part)
        return;

    std::vector<GLfloat> buffer[Count];
    std::vector<GLfloat> *buffer_ptr[3] = { &buffer[0], &buffer[1], &buffer[2] };
    renderVBOs(m_part, m_color, QMatrix4x4(), m_dirty, buffer_ptr);

    for (int i = 0; i < Count; ++i) {
        if (m_dirty & (1 << i)) {
            m_vboSizes[i] = int(buffer[i].size()) / 7;

            m_vbos[i].bind();
            m_vbos[i].allocate(buffer[i].data(), int(buffer[i].size()) * sizeof(GLfloat));
            m_vbos[i].release();
        }
    }
    m_dirty = 0;
}

void LDraw::GLRenderer::resizeGL(QOpenGLContext *context, int w, int h)
{
    Q_UNUSED(context);

    m_viewport.setRect(0, 0, w, h);
    glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());

    updateProjectionMatrix();
    updateWorldMatrix();
}

void LDraw::GLRenderer::paintGL(QOpenGLContext *context)
{
    Q_UNUSED(context);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();
    m_program->setUniformValue(m_projMatrixLoc, m_proj);
    m_program->setUniformValue(m_mvMatrixLoc, m_camera * m_world);
    QMatrix3x3 normalMatrix = m_world.normalMatrix();
    m_program->setUniformValue(m_normalMatrixLoc, normalMatrix);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);

    if (m_dirty)
        recreateVBOs();

    auto renderVBO = [this](int index, GLenum mode) {
        m_vbos[index].bind();

        glEnableVertexAttribArray(0); // vertex
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), reinterpret_cast<void *>(0));
        glEnableVertexAttribArray(1); // color
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), reinterpret_cast<void *>(3 * sizeof(GLfloat)));

        glDrawArrays(mode, 0, m_vboSizes[index]);
        m_vbos[index].release();
    };

    renderVBO(Surfaces, GL_TRIANGLES);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_FALSE);

    glLineWidth(2.5); // not supported on VMware's Linux driver

    renderVBO(Lines, GL_LINES);
    renderVBO(ConditionalLines, GL_LINES);

    glDepthMask(GL_TRUE);

    m_program->release();
}


void LDraw::GLRenderer::renderVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix,
                                  int dirty, std::vector<float> *buffers[])
{
    // 1 element == vec3 vector + vec4 color

    auto addTriangle = [&buffers](const QVector3D &p0, const QVector3D &p1,
            const QVector3D &p2, const QColor &color, const QMatrix4x4 &matrix)
    {
        float r = color.redF();
        float g = color.greenF();
        float b = color.blueF();
        float a = color.alphaF();

        for (const auto &p : { p0, p1, p2 }) {
            auto mp = matrix.map(p);
            buffers[Surfaces]->insert(buffers[Surfaces]->end(), { mp.x(), mp.y(), mp.z(), r, g, b, a });
        }
    };

    auto addLine = [&buffers](bool isConditional, const QVector3D &p0, const QVector3D &p1,
            const QColor &color, const QMatrix4x4 &matrix)
    {
        float r = color.redF();
        float g = color.greenF();
        float b = color.blueF();
        float a = color.alphaF();
        int index = isConditional ? ConditionalLines : Lines;

        for (const auto &p : { p0, p1 }) {
            auto mp = matrix.map(p);
            buffers[index]->insert(buffers[index]->end(), { mp.x(), mp.y(), mp.z(), r, g, b, a });
        }
    };

    auto color = [this](int ldrawColor, int baseColor) -> QColor
    {
        if ((baseColor == -1) && (ldrawColor == 16))
            return m_baseColor;
        else if ((baseColor == -1) && (ldrawColor == 24))
            return m_edgeColor;
        else
            return LDraw::core()->color(ldrawColor, baseColor);
    };

    Element * const *ep = part->elements().constData();
    int lastind = part->elements().count() - 1;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        switch (e->type()) {
        case Element::Triangle: {
            if (dirty & (1 << Surfaces)) {
                const auto *te = static_cast<const TriangleElement *>(e);
                const QColor col = color(te->color(), ldrawBaseColor);
                const auto p = te->points();
                addTriangle(p[0], p[1], p[2], col, matrix);
            }
            break;
        }
        case Element::Quad: {
            if (dirty & (1 << Surfaces)) {
                const auto *qe = static_cast<const QuadElement *>(e);
                const QColor col = color(qe->color(), ldrawBaseColor);
                const auto p = qe->points();
                addTriangle(p[0], p[1], p[2], col, matrix);
                addTriangle(p[0], p[2], p[3], col, matrix);
            }
            break;
        }
        case Element::Line: {
            if (dirty & (1 << Lines)) {
                const auto *le = static_cast<const LineElement *>(e);
                const QColor col = color(le->color(), ldrawBaseColor);
                const auto p = le->points();
                addLine(false, p[0], p[1], col, matrix);
            }
            break;
        }
        case Element::CondLine: {
            if (dirty & (1 << ConditionalLines)) {
                const auto *cle = static_cast<const CondLineElement *>(e);
                const QVector3D *v = cle->points();

                QVector3D pv[4];
                for (int j = 0; j < 4; j++)
                    pv[j] = matrix.map(v[j]).project(m_camera * m_world, m_proj, m_viewport);

                QVector3D line_norm = QVector3D::crossProduct(pv[1] - pv[0], QVector3D(0, 0, -1));

                if ((QVector3D::dotProduct(line_norm, pv[0] - pv[2]) < 0)
                        == (QVector3D::dotProduct(line_norm, pv[0] - pv[3]) < 0)) {
                    const QColor col = color(cle->color(), ldrawBaseColor);
                    const auto p = cle->points();
                    addLine(true, p[0], p[1], col, matrix);
                }
            }
            break;
        }
        case Element::Part: {
            const auto *pe = static_cast<const PartElement *>(e);
            renderVBOs(pe->part(), pe->color() == 16 ? ldrawBaseColor : pe->color(),
                           matrix * pe->matrix(), dirty, buffers);
            break;
        }
        default: {
            break;
        }
        }
    }
}









LDraw::RenderWidget::RenderWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_AlwaysStackOnTop);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    fmt.setSamples(4);
    setFormat(fmt);

    m_renderer = new GLRenderer(this);
    resetCamera();
    connect(m_renderer, &GLRenderer::makeCurrent, this, &RenderWidget::slotMakeCurrent);
    connect(m_renderer, &GLRenderer::doneCurrent, this, &RenderWidget::slotDoneCurrent);
    connect(m_renderer, &GLRenderer::updateNeeded, this, QOverload<>::of(&RenderWidget::update));

    setCursor(Qt::OpenHandCursor);
}

LDraw::RenderWidget::~RenderWidget()
{
    m_renderer->cleanup();
}

QSize LDraw::RenderWidget::minimumSizeHint() const
{
    return { 50, 50 };
}

QSize LDraw::RenderWidget::sizeHint() const
{
    return { 100, 100 };
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

void LDraw::RenderWidget::slotDoneCurrent()
{
    doneCurrent();
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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


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
    setXRotation(xRotation() + 0.5 / 4);
    setYRotation(yRotation() + 0.375 / 4);
    setZRotation(zRotation() + 0.25 / 4);
}

void LDraw::GLRenderer::updateProjectionMatrix()
{
    qreal w = m_viewport.width();
    qreal h = m_viewport.height();

    qreal ax = (h < w && h) ? w / h : 1;
    qreal ay = (w < h && w) ? h / w : 1;

    m_proj.setToIdentity();
    m_proj.ortho((m_center.x() - m_radius) * ax, (m_center.x() + m_radius) * ax,
                 (m_center.y() - m_radius) * ay, (m_center.y() + m_radius) * ay,
                 -m_center.z() - 2 * m_radius, -m_center.z() + 2 * m_radius);
}

void LDraw::GLRenderer::updateWorldMatrix()
{
    m_world.setToIdentity();
    m_world.translate(m_tx, m_ty, m_tz);
    m_world.translate(m_center.x(), m_center.y(), m_center.z());
    m_world.rotate(m_rx, 1, 0, 0);
    m_world.rotate(m_ry, 0, 1, 0);
    m_world.rotate(m_rz, 0, 0, 1);
    m_world.translate(-m_center.x(), -m_center.y(), -m_center.z());
    m_world.scale(m_zoom);
    m_dirty |= (1 << ConditionalLines);
}

void LDraw::RenderWidget::resetCamera()
{
    stopAnimation();

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

#endif // !QT_NO_OPENGL

#include "moc_renderwidget.cpp"
