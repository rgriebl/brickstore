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
#include <qtguiglobal.h>

#if !defined(QT_NO_OPENGL)

#include <QMouseEvent>
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
    for (int i = 0; i < VBO_Count; ++i)
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        qreal h, s, v, a;
#else
        float h, s, v, a;
#endif
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
    m_dirty = (1 << VBO_Surfaces) | (1 << VBO_Lines) | (1 << VBO_ConditionalLines);

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


void LDraw::GLRenderer::setXRotation(float r)
{
    if (!qFuzzyCompare(m_rx, r)) {
        m_rx = r;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setYRotation(float r)
{
    if (!qFuzzyCompare(m_ry, r)) {
        m_ry = r;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setZRotation(float r)
{
    if (!qFuzzyCompare(m_rz, r)) {
        m_rz = r;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setXTranslation(float t)
{
    if (!qFuzzyCompare(m_tx, t)) {
        m_tx = t;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setYTranslation(float t)
{
    if (!qFuzzyCompare(m_ty, t)) {
        m_ty = t;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setZTranslation(float t)
{
    if (!qFuzzyCompare(m_tz, t)) {
        m_tz = t;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setZoom(float z)
{
    if (!qFuzzyCompare(m_zoom, z)) {
        m_zoom = z;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::updateProjectionMatrix()
{
    int w = m_viewport.width();
    int h = m_viewport.height();

    float ax = (h < w && h) ? float(w) / float(h) : 1;
    float ay = (w < h && w) ? float(h) / float(w) : 1;

    m_proj.setToIdentity();
    m_proj.ortho((m_center.x() - m_radius) * ax, (m_center.x() + m_radius) * ax,
                 (m_center.y() - m_radius) * ay, (m_center.y() + m_radius) * ay,
                 -m_center.z() - 2 * m_radius, -m_center.z() + 2 * m_radius);
}

void LDraw::GLRenderer::updateWorldMatrix()
{
    m_model.setToIdentity();
    m_model.translate(m_tx, m_ty, m_tz);
    m_model.translate(m_center.x(), m_center.y(), m_center.z());
    m_model.rotate(m_rx, 1, 0, 0);
    m_model.rotate(m_ry, 0, 1, 0);
    m_model.rotate(m_rz, 0, 0, 1);
    m_model.translate(-m_center.x(), -m_center.y(), -m_center.z());
    m_model.scale(m_zoom);
    m_dirty |= (1 << VBO_ConditionalLines);
}

void LDraw::GLRenderer::initializeGL(QOpenGLContext *context)
{
    connect(context, &QOpenGLContext::aboutToBeDestroyed, this, &GLRenderer::cleanup);

    initializeOpenGLFunctions();
    glClearColor(.5, .5, .5, 0);

    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSourcePhong20);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSourcePhong20);
    m_program->bindAttributeLocation("vertex", 0);
    m_program->bindAttributeLocation("normal", 1);
    m_program->bindAttributeLocation("color", 2);
    m_program->link();

    m_program->bind();
    m_projMatrixLoc = m_program->uniformLocation("projMatrix");
    m_modelMatrixLoc = m_program->uniformLocation("modelMatrix");
    m_viewMatrixLoc = m_program->uniformLocation("viewMatrix");
    m_normalMatrixLoc = m_program->uniformLocation("normalMatrix");
    m_lightPosLoc = m_program->uniformLocation("lightPos");
    m_cameraPosLoc = m_program->uniformLocation("cameraPos");

    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    for (int i = 0; i < VBO_Count; ++i)
        m_vbos[i].create();

    if (m_part && m_dirty)
        recreateVBOs();

    // Fixed camera / viewMatrix
    QVector3D cameraPos = { 0, 0, -1 };
    m_view.setToIdentity();
    m_view.translate(cameraPos);
    m_program->setUniformValue(m_cameraPosLoc, cameraPos);

    // Fixed light position (very far away, because we can scale the model up quite a bit)
    QVector3D lightPos = { -1, .2f, 1 };
    m_program->setUniformValue(m_lightPosLoc, lightPos * 10000);

    m_program->release();
}

void LDraw::GLRenderer::resizeGL(QOpenGLContext *context, int w, int h)
{
    Q_UNUSED(context)

    m_viewport.setRect(0, 0, w, h);
    glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());

    updateProjectionMatrix();
    updateWorldMatrix();
}

void LDraw::GLRenderer::paintGL(QOpenGLContext *context)
{
    Q_UNUSED(context)

    if (m_clearColor.isValid()) {
        glClearColor(m_clearColor.redF(), m_clearColor.greenF(),
                     m_clearColor.blueF(), m_clearColor.alphaF());
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

#if !defined(GL_MULTISAMPLE) && defined(GL_MULTISAMPLE_EXT) // ES vs Desktop
#  define GL_MULTISAMPLE GL_MULTISAMPLE_EXT
#endif
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();
    m_program->setUniformValue(m_projMatrixLoc, m_proj);
    m_program->setUniformValue(m_modelMatrixLoc, m_model);
    m_program->setUniformValue(m_viewMatrixLoc, m_view);
    QMatrix3x3 normalMatrix = m_model.normalMatrix();
    m_program->setUniformValue(m_normalMatrixLoc, normalMatrix);

    // we need to offset the surfaces in the depth buffer, so that the lines are always
    // rendered on top, even though the technically have the same Z coordinate
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);

    if (m_dirty)
        recreateVBOs();

    auto renderVBO = [this](int index, GLenum mode) {
        m_vbos[index].bind();

        glEnableVertexAttribArray(0); // vertex
        glVertexAttribPointer(0, VBO_Size_Vertex, GL_FLOAT, GL_FALSE, VBO_Stride * sizeof(GLfloat),
                              reinterpret_cast<void *>(VBO_Offset_Vertex * sizeof(GLfloat)));
        glEnableVertexAttribArray(1); // normals
        glVertexAttribPointer(1, VBO_Size_Normal, GL_FLOAT, GL_FALSE, VBO_Stride * sizeof(GLfloat),
                              reinterpret_cast<void *>(VBO_Offset_Normal * sizeof(GLfloat)));
        glEnableVertexAttribArray(2); // color
        glVertexAttribPointer(2, VBO_Size_Color, GL_FLOAT, GL_FALSE, VBO_Stride * sizeof(GLfloat),
                              reinterpret_cast<void *>(VBO_Offset_Color * sizeof(GLfloat)));

        glDrawArrays(mode, 0, m_vboSizes[index]);
        m_vbos[index].release();
    };

    renderVBO(VBO_Surfaces, GL_TRIANGLES);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_FALSE);

    glLineWidth(2.5); // not supported on VMware's Linux driver

    renderVBO(VBO_Lines, GL_LINES);
    renderVBO(VBO_ConditionalLines, GL_LINES);

    glDepthMask(GL_TRUE);

    m_program->release();
}

void LDraw::GLRenderer::recreateVBOs()
{
    std::vector<GLfloat> buffer[VBO_Count];
    std::vector<GLfloat> *buffer_ptr[VBO_Count];
    for (int i = 0; i < VBO_Count; ++i)
        buffer_ptr[i] = &buffer[i];

    renderVBOs(m_part, m_color, QMatrix4x4(), false, m_dirty, buffer_ptr);

    for (int i = 0; i < VBO_Count; ++i) {
        if (m_dirty & (1 << i)) {
            m_vboSizes[i] = int(buffer[i].size()) / VBO_Stride;

            m_vbos[i].bind();
            m_vbos[i].allocate(buffer[i].data(), int(buffer[i].size() * sizeof(GLfloat)));
            m_vbos[i].release();
        }
    }
    m_dirty = 0;
}

void LDraw::GLRenderer::renderVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix,
                                   bool inverted, int dirty, std::vector<float> *buffers[])
{
    if (!part)
        return;

    bool invertNext = false;
    bool ccw = true;

    // 1 element == vec3 vector + vec3 normal + vec4 color

    auto addTriangle = [&buffers](const QVector3D &p0, const QVector3D &p1,
            const QVector3D &p2, const QColor &color, const QMatrix4x4 &matrix)
    {
        float r = color.redF();
        float g = color.greenF();
        float b = color.blueF();
        float a = color.alphaF();

        auto p0m = matrix.map(p0);
        auto p1m = matrix.map(p1);
        auto p2m = matrix.map(p2);

        auto n = QVector3D::normal(p0m, p1m, p2m);

        for (const auto &p : { p0m, p1m, p2m }) {
            buffers[VBO_Surfaces]->insert(buffers[VBO_Surfaces]->end(), {
                                          p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), r, g, b, a
                                      });
        }
    };

    auto addLine = [&buffers](bool isConditional, const QVector3D &p0, const QVector3D &p1,
            const QColor &color, const QMatrix4x4 &matrix)
    {
        float r = color.redF();
        float g = color.greenF();
        float b = color.blueF();
        float a = color.alphaF();
        int index = isConditional ? VBO_ConditionalLines : VBO_Lines;

        for (const auto &p : { p0, p1 }) {
            auto mp = matrix.map(p);
            buffers[index]->insert(buffers[index]->end(), {
                                       mp.x(), mp.y(), mp.z(), 0, 0, 0, r, g, b, a
                                   });
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

    bool gotFirstNonComment = false;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

        if (e->type() != Element::Comment)
            gotFirstNonComment = true;

        bool isBFCCommand = false;
        bool isBFCInvertNext = false;

        switch (e->type()) {
        case Element::BfcCommand: {
            const auto *be = static_cast<const BfcCommandElement *>(e);

            if (be->invertNext()) {
                invertNext = true;
                isBFCInvertNext = true;
            }
            if (be->cw())
                ccw = inverted ? false : true;
            if (be->ccw())
                ccw = inverted ? true : false;

            isBFCCommand = true;
            break;
        }
        case Element::Triangle: {
            if (dirty & (1 << VBO_Surfaces)) {
                const auto *te = static_cast<const TriangleElement *>(e);
                const QColor col = color(te->color(), ldrawBaseColor);
                const auto p = te->points();
                addTriangle(p[0], ccw ? p[2] : p[1], ccw ? p[1] : p[2], col, matrix);
            }
            break;
        }
        case Element::Quad: {
            if (dirty & (1 << VBO_Surfaces)) {
                const auto *qe = static_cast<const QuadElement *>(e);
                const QColor col = color(qe->color(), ldrawBaseColor);
                const auto p = qe->points();
                addTriangle(p[0], ccw ? p[3] : p[1], p[2], col, matrix);
                addTriangle(p[2], ccw ? p[1] : p[3], p[0], col, matrix);
            }
            break;
        }
        case Element::Line: {
            if (dirty & (1 << VBO_Lines)) {
                const auto *le = static_cast<const LineElement *>(e);
                const QColor col = color(le->color(), ldrawBaseColor);
                const auto p = le->points();
                addLine(false, p[0], p[1], col, matrix);
            }
            break;
        }
        case Element::CondLine: {
            if (dirty & (1 << VBO_ConditionalLines)) {
                const auto *cle = static_cast<const CondLineElement *>(e);
                const QVector3D *v = cle->points();

                QVector3D pv[4];
                for (int j = 0; j < 4; j++)
                    pv[j] = matrix.map(v[j]).project(m_view * m_model, m_proj, m_viewport);

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
            bool matrixReversed = (pe->matrix().determinant() < 0);

            renderVBOs(pe->part(), pe->color() == 16 ? ldrawBaseColor : pe->color(),
                       matrix * pe->matrix(), inverted ^ invertNext ^ matrixReversed, dirty, buffers);
            break;
        }
        default: {
            break;
        }
        }

        if (!isBFCCommand || !isBFCInvertNext)
            invertNext = false;
    }
}

void LDraw::GLRenderer::setClearColor(const QColor &color)
{
    m_clearColor = color;
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
    setXRotation(xRotation() + 0.5f / 4);
    setYRotation(yRotation() + 0.375f / 4);
    setZRotation(zRotation() + 0.25f / 4);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


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

void LDraw::RenderWidget::setClearColor(const QColor &color)
{
    m_renderer->setClearColor(color);
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
    auto dx = float(e->x() - m_last_pos.x());
    auto dy = float(e->y() - m_last_pos.y());

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
        m_renderer->setZoom(m_renderer->zoom() * (dy < 0 ? 0.9f : 1.1f));
    }
    m_last_pos = e->pos();
}

void LDraw::RenderWidget::wheelEvent(QWheelEvent *e)
{
    float d = 1.0f + (float(e->angleDelta().y()) / 1200.0f);
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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


LDraw::RenderWindow::RenderWindow()
    : QOpenGLWindow()
{
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    setFormat(fmt);

    m_renderer = new GLRenderer(this);
    resetCamera();
    connect(m_renderer, &GLRenderer::makeCurrent, this, &RenderWindow::slotMakeCurrent);
    connect(m_renderer, &GLRenderer::doneCurrent, this, &RenderWindow::slotDoneCurrent);
    connect(m_renderer, &GLRenderer::updateNeeded, this, QOverload<>::of(&RenderWindow::update));

    setCursor(Qt::OpenHandCursor);
}

LDraw::RenderWindow::~RenderWindow()
{
    m_renderer->cleanup();
}

void LDraw::RenderWindow::setClearColor(const QColor &color)
{
    m_renderer->setClearColor(color);
}

void LDraw::RenderWindow::mousePressEvent(QMouseEvent *e)
{
    m_last_pos = e->pos();
    setCursor(Qt::ClosedHandCursor);
}

void LDraw::RenderWindow::mouseReleaseEvent(QMouseEvent *)
{
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWindow::mouseMoveEvent(QMouseEvent *e)
{
    auto dx = float(e->x() - m_last_pos.x());
    auto dy = float(e->y() - m_last_pos.y());

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
        m_renderer->setZoom(m_renderer->zoom() * (dy < 0 ? 0.9f : 1.1f));
    }
    m_last_pos = e->pos();
}

void LDraw::RenderWindow::wheelEvent(QWheelEvent *e)
{
    float d = 1.0f + (float(e->angleDelta().y()) / 1200.0f);
    m_renderer->setZoom(m_renderer->zoom() * d);
}

bool LDraw::RenderWindow::event(QEvent *e)
{
    if (e->type() == QEvent::NativeGesture) {
        const auto *nge = static_cast<QNativeGestureEvent *>(e);
        if (nge->gestureType() == Qt::ZoomNativeGesture) {
            float d = 1.0f + float(nge->value());
            m_renderer->setZoom(m_renderer->zoom() * d);
            e->accept();
            return true;
        } else if (nge->gestureType() == Qt::SmartZoomNativeGesture) {
            resetCamera();
            e->accept();
            return true;
        } else if (nge->gestureType() == Qt::RotateNativeGesture) {
            m_renderer->setZRotation(m_renderer->zRotation() + float(nge->value()));
            e->accept();
            return true;
        }
    }
    return QOpenGLWindow::event(e);
}

void LDraw::RenderWindow::slotMakeCurrent()
{
    makeCurrent();
}

void LDraw::RenderWindow::slotDoneCurrent()
{
    doneCurrent();
}

void LDraw::RenderWindow::initializeGL()
{
    m_renderer->initializeGL(context());
}

void LDraw::RenderWindow::resizeGL(int w, int h)
{
    m_renderer->resizeGL(context(), w, h);
}

void LDraw::RenderWindow::paintGL()
{
    m_renderer->paintGL(context());
}

void LDraw::RenderWindow::resetCamera()
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

void LDraw::RenderWindow::startAnimation()
{
    m_renderer->startAnimation();
}

void LDraw::RenderWindow::stopAnimation()
{
    m_renderer->stopAnimation();
}

#endif // !QT_NO_OPENGL

#include "moc_renderwidget.cpp"

