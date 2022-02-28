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

#include <QOpenGLShaderProgram>
#include <QTimer>

#include <cfloat>
#include <cmath>

#include "library.h"
#include "part.h"
#include "glrenderer.h"


namespace LDraw {

GLRenderer::GLRenderer(QObject *parent)
    : QObject(parent)
{
    m_coreProfile = (QSurfaceFormat::defaultFormat().profile() == QSurfaceFormat::CoreProfile);

    m_animation = new QTimer(this);
    m_animation->setInterval(1000/60);

    connect(m_animation, &QTimer::timeout, this, &GLRenderer::animationStep);

    for (int i = 0; i < VBO_Count; ++i) {
        m_vbos[i] = new QOpenGLBuffer((i != VBO_TransparentIndexes) ? QOpenGLBuffer::VertexBuffer
                                                                    : QOpenGLBuffer::IndexBuffer);
    }
}

GLRenderer::~GLRenderer()
{
    setPartAndColor(nullptr, -1);
}

void GLRenderer::adjustSurfaceFormat(QSurfaceFormat &fmt)
{
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
}

void GLRenderer::cleanup()
{
    emit makeCurrent();
    for (int i = 0; i < VBO_Count; ++i) {
        m_vbos[i]->destroy();
        delete m_vbos[i];
    }
    delete m_surfaceShader;
    delete m_lineShader;
    delete m_conditionalShader;
    m_surfaceShader = nullptr;
    m_lineShader = nullptr;
    m_conditionalShader = nullptr;
    emit doneCurrent();
}

void GLRenderer::setPartAndColor(Part *part, const QColor &color)
{
    if ((m_part == part) && (m_baseColor == color))
        return;

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

void GLRenderer::setPartAndColor(Part *part, int basecolor)
{
    if ((m_part == part) && (m_color == basecolor))
        return;

    if (m_part)
        m_part->release();
    m_part = part;
    if (m_part)
        m_part->addRef();
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

Part *GLRenderer::part() const
{
    return m_part;
}

int GLRenderer::color() const
{
    return m_color;
}


void GLRenderer::setTranslation(const QVector3D &translation)
{
    if (m_translation != translation) {
        m_translation = translation;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void GLRenderer::setRotation(const QQuaternion &rotation)
{
    if (m_rotation != rotation) {
        m_rotation = rotation;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void GLRenderer::setZoom(float z)
{
    if (!qFuzzyCompare(m_zoom, z)) {
        m_zoom = z;
        updateWorldMatrix();
        emit updateNeeded();
    }
}

void GLRenderer::updateProjectionMatrix()
{
    int w = m_viewport.width();
    int h = m_viewport.height();

    float ax = (h < w && h) ? float(w) / float(h) : 1;
    float ay = (w < h && w) ? float(h) / float(w) : 1;

    m_proj.setToIdentity();
    m_proj.ortho((m_center.x() - m_radius) * ax, (m_center.x() + m_radius) * ax,
                 (m_center.y() - m_radius) * ay, (m_center.y() + m_radius) * ay,
                 -m_center.z() - 10 * m_radius, -m_center.z() + 5 * m_radius);
}

void GLRenderer::updateWorldMatrix()
{
    m_model.setToIdentity();
    m_model.translate(m_translation);
    m_model.translate(m_center);
    m_model.rotate(m_rotation);
    m_model.translate(-m_center);
    m_model.scale(m_zoom);

    m_resortTransparentSurfaces = true;
}

void GLRenderer::initializeGL(QOpenGLContext *context)
{
    connect(context, &QOpenGLContext::aboutToBeDestroyed, this, &GLRenderer::cleanup);

    initializeOpenGLFunctions();
    glClearColor(.5, .5, .5, 0);

    m_surfaceShader = new QOpenGLShaderProgram;
    m_surfaceShader->addShaderFromSourceFile(QOpenGLShader::Vertex, m_coreProfile
                                             ? QLatin1String(":/ldraw/shaders/surface_core.vert")
                                             : QLatin1String(":/ldraw/shaders/surface.vert"));
    m_surfaceShader->addShaderFromSourceFile(QOpenGLShader::Fragment, m_coreProfile
                                             ? QLatin1String(":/ldraw/shaders/phong_core.frag")
                                             : QLatin1String(":/ldraw/shaders/phong.frag"));
    m_surfaceShader->bindAttributeLocation("vertex", 0);
    m_surfaceShader->bindAttributeLocation("normal", 1);
    m_surfaceShader->bindAttributeLocation("color", 2);
    m_surfaceShader->link();

    m_lineShader = new QOpenGLShaderProgram;
    m_lineShader->addShaderFromSourceFile(QOpenGLShader::Vertex, m_coreProfile
                                          ? QLatin1String(":/ldraw/shaders/line_core.vert")
                                          : QLatin1String(":/ldraw/shaders/line.vert"));
    m_lineShader->addShaderFromSourceFile(QOpenGLShader::Fragment, m_coreProfile
                                          ? QLatin1String(":/ldraw/shaders/line_core.frag")
                                          : QLatin1String(":/ldraw/shaders/line.frag"));
    m_lineShader->bindAttributeLocation("vertex", 0);
    m_lineShader->bindAttributeLocation("color", 1);
    m_lineShader->link();

    m_conditionalShader = new QOpenGLShaderProgram;
    m_conditionalShader->addShaderFromSourceFile(QOpenGLShader::Vertex, m_coreProfile
                                                 ? QLatin1String(":/ldraw/shaders/conditional_core.vert")
                                                 : QLatin1String(":/ldraw/shaders/conditional.vert"));
    m_conditionalShader->addShaderFromSourceFile(QOpenGLShader::Fragment, m_coreProfile
                                                 ? QLatin1String(":/ldraw/shaders/line_core.frag")
                                                 : QLatin1String(":/ldraw/shaders/line.frag"));
    m_conditionalShader->bindAttributeLocation("vertex0", 0);
    m_conditionalShader->bindAttributeLocation("vertex1", 1);
    m_conditionalShader->bindAttributeLocation("vertex2", 2);
    m_conditionalShader->bindAttributeLocation("vertex3", 3);
    m_conditionalShader->bindAttributeLocation("color", 4);
    m_conditionalShader->link();

    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    for (int i = 0; i < VBO_Count; ++i)
        m_vbos[i]->create();

    if (m_part && m_dirty)
        recreateVBOs();

    // Fixed camera / viewMatrix
    QVector3D cameraPos = { 0, 0, -1 };
    m_view.setToIdentity();
    m_view.translate(cameraPos);

    // Fixed light position (very far away, because we can scale the model up quite a bit)
    QVector3D lightPos = { 0, 0, 1 };

    m_surfaceShader->bind();
    m_surfaceShader->setUniformValue("cameraPos", cameraPos);
    m_surfaceShader->setUniformValue("lightPos", lightPos * 10000);
    m_surfaceShader->setUniformValue("viewMatrix", m_view);
    m_surfaceShader->release();

    m_lineShader->bind();
    m_lineShader->setUniformValue("viewMatrix", m_view);
    m_lineShader->release();

    m_conditionalShader->bind();
    m_conditionalShader->setUniformValue("viewMatrix", m_view);
    m_conditionalShader->release();
}

void GLRenderer::resizeGL(QOpenGLContext *context, int w, int h)
{
    Q_UNUSED(context)

    m_viewport.setRect(0, 0, w, h);
    glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());

    updateProjectionMatrix();
    updateWorldMatrix();
}

void GLRenderer::paintGL(QOpenGLContext *context)
{
    Q_UNUSED(context)

    if (m_clearColor.isValid()) {
        glClearColor(float(m_clearColor.redF()), float(m_clearColor.greenF()),
                     float(m_clearColor.blueF()), float(m_clearColor.alphaF()));
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

#if !defined(GL_MULTISAMPLE) && defined(GL_MULTISAMPLE_EXT) // ES vs Desktop
#  define GL_MULTISAMPLE GL_MULTISAMPLE_EXT
#endif
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    if (m_dirty) {
        recreateVBOs();
        m_resortTransparentSurfaces = true;
    }

    static auto vertexOffset = [](int offset) {
        return reinterpret_cast<void *>(offset * int(sizeof(GLfloat)));
    };

    auto renderSurfacesVBO = [this](int index, GLenum mode, int indexBufferIndex = -1) {
        m_vbos[index]->bind();

        // Vertex: vec3
        // Normal: vec3
        // Color : rgba

        const int stride = 7 * sizeof(GLfloat);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, vertexOffset(6));

        if (indexBufferIndex == -1) {
            glDrawArrays(mode, 0, m_vbos[index]->size() / stride);
        } else {
            m_vbos[indexBufferIndex]->bind();
            glDrawElements(mode, m_vbos[indexBufferIndex]->size() / int(sizeof(GLuint)), GL_UNSIGNED_INT, nullptr);
            m_vbos[indexBufferIndex]->release();
        }

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        m_vbos[index]->release();
    };

    auto renderLinesVBO = [this](int index, GLenum mode, bool conditional) {
        m_vbos[index]->bind();

        // Vertex: vec3 (p1)
        // Vertex: vec3 (p2)
        // Vertex: vec3 (p3)
        // Vertex: vec3 (p4)
        // Color : rgba

        // or

        // Vertex: vec
        // Color : rgba

        const int stride = (conditional ? 13 : 4) * sizeof(GLfloat);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(0));
        if (conditional) {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(3));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(6));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, vertexOffset(9));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, vertexOffset(12));
        } else {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, vertexOffset(3));
        }

        glDrawArrays(mode, 0, m_vbos[index]->size() / stride);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        if (conditional) {
            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(3);
            glDisableVertexAttribArray(4);
        }
        m_vbos[index]->release();
    };

    // #### SURFACES ####

    m_surfaceShader->bind();
    m_surfaceShader->setUniformValue("projMatrix", m_proj);
    m_surfaceShader->setUniformValue("modelMatrix", m_model);
    m_surfaceShader->setUniformValue("normalMatrix", m_model.normalMatrix());

    // we need to offset the surfaces in the depth buffer, so that the lines are always
    // rendered on top, even though the technically have the same Z coordinate
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);

    // backface culling on the solid surfaces
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    renderSurfacesVBO(VBO_Surfaces, GL_TRIANGLES);

    m_surfaceShader->release();

    // #### LINES ####

    m_lineShader->bind();
    m_lineShader->setUniformValue("projMatrix", m_proj);
    m_lineShader->setUniformValue("modelMatrix", m_model);

    // no culling on the transparent surfaces and the lines
    glDisable(GL_CULL_FACE);

    // no more writing to the depth buffer for the transparent surfaces and lines
    glDepthMask(GL_FALSE);

    // no offset for the lines
    glDisable(GL_POLYGON_OFFSET_FILL);

    glLineWidth(2.5);

    renderLinesVBO(VBO_Lines, GL_LINES, false /*not conditional*/);

    m_lineShader->release();

    // #### CONDITIONAL LINES ####

    m_conditionalShader->bind();
    m_conditionalShader->setUniformValue("projMatrix", m_proj);
    m_conditionalShader->setUniformValue("modelMatrix", m_model);

    renderLinesVBO(VBO_ConditionalLines, GL_LINES, true /*conditional*/);

    m_conditionalShader->release();

    // #### TRANSPARENT SURFACES ####

    if (!m_transparentCenters.empty()) {
        // render transparent surfaces back to front

        if (m_resortTransparentSurfaces) {
            auto transformed = m_transparentCenters;
            auto m = m_proj * m_view * m_model;
            std::for_each(transformed.begin(), transformed.end(), [&m](QVector3D &v) {
                v = m.map(v);
            });

            struct triple_uint_t {
                GLuint i1 = 0, i2 = 1, i3 = 2;
                triple_uint_t &operator++() { i1+=3; i2+=3; i3+=3; return *this; }
            };

            std::vector<triple_uint_t> indexes(transformed.size());
            std::iota(indexes.begin(), indexes.end(), triple_uint_t { });
            std::sort(indexes.begin(), indexes.end(), [&transformed](const auto &a, const auto &b) {
                return (transformed.at(a.i1 / 3)).z() > (transformed.at(b.i1 / 3)).z();
            });
            m_vbos[VBO_TransparentIndexes]->bind();
            m_vbos[VBO_TransparentIndexes]->allocate(indexes.data(), int(indexes.size() * 3 * sizeof(GLuint)));
            m_vbos[VBO_TransparentIndexes]->release();
            m_resortTransparentSurfaces = false;
        }

        m_surfaceShader->bind();
        glEnable(GL_POLYGON_OFFSET_FILL);

        renderSurfacesVBO(VBO_TransparentSurfaces, GL_TRIANGLES, VBO_TransparentIndexes);

        glDisable(GL_POLYGON_OFFSET_FILL);
        m_surfaceShader->release();
    }
    glDepthMask(GL_TRUE);
}

void GLRenderer::resetTransformation()
{
    // this is the approximate angle from which most of the BL 2D images have been rendered

    QMatrix4x4 rotation;
    rotation.rotate(-145, 1, 0, 0);
    rotation.rotate( -37, 0, 1, 0);
    rotation.rotate(   0, 0, 0, 1);

    setTranslation({ 0, 0, 0 });
    setRotation(QQuaternion::fromRotationMatrix(rotation.toGenericMatrix<3, 3>()));
    setZoom(1);
}

void GLRenderer::handleMouseEvent(QMouseEvent *e)
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

void GLRenderer::handleWheelEvent(QWheelEvent *e)
{
    float d = 1.0f + (float(e->angleDelta().y()) / 1200.0f);
    setZoom(zoom() * d);
}


void GLRenderer::rotateArcBall()
{
    // map the mouse coordiantes to the sphere describing this arcball
    auto mapMouseToBall = [this](const QPoint &mouse) -> QVector3D {
        // normalize mouse pos to -1..+1 and reverse y
        QVector3D mouseView(
                    2.f * float(mouse.x()) / float(m_viewport.width()) - 1.f,
                    1.f - 2.f * float(mouse.y()) / float(m_viewport.height()),
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

void GLRenderer::recreateVBOs()
{
    std::vector<GLfloat> buffer[VBO_Count];
    std::vector<GLfloat> *buffer_ptr[VBO_Count];
    for (int i = 0; i < VBO_Count; ++i)
        buffer_ptr[i] = &buffer[i];
    m_transparentCenters.clear();

    fillVBOs(m_part, m_color, QMatrix4x4(), false, buffer_ptr);

    for (int i = 0; i < VBO_Count; ++i) {
        m_vbos[i]->bind();
        m_vbos[i]->allocate(buffer[i].data(), int(buffer[i].size() * sizeof(GLfloat)));
        m_vbos[i]->release();
    }
    m_dirty = false;
}

void GLRenderer::fillVBOs(Part *part, int ldrawBaseColor, const QMatrix4x4 &matrix,
                          bool inverted, std::vector<float> *buffers[])
{
    if (!part)
        return;

    bool invertNext = false;
    bool ccw = true;

    // 1 element == vec3 vector + vec3 normal + vec4 color

    static auto convertColor = [](const QColor &color) -> float {
        quint32 argb = color.rgba();
        quint32 abgr = (argb & 0xff00ff00)
                | ((argb & 0x00ff0000) >> 16)
                | ((argb & 0x000000ff) << 16);
        return *(reinterpret_cast<float *>(&abgr));
    };

    auto addTriangle = [this, &buffers](const QVector3D &p0, const QVector3D &p1,
            const QVector3D &p2, const QVector3D &pc, const QColor &color, const QMatrix4x4 &matrix)
    {
        auto p0m = matrix.map(p0);
        auto p1m = matrix.map(p1);
        auto p2m = matrix.map(p2);
        auto pcm = matrix.map(pc);

        auto n = QVector3D::normal(p0m, p1m, p2m);

        bool isTransparent = (color.alpha() < 255);
        int index = isTransparent ? VBO_TransparentSurfaces : VBO_Surfaces;

        for (const auto &p : { p0m, p1m, p2m }) {
            buffers[index]->insert(buffers[index]->end(), {
                                       p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), convertColor(color)
                                   });
        }
        if (isTransparent)
            m_transparentCenters.push_back(pcm);
    };

    auto addLine = [&buffers](const QVector3D &p0, const QVector3D &p1,
            const QColor &color, const QMatrix4x4 &matrix)
    {
        for (const auto &p : { p0, p1 }) {
            auto mp = matrix.map(p);
            buffers[VBO_Lines]->insert(buffers[VBO_Lines]->end(), {
                                       mp.x(), mp.y(), mp.z(), convertColor(color)
                                   });
        }
    };

    auto addConditionalLine = [&buffers](const QVector3D &p0, const QVector3D &p1,
            const QVector3D &p2, const QVector3D &p3, const QColor &color, const QMatrix4x4 &matrix)
    {
        auto mp0 = matrix.map(p0);
        auto mp1 = matrix.map(p1);
        auto mp2 = matrix.map(p2);
        auto mp3 = matrix.map(p3);
        buffers[VBO_ConditionalLines]->insert(buffers[VBO_ConditionalLines]->end(), {
                                                  mp0.x(), mp0.y(), mp0.z(),
                                                  mp1.x(), mp1.y(), mp1.z(),
                                                  mp2.x(), mp2.y(), mp2.z(),
                                                  mp3.x(), mp3.y(), mp3.z(),
                                                  convertColor(color)
                                              });
        buffers[VBO_ConditionalLines]->insert(buffers[VBO_ConditionalLines]->end(), {
                                                  mp1.x(), mp1.y(), mp1.z(),
                                                  mp0.x(), mp0.y(), mp0.z(),
                                                  mp2.x(), mp2.y(), mp2.z(),
                                                  mp3.x(), mp3.y(), mp3.z(),
                                                  convertColor(color)
                                              });
    };

    auto color = [this](int ldrawColor, int baseColor) -> QColor
    {
        if ((baseColor == -1) && (ldrawColor == 16))
            return m_baseColor;
        else if ((baseColor == -1) && (ldrawColor == 24))
            return m_edgeColor;
        else
            return library()->color(ldrawColor, baseColor);
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
        case Element::Type::BfcCommand: {
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
        case Element::Type::Triangle: {
            const auto *te = static_cast<const TriangleElement *>(e);
            const QColor col = color(te->color(), ldrawBaseColor);
            const auto p = te->points();
            addTriangle(p[0], ccw ? p[2] : p[1], ccw ? p[1] : p[2], (p[0] + p[1] + p[2]) / 3, col, matrix);
            break;
        }
        case Element::Type::Quad: {
            const auto *qe = static_cast<const QuadElement *>(e);
            const QColor col = color(qe->color(), ldrawBaseColor);
            const auto p = qe->points();
            const auto pc = (p[0] + p[1] + p[2] + p[3]) / 4;
            addTriangle(p[0], ccw ? p[3] : p[1], p[2], pc, col, matrix);
            addTriangle(p[2], ccw ? p[1] : p[3], p[0], pc, col, matrix);
            break;
        }
        case Element::Type::Line: {
            const auto *le = static_cast<const LineElement *>(e);
            const QColor col = color(le->color(), ldrawBaseColor);
            const auto p = le->points();
            addLine(p[0], p[1], col, matrix);
            break;
        }
        case Element::Type::CondLine: {
            const auto *cle = static_cast<const CondLineElement *>(e);
            const QColor col = color(cle->color(), ldrawBaseColor);
            const QVector3D *p = cle->points();
            addConditionalLine(p[0], p[1], p[2], p[3], col, matrix);
            break;
        }
        case Element::Type::Part: {
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

void GLRenderer::setClearColor(const QColor &color)
{
    m_clearColor = color;
}

void GLRenderer::startAnimation()
{
    if (!m_animation->isActive())
        m_animation->start();
}

void GLRenderer::stopAnimation()
{
    if (m_animation->isActive())
        m_animation->stop();
}

bool GLRenderer::isAnimationActive() const
{
    return m_animation->isActive();
}

void GLRenderer::animationStep()
{
    auto q = rotation();
    QQuaternion r(180, 0.5, 0.375, 0.25);
    setRotation(r.normalized() * q);
}

} // namespace LDraw

#endif // !QT_NO_OPENGL

#include "moc_glrenderer.cpp"
