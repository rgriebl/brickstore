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
#include <QtGui/QMatrix4x4>

#include "library.h"
#include "part.h"
#include "rendercontroller.h"


namespace LDraw {

RenderGeometry::RenderGeometry(int colorId)
    : QQuick3DGeometry()
{
    m_color = library()->color(colorId);
}


RenderController::RenderController(QObject *parent)
    : QObject{parent}
{
    updateGeometries();
}

RenderController::~RenderController()
{
    for (auto *geo : m_geos)
        delete geo;
}

QQuaternion RenderController::standardRotation() const
{
    static QQuaternion q { 0, 0, 0, 0 };

    if (q.isNull()) {
        // this is the approximate angle from which most of the BL 2D images have been rendered
        QMatrix4x4 rotation;
        rotation.rotate(-150, 1, 0, 0);
        rotation.rotate( -37, 0, 1, 0);
        rotation.rotate(   0, 0, 0, 1);

        q = QQuaternion::fromRotationMatrix(rotation.toGenericMatrix<3, 3>());
    }
    return q;
}

QQuaternion RenderController::rotateArcBall(QPointF pressPos, QPointF mousePos,
                                            QQuaternion pressRotation, QSizeF viewportSize)
{
    // map the mouse coordiantes to the sphere describing this arcball
    auto mapMouseToBall = [=](const QPointF &mouse) -> QVector3D {
        // normalize mouse pos to -1..+1 and reverse y
        QVector3D mouseView(
                    2.f * mouse.x() / viewportSize.width() - 1.f,
                    1.f - 2.f * mouse.y() / viewportSize.height(),
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
    auto from = mapMouseToBall(pressPos);
    auto to = mapMouseToBall(mousePos);

    // given these two vectors on the arcball sphere, calculate the quaternion for the arc between
    // this is the correct rotation, but it follows too slow...
    //auto q = QQuaternion::rotationTo(from, to);

    // this one seems to work far better
    auto q = QQuaternion(QVector3D::dotProduct(from, to), QVector3D::crossProduct(from, to));
    return q * pressRotation;
}

Part *RenderController::part() const
{
    return m_part;
}

int RenderController::color() const
{
    return m_colorId;
}

QVector<RenderGeometry *> RenderController::surfaces()
{
    return m_geos;
}

void RenderController::setPartAndColor(Part *part, int colorId)
{
    if ((m_part == part) && (m_colorId == colorId))
        return;

    if (m_part)
        m_part->release();
    m_part = part;
    if (m_part)
        m_part->addRef();
    m_colorId = colorId;

    updateGeometries();
    emit partOrColorChanged();
}

bool RenderController::isAnimationActive() const
{
    return m_animationActive;
}

void RenderController::setAnimationActive(bool active)
{
    if (m_animationActive != active) {
        m_animationActive = active;
        emit animationActiveChanged();
    }
}

void RenderController::updateGeometries()
{
    qDeleteAll(m_geos);
    m_geos.clear();

    if (!m_part) {
        emit surfacesChanged();
        return;
    }

    QHash<int, QByteArray> surfaceBuffers;
    fillVertexBuffers(m_part, m_colorId, QMatrix4x4(), false, surfaceBuffers);

    for (auto it = surfaceBuffers.cbegin(); it != surfaceBuffers.cend(); ++it) {
        const QByteArray &data = it.value();
        if (data.isEmpty())
            continue;

        constexpr int stride = (3 + 3) * sizeof(float);

        auto geo = new RenderGeometry(it.key());

        // calculate bounding box
        static constexpr auto fmin = std::numeric_limits<float>::min();
        static constexpr auto fmax = std::numeric_limits<float>::max();

        QVector3D vmin = QVector3D(fmax, fmax, fmax);
        QVector3D vmax = QVector3D(fmin, fmin, fmin);

        for (int i = 0; i < data.size(); i += stride) {
            auto v = reinterpret_cast<const float *>(it->constData() + i);
            vmin = QVector3D(std::min(vmin.x(), v[0]), std::min(vmin.y(), v[1]), std::min(vmin.z(), v[2]));
            vmax = QVector3D(std::max(vmax.x(), v[0]), std::max(vmax.y(), v[1]), std::max(vmax.z(), v[2]));
        }

        // calculate bounding sphere
        QVector3D center = (vmin + vmax) / 2;
        float radius = 0;

        for (int i = 0; i < data.size(); i += stride) {
            auto v = reinterpret_cast<const float *>(it->constData() + i);
            radius = std::max(radius, (center - QVector3D { v[0], v[1], v[2] }).lengthSquared());
        }
        radius = std::sqrt(radius);

        geo->setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
        geo->setStride(stride);
        geo->addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
        geo->addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        geo->setBounds(vmin, vmax);
        geo->setCenter(center);
        geo->setRadius(radius);
        geo->setVertexData(data);

        m_geos.append(geo);
    }

    emit surfacesChanged();

    QVector3D center;
    float radius = 0;

    for (auto *geo : qAsConst(m_geos)) {
        // Merge all the bounding spheres. This is not perfect, but very, very close in most cases
        const auto geoCenter = geo->center();
        const auto geoRadius = geo->radius();

        if (!radius) { // first one
            center = geoCenter;
            radius = geoRadius;
        } else {
            QVector3D d = geoCenter - center;
            float l = d.length();

            if ((l + radius) < geoRadius) { // the old one is inside the new one
                center = geoCenter;
                radius = geoRadius;
            } else if ((l + geoRadius) > radius) { // the new one is NOT inside the old one -> we need to merge
                float nr = (radius + l + geoRadius) / 2;
                center = center + (geoCenter - center).normalized() * (nr - radius);
                radius = nr;
            }
        }

        geo->update();
    }

    if (m_center != center) {
        m_center = center;
        emit centerChanged();
    }
    if (m_radius != radius) {
        m_radius = radius;
        emit radiusChanged();
    }
}

void RenderController::fillVertexBuffers(Part *part, int baseColorId, const QMatrix4x4 &matrix,
                                         bool inverted, QHash<int, QByteArray> &surfaceBuffers)
{
    if (!part)
        return;

    bool invertNext = false;
    bool ccw = true;

    static auto addFloatsToByteArray = [](QByteArray &buffer, std::initializer_list<float> fs)
    {
        qsizetype oldSize = buffer.size();
        buffer.resize(buffer.size() + fs.size() * sizeof(float));
        float *ptr = reinterpret_cast<float *>(buffer.data() + oldSize);
        for (float f : fs)
            *ptr++ = f;
    };

    static auto addTriangle = [](QByteArray &buffer, const QVector3D &p0,
            const QVector3D &p1, const QVector3D &p2, const QMatrix4x4 &matrix)
    {
        auto p0m = matrix.map(p0);
        auto p1m = matrix.map(p1);
        auto p2m = matrix.map(p2);
        auto n = QVector3D::normal(p0m, p1m, p2m);

        for (const auto &p : { p0m, p1m, p2m })
            addFloatsToByteArray(buffer, { p.x(), p.y(), p.z(), n.x(), n.y(), n.z() });
    };

    auto mapColorId = [&](int colorId, int baseColorId) -> int
    {
        return (colorId == 16) ? ((baseColorId == -1) ? m_colorId : baseColorId)
                               : colorId;
    };

    Element * const *ep = part->elements().constData();
    int lastind = int(part->elements().count()) - 1;

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
            int colorId = mapColorId(te->color(), baseColorId);
            const auto p = te->points();
            addTriangle(surfaceBuffers[colorId], p[0], ccw ? p[2] : p[1], ccw ? p[1] : p[2], matrix);
            break;
        }
        case Element::Type::Quad: {
            const auto *qe = static_cast<const QuadElement *>(e);
            int colorId = mapColorId(qe->color(), baseColorId);
            const auto p = qe->points();
            addTriangle(surfaceBuffers[colorId], p[0], ccw ? p[3] : p[1], p[2], matrix);
            addTriangle(surfaceBuffers[colorId], p[2], ccw ? p[1] : p[3], p[0], matrix);
            break;
        }
        case Element::Type::Line: {
            break;
        }
        case Element::Type::CondLine: {
            break;
        }
        case Element::Type::Part: {
            const auto *pe = static_cast<const PartElement *>(e);
            bool matrixReversed = (pe->matrix().determinant() < 0);

            fillVertexBuffers(pe->part(), pe->color() == 16 ? baseColorId : pe->color(),
                              matrix * pe->matrix(), inverted ^ invertNext ^ matrixReversed,
                              surfaceBuffers);
            break;
        }
        default:
            break;
        }

        if (!isBFCCommand || !isBFCInvertNext)
            invertNext = false;
    }
}

void RenderController::resetCamera()
{
    emit qmlResetCamera();
}

float RenderController::aoStrength() const
{
    return m_aoStrength;
}

void RenderController::setAoStrength(float newAoStrength)
{
    if (!qFuzzyCompare(m_aoStrength, newAoStrength)) {
        m_aoStrength = newAoStrength;
        emit aoStrengthChanged();
    }
}

float RenderController::aoDistance() const
{
    return m_aoDistance;
}

void RenderController::setAoDistance(float newAoDistance)
{
    if (!qFuzzyCompare(m_aoDistance, newAoDistance)) {
        m_aoDistance = newAoDistance;
        emit aoDistanceChanged();
    }
}

float RenderController::aoSoftness() const
{
    return m_aoSoftness;
}

void RenderController::setAoSoftness(float newAoSoftness)
{
    if (!qFuzzyCompare(m_aoSoftness, newAoSoftness)) {
        m_aoSoftness = newAoSoftness;
        emit aoSoftnessChanged();
    }
}

bool RenderController::renderLines() const
{
    return m_renderLines;
}

void RenderController::setRenderLines(bool newRenderLines)
{
    if (m_renderLines != newRenderLines) {
        m_renderLines = newRenderLines;
        emit renderLinesChanged();
    }
}

QVector3D RenderController::center() const
{
    return m_center;
}

float RenderController::radius() const
{
    return m_radius;
}

float RenderController::lightProbeExposure() const
{
    return m_lightProbeExposure;
}

void RenderController::setLightProbeExposure(float newLightProbeExposure)
{
    if (qFuzzyCompare(m_lightProbeExposure, newLightProbeExposure))
        return;
    m_lightProbeExposure = newLightProbeExposure;
    emit lightProbeExposureChanged();
}

float RenderController::plainMetalness() const
{
    return m_plainMetalness;
}

void RenderController::setPlainMetalness(float newPlainMetalness)
{
    if (qFuzzyCompare(m_plainMetalness, newPlainMetalness))
        return;
    m_plainMetalness = newPlainMetalness;
    emit plainMetalnessChanged();
}

float RenderController::plainRoughness() const
{
    return m_plainRoughness;
}

void RenderController::setPlainRoughness(float newPlainRoughness)
{
    if (qFuzzyCompare(m_plainRoughness, newPlainRoughness))
        return;
    m_plainRoughness = newPlainRoughness;
    emit plainRoughnessChanged();
}

float RenderController::chromeMetalness() const
{
    return m_chromeMetalness;
}

void RenderController::setChromeMetalness(float newChromeMetalness)
{
    if (qFuzzyCompare(m_chromeMetalness, newChromeMetalness))
        return;
    m_chromeMetalness = newChromeMetalness;
    emit chromeMetalnessChanged();
}

float RenderController::chromeRoughness() const
{
    return m_chromeRoughness;
}

void RenderController::setChromeRoughness(float newChromeRoughness)
{
    if (qFuzzyCompare(m_chromeRoughness, newChromeRoughness))
        return;
    m_chromeRoughness = newChromeRoughness;
    emit chromeRoughnessChanged();
}

float RenderController::metalMetalness() const
{
    return m_metalMetalness;
}

void RenderController::setMetalMetalness(float newMetalMetalness)
{
    if (qFuzzyCompare(m_metalMetalness, newMetalMetalness))
        return;
    m_metalMetalness = newMetalMetalness;
    emit metalMetalnessChanged();
}

float RenderController::metalRoughness() const
{
    return m_metalRoughness;
}

void RenderController::setMetalRoughness(float newMetalRoughness)
{
    if (qFuzzyCompare(m_metalRoughness, newMetalRoughness))
        return;
    m_metalRoughness = newMetalRoughness;
    emit metalRoughnessChanged();
}

float RenderController::pearlescentMetalness() const
{
    return m_pearlescentMetalness;
}

void RenderController::setPearlescentMetalness(float newPearlescentMetalness)
{
    if (qFuzzyCompare(m_pearlescentMetalness, newPearlescentMetalness))
        return;
    m_pearlescentMetalness = newPearlescentMetalness;
    emit pearlescentMetalnessChanged();
}

float RenderController::pearlescentRoughness() const
{
    return m_pearlescentRoughness;
}

void RenderController::setPearlescentRoughness(float newPearlescentRoughness)
{
    if (qFuzzyCompare(m_pearlescentRoughness, newPearlescentRoughness))
        return;
    m_pearlescentRoughness = newPearlescentRoughness;
    emit pearlescentRoughnessChanged();
}

float RenderController::matteMetallicMetalness() const
{
    return m_matteMetallicMetalness;
}

void RenderController::setMatteMetallicMetalness(float newMatteMetallicMetalness)
{
    if (qFuzzyCompare(m_matteMetallicMetalness, newMatteMetallicMetalness))
        return;
    m_matteMetallicMetalness = newMatteMetallicMetalness;
    emit matteMetallicMetalnessChanged();
}

float RenderController::matteMetallicRoughness() const
{
    return m_matteMetallicRoughness;
}

void RenderController::setMatteMetallicRoughness(float newMatteMetallicRoughness)
{
    if (qFuzzyCompare(m_matteMetallicRoughness, newMatteMetallicRoughness))
        return;
    m_matteMetallicRoughness = newMatteMetallicRoughness;
    emit matteMetallicRoughnessChanged();
}

float RenderController::rubberMetalness() const
{
    return m_rubberMetalness;
}

void RenderController::setRubberMetalness(float newRubberMetalness)
{
    if (qFuzzyCompare(m_rubberMetalness, newRubberMetalness))
        return;
    m_rubberMetalness = newRubberMetalness;
    emit rubberMetalnessChanged();
}

float RenderController::rubberRoughness() const
{
    return m_rubberRoughness;
}

void RenderController::setRubberRoughness(float newRubberRoughness)
{
    if (qFuzzyCompare(m_rubberRoughness, newRubberRoughness))
        return;
    m_rubberRoughness = newRubberRoughness;
    emit rubberRoughnessChanged();
}

float RenderController::tumblingAnimationAngle() const
{
    return m_tumblingAnimationAngle;
}

void RenderController::setTumblingAnimationAngle(float newTumblingAnimationAngle)
{
    if (qFuzzyCompare(m_tumblingAnimationAngle, newTumblingAnimationAngle))
        return;
    m_tumblingAnimationAngle = newTumblingAnimationAngle;
    emit tumblingAnimationAngleChanged();
}

QVector3D RenderController::tumblingAnimationAxis() const
{
    return m_tumblingAnimationAxis;
}

void RenderController::setTumblingAnimationAxis(const QVector3D &newTumblingAnimationAxis)
{
    if (m_tumblingAnimationAxis == newTumblingAnimationAxis)
        return;
    m_tumblingAnimationAxis = newTumblingAnimationAxis;
    emit tumblingAnimationAxisChanged();
}

bool RenderController::showBoundingSpheres() const
{
    return m_showBoundingSpheres;
}

void RenderController::setShowBoundingSpheres(bool newShowBoundingSpheres)
{
    if (m_showBoundingSpheres == newShowBoundingSpheres)
        return;
    m_showBoundingSpheres = newShowBoundingSpheres;
    emit showBoundingSpheresChanged();
}


} // namespace LDraw

#include "moc_rendercontroller.cpp"
