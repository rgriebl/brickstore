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
#include <QQuaternion>

#include <cfloat>
#include <cmath>

#include "ldraw.h"
#include "renderwidget.h"


LDraw::GLRenderer::GLRenderer(QObject *parent)
    : QObject(parent)
{
    m_coreProfile = (QSurfaceFormat::defaultFormat().profile() == QSurfaceFormat::CoreProfile);

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
    delete m_standardShader;
    delete m_conditionalShader;
    m_standardShader = nullptr;
    m_conditionalShader = nullptr;
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
    m_dirty = true;

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


void LDraw::GLRenderer::setTranslation(const QVector3D &translation)
{
    if (m_translation != translation) {
        m_translation = translation;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void LDraw::GLRenderer::setRotation(const QQuaternion &rotation)
{
    if (m_rotation != rotation) {
        m_rotation = rotation;
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
    m_model.translate(m_translation);
    m_model.translate(m_center);
    m_model.rotate(m_rotation);
    m_model.translate(-m_center);
    m_model.scale(m_zoom);
}

void LDraw::GLRenderer::initializeGL(QOpenGLContext *context)
{
    connect(context, &QOpenGLContext::aboutToBeDestroyed, this, &GLRenderer::cleanup);

    initializeOpenGLFunctions();
    glClearColor(.5, .5, .5, 0);

    m_standardShader = new QOpenGLShaderProgram;
    m_standardShader->addShaderFromSourceFile(QOpenGLShader::Vertex, m_coreProfile
                                       ? QLatin1String(":/ldraw/shaders/phong_core.vert")
                                       : QLatin1String(":/ldraw/shaders/phong.vert"));
    m_standardShader->addShaderFromSourceFile(QOpenGLShader::Fragment, m_coreProfile
                                       ? QLatin1String(":/ldraw/shaders/phong_core.frag")
                                       : QLatin1String(":/ldraw/shaders/phong.frag"));
    m_standardShader->bindAttributeLocation("vertex", 0);
    m_standardShader->bindAttributeLocation("normal", 1);
    m_standardShader->bindAttributeLocation("color", 2);
    m_standardShader->link();

    m_conditionalShader = new QOpenGLShaderProgram;
    m_conditionalShader->addShaderFromSourceFile(QOpenGLShader::Vertex, m_coreProfile
                                       ? QLatin1String(":/ldraw/shaders/conditional_core.vert")
                                       : QLatin1String(":/ldraw/shaders/conditional.vert"));
    m_conditionalShader->addShaderFromSourceFile(QOpenGLShader::Fragment, m_coreProfile
                                       ? QLatin1String(":/ldraw/shaders/phong_core.frag")
                                       : QLatin1String(":/ldraw/shaders/phong.frag"));
    m_conditionalShader->bindAttributeLocation("vertex0", 0);
    m_conditionalShader->bindAttributeLocation("vertex1", 1);
    m_conditionalShader->bindAttributeLocation("vertex2", 2);
    m_conditionalShader->bindAttributeLocation("vertex3", 3);
    m_conditionalShader->bindAttributeLocation("color", 4);
    m_conditionalShader->link();

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

    // Fixed light position (very far away, because we can scale the model up quite a bit)
    QVector3D lightPos = { -1, .2f, 1 };

    m_standardShader->bind();
    m_standardShader->setUniformValue("cameraPos", cameraPos);
    m_standardShader->setUniformValue("lightPos", lightPos * 10000);
    m_standardShader->setUniformValue("viewMatrix", m_view);
    m_standardShader->release();

    m_conditionalShader->bind();
    m_conditionalShader->setUniformValue("cameraPos", cameraPos);
    m_conditionalShader->setUniformValue("lightPos", lightPos * 10000);
    m_conditionalShader->setUniformValue("viewMatrix", m_view);
    m_conditionalShader->release();
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

    // we need to offset the surfaces in the depth buffer, so that the lines are always
    // rendered on top, even though the technically have the same Z coordinate
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);

    if (m_dirty)
        recreateVBOs();

    static auto vertexOffset = [](int offset) {
        return reinterpret_cast<void *>(offset * sizeof(GLfloat));
    };

    auto renderStandardVBO = [this](int index, GLenum mode) {
        m_vbos[index].bind();

        // Vertex: vec3
        // Normal: vec3
        // Color : rgba

        const int stride = 10 * sizeof(GLfloat);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, vertexOffset(6));

        glDrawArrays(mode, 0, m_vbos[index].size() / stride);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        m_vbos[index].release();
    };

    auto renderConditionalVBO = [this](int index, GLenum mode) {
        m_vbos[index].bind();

        // Vertex: vec3 (p1)
        // Vertex: vec3 (p2)
        // Vertex: vec3 (p3)
        // Vertex: vec3 (p4)
        // Color : rgba

        const int stride = 16 * sizeof(GLfloat);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(6));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(9));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, vertexOffset(12));

        glDrawArrays(mode, 0, m_vbos[index].size() / stride);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
        glDisableVertexAttribArray(4);
        m_vbos[index].release();
    };

    m_standardShader->bind();
    m_standardShader->setUniformValue("projMatrix", m_proj);
    m_standardShader->setUniformValue("modelMatrix", m_model);
    m_standardShader->setUniformValue("normalMatrix", m_model.normalMatrix());

    renderStandardVBO(VBO_Surfaces, GL_TRIANGLES);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_FALSE);

    glLineWidth(2.5); // not supported on VMware's Linux driver

    renderStandardVBO(VBO_Lines, GL_LINES);

    m_standardShader->release();

    m_conditionalShader->bind();
    m_conditionalShader->setUniformValue("projMatrix", m_proj);
    m_conditionalShader->setUniformValue("modelMatrix", m_model);
    m_conditionalShader->setUniformValue("normalMatrix", m_model.normalMatrix());

    renderConditionalVBO(VBO_ConditionalLines, GL_LINES);

    m_conditionalShader->release();

    glDepthMask(GL_TRUE);

}

void LDraw::GLRenderer::resetTransformation()
{
    QMatrix4x4 rotation;
    rotation.rotate(-180+30, 1, 0, 0);
    rotation.rotate(-45, 0, 1, 0);
    rotation.rotate(0, 0, 0, 1);

    setTranslation({ 0, 0, 0 });
    setRotation(QQuaternion::fromRotationMatrix(rotation.toGenericMatrix<3, 3>()));
    setZoom(1);
}

void LDraw::GLRenderer::handleMouseEvent(QMouseEvent *e)
{
    switch (e->type()) {
    case QEvent::MouseButtonPress:
        stopAnimation();

        m_arcBallPressRotation = m_rotation;
        m_arcBallMousePos = m_arcBallPressPos = e->pos();
        m_arcBallActive = true;
        break;

    case QEvent::MouseMove:
        if (m_arcBallActive) {
            m_arcBallMousePos = e->pos();
            rotateArcBall();
        }
        break;

    case QEvent::MouseButtonRelease:
        m_arcBallActive = false;
        break;

    default: break;
    }
}

void LDraw::GLRenderer::handleWheelEvent(QWheelEvent *e)
{
    float d = 1.0f + (float(e->angleDelta().y()) / 1200.0f);
    setZoom(zoom() * d);
}


void LDraw::GLRenderer::rotateArcBall()
{
    // map the mouse coordiantes to the sphere describing this arcball
    auto mapMouseToBall = [this](const QPoint &mouse) -> QVector3D {
        // normalize mouse pos to -1..+1 and reverse y
        QVector3D mouseView(
                    2.f * mouse.x() / m_viewport.width() - 1.f,
                    1.f - 2.f * mouse.y() / m_viewport.height(),
                    0.f
        );

        QVector3D mapped = mouseView; // (mouseView - m_center)* (1.f / m_radius);
        auto l2 = mapped.lengthSquared();
        if (l2 > 1.f) {
            mapped.normalize();
            mapped[2] = 0.f;
        } else {
            mapped[2] = sqrt(1.f - l2);
        }
        return mapped;
    };

    // re-calculate the rotation given the current mouse position
    auto from = mapMouseToBall(m_arcBallPressPos);
    auto to = mapMouseToBall(m_arcBallMousePos);

    // given these two vectors on the arcball sphere, calculate the quaternion for the arc between
    // this is the correct rotation, but it follows too slow...
    //auto q = QQuaternion::rotationTo(from, to);

    // this one seems to work far better
    auto q = QQuaternion(QVector3D::dotProduct(from, to), QVector3D::crossProduct(from, to));

    m_rotation = q * m_arcBallPressRotation;

    updateWorldMatrix();
    emit updateNeeded();
}

void LDraw::GLRenderer::recreateVBOs()
{
    std::vector<GLfloat> buffer[VBO_Count];
    std::vector<GLfloat> *buffer_ptr[VBO_Count];
    for (int i = 0; i < VBO_Count; ++i)
        buffer_ptr[i] = &buffer[i];

    fillVBOs(m_part, m_color, QMatrix4x4(), false, buffer_ptr);

    for (int i = 0; i < VBO_Count; ++i) {
        m_vbos[i].bind();
        m_vbos[i].allocate(buffer[i].data(), int(buffer[i].size() * sizeof(GLfloat)));
        m_vbos[i].release();
    }
    m_dirty = false;
}

void LDraw::GLRenderer::fillVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix,
                                   bool inverted, std::vector<float> *buffers[])
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

    auto addLine = [&buffers](const QVector3D &p0, const QVector3D &p1,
            const QColor &color, const QMatrix4x4 &matrix)
    {
        float r = color.redF();
        float g = color.greenF();
        float b = color.blueF();
        float a = color.alphaF();

        for (const auto &p : { p0, p1 }) {
            auto mp = matrix.map(p);
            buffers[VBO_Lines]->insert(buffers[VBO_Lines]->end(), {
                                       mp.x(), mp.y(), mp.z(), 0, 0, 0, r, g, b, a
                                   });
        }
    };

    auto addConditionalLine = [&buffers](const QVector3D &p0, const QVector3D &p1,
            const QVector3D &p2, const QVector3D &p3, const QColor &color, const QMatrix4x4 &matrix)
    {
        float r = color.redF();
        float g = color.greenF();
        float b = color.blueF();
        float a = color.alphaF();

        auto mp0 = matrix.map(p0);
        auto mp1 = matrix.map(p1);
        auto mp2 = matrix.map(p2);
        auto mp3 = matrix.map(p3);
        buffers[VBO_ConditionalLines]->insert(buffers[VBO_ConditionalLines]->end(), {
                                                  mp0.x(), mp0.y(), mp0.z(),
                                                  mp1.x(), mp1.y(), mp1.z(),
                                                  mp2.x(), mp2.y(), mp2.z(),
                                                  mp3.x(), mp3.y(), mp3.z(),
                                                  r, g, b, a
                                              });
        buffers[VBO_ConditionalLines]->insert(buffers[VBO_ConditionalLines]->end(), {
                                                  mp1.x(), mp1.y(), mp1.z(),
                                                  mp0.x(), mp0.y(), mp0.z(),
                                                  mp2.x(), mp2.y(), mp2.z(),
                                                  mp3.x(), mp3.y(), mp3.z(),
                                                  r, g, b, a
                                              });
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

//    bool gotFirstNonComment = false;

    for (int i = 0; i <= lastind; ++i, ++ep) {
        const Element *e = *ep;

//        if (e->type() != Element::Comment)
//            gotFirstNonComment = true;

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
            const auto *te = static_cast<const TriangleElement *>(e);
            const QColor col = color(te->color(), ldrawBaseColor);
            const auto p = te->points();
            addTriangle(p[0], ccw ? p[2] : p[1], ccw ? p[1] : p[2], col, matrix);
            break;
        }
        case Element::Quad: {
            const auto *qe = static_cast<const QuadElement *>(e);
            const QColor col = color(qe->color(), ldrawBaseColor);
            const auto p = qe->points();
            addTriangle(p[0], ccw ? p[3] : p[1], p[2], col, matrix);
            addTriangle(p[2], ccw ? p[1] : p[3], p[0], col, matrix);
            break;
        }
        case Element::Line: {
            const auto *le = static_cast<const LineElement *>(e);
            const QColor col = color(le->color(), ldrawBaseColor);
            const auto p = le->points();
            addLine(p[0], p[1], col, matrix);
            break;
        }
        case Element::CondLine: {
            const auto *cle = static_cast<const CondLineElement *>(e);
            const QColor col = color(cle->color(), ldrawBaseColor);
            const QVector3D *p = cle->points();
            addConditionalLine(p[0], p[1], p[2], p[3], col, matrix);
            break;
        }
        case Element::Part: {
            const auto *pe = static_cast<const PartElement *>(e);
            bool matrixReversed = (pe->matrix().determinant() < 0);

            fillVBOs(pe->part(), pe->color() == 16 ? ldrawBaseColor : pe->color(),
                       matrix * pe->matrix(), inverted ^ invertNext ^ matrixReversed, buffers);
            break;
        }
        default:
            break;
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
    auto q = rotation();
    QQuaternion r(180, 0.5, 0.375, 0.25);
    setRotation(r.normalized() * q);
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
    setCursor(Qt::ClosedHandCursor);
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWidget::mouseReleaseEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWidget::wheelEvent(QWheelEvent *e)
{
    m_renderer->handleWheelEvent(e);
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
    m_renderer->resetTransformation();
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
    setCursor(Qt::ClosedHandCursor);
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWindow::mouseReleaseEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWindow::mouseMoveEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWindow::wheelEvent(QWheelEvent *e)
{
    m_renderer->handleWheelEvent(e);
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
            m_renderer->resetTransformation();
            e->accept();
            return true;
        } else if (nge->gestureType() == Qt::RotateNativeGesture) {
            auto q = m_renderer->rotation();
            float p, y, r;
            q.getEulerAngles(&p, &y, &r);
            r += float(nge->value());
            m_renderer->setRotation(QQuaternion::fromEulerAngles(p, y, r));
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
    m_renderer->resetTransformation();
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

