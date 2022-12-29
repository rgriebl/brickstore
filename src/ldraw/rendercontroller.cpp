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
#include <cmath>

#include <QtConcurrent>
#include <QRandomGenerator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QQuick3DTextureData>
#include <QPainter>

#include "qcoro/qcorofuture.h"
#include "bricklink/core.h"
#include "library.h"
#include "part.h"
#include "rendercontroller.h"


namespace LDraw {

QHash<const BrickLink::Color *, QImage> RenderController::s_materialTextureDatas;


RenderController::RenderController(QObject *parent)
    : QObject(parent)
    , m_lineGeo(new QQuick3DGeometry())
    , m_lines(new QmlRenderLineInstancing())
    , m_clearColor(Qt::white)
    , m_updateTimer(new QTimer(this))
{
    static const float lineGeo[] = {
        0, -0.5, 0,
        0, -0.5, 1,
        0, 0.5, 1,
        0, -0.5, 0,
        0, 0.5, 1,
        0, 0.5, 0
    };

    m_lineGeo->setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    m_lineGeo->setStride(3 * sizeof(float));
    m_lineGeo->addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
    m_lineGeo->setVertexData(QByteArray::fromRawData(reinterpret_cast<const char *>(lineGeo), sizeof(lineGeo)));

    m_updateTimer->setInterval(200);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, &RenderController::updateGeometries);
    m_updateTimer->start();
}

RenderController::~RenderController()
{
    for (auto *geo : m_geos)
        delete geo;
    delete m_lines;
    delete m_lineGeo;
    if (m_part)
        m_part->release();
}

QQuaternion RenderController::rotateArcBall(QPointF pressPos, QPointF mousePos,
                                            QQuaternion pressRotation, QSizeF viewportSize)
{
    // map the mouse coordiantes to the sphere describing this arcball
    auto mapMouseToBall = [=](const QPointF &mouse) -> QVector3D {
        // normalize mouse pos to -1..+1 and reverse y
        QVector3D mouseView(
                    float(2 * mouse.x() / viewportSize.width() - 1),
                    float(1 - 2 * mouse.y() / viewportSize.height()),
                    0
        );

        QVector3D mapped = mouseView; // (mouseView - m_center)* (1.f / m_radius);
        float l2 = mapped.lengthSquared();
        if (l2 > 1.f) {
            mapped.normalize();
            mapped[2] = 0.f;
        } else {
            mapped[2] = std::sqrt(1.f - l2);
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

const BrickLink::Item *RenderController::item() const
{
    return m_item;
}

const BrickLink::Color *RenderController::color() const
{
    return m_color;
}

QList<QmlRenderGeometry *> RenderController::surfaces()
{
    return m_geos;
}

QQuick3DGeometry *RenderController::lineGeometry()
{
    return m_lineGeo;
}

QQuick3DInstancing *RenderController::lines()
{
    return m_lines;
}

void RenderController::setItemAndColor(BrickLink::QmlItem item, BrickLink::QmlColor color)
{
    const BrickLink::Item *i = item.wrappedObject();
    const BrickLink::Color *c = color.wrappedObject();

    setItemAndColor(i, c);
}

void RenderController::setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color)
{
    if (!color)
        color = BrickLink::core()->color(99); // Very Light Bluish Gray

    if (item == m_item) {
        if (!item || (color == m_color))
            return;

        // switch color - redraw, if we have a Part loaded already
        m_color = color;
        if (m_part)
            m_updateTimer->start();
    } else {
        // new item
        if (m_part)
            m_part->release();
        m_part = nullptr;
        m_item = item;
        m_color = color;

        m_updateTimer->start();

        if (item) {
            LDraw::library()->partFromId(item->id()).then(this, [this, item](Part *part) {
                bool stillCurrentItem = (item == m_item);

                if ((m_part != part) && stillCurrentItem) {
                    if (m_part)
                        m_part->release();
                    m_part = part;
                    if (m_part)
                        m_part->addRef();
                }
                // the future comes already ref'ed
                if (part)
                    part->release();
                if (stillCurrentItem)
                    updateGeometries();
            });
        }
    }
}

bool RenderController::canRender() const
{
    return (m_part);
}

QCoro::Task<void> RenderController::updateGeometries()
{
    m_updateTimer->stop();

    if (!m_part) {
        qDeleteAll(m_geos);
        m_geos.clear();
        m_lines->clear();

        emit surfacesChanged();
        emit itemOrColorChanged();
        emit canRenderChanged(canRender());
        co_return;
    }

    // in
    Part *part = m_part;
    const BrickLink::Color *color = m_color;

    // out
    float radius = 0;
    QVector3D center;
    QList<QmlRenderGeometry *> geos;
    QByteArray lineBuffer;

    co_await QtConcurrent::run([part, color, &lineBuffer, &geos, &radius, &center]() {
        QHash<const BrickLink::Color *, QByteArray> surfaceBuffers;
        RenderController::fillVertexBuffers(part, color, color, QMatrix4x4(), false, surfaceBuffers, lineBuffer);

        for (auto it = surfaceBuffers.cbegin(); it != surfaceBuffers.cend(); ++it) {
            const QByteArray &data = it.value();
            if (data.isEmpty())
                continue;

            const BrickLink::Color *surfaceColor = it.key();

            const int stride = (3 + 3 + (surfaceColor->hasParticles() ? 2 : 0)) * sizeof(float);

            auto geo = new QmlRenderGeometry(surfaceColor);

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
            QVector3D surfaceCenter = (vmin + vmax) / 2;
            float surfaceRadius = 0;

            for (int i = 0; i < data.size(); i += stride) {
                auto v = reinterpret_cast<const float *>(it->constData() + i);
                surfaceRadius = std::max(surfaceRadius, (surfaceCenter - QVector3D { v[0], v[1], v[2] }).lengthSquared());
            }
            surfaceRadius = std::sqrt(surfaceRadius);

            geo->setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
            geo->setStride(stride);
            geo->addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
            geo->addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
            if (surfaceColor->hasParticles()) {
                geo->addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic, 6 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);

                QQuick3DTextureData *texData = generateMaterialTextureData(surfaceColor);
                texData->setParentItem(geo);
                geo->setTextureData(texData);
            }
            geo->setBounds(vmin, vmax);
            geo->setCenter(surfaceCenter);
            geo->setRadius(surfaceRadius);
            geo->setVertexData(data);

            geos.append(geo);
        }

        for (auto *geo : std::as_const(geos)) {
            // Merge all the bounding spheres. This is not perfect, but very, very close in most cases
            const auto geoCenter = geo->center();
            const auto geoRadius = geo->radius();

            if (qFuzzyIsNull(radius)) { // first one
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
        }
    });

    qDeleteAll(m_geos);
    m_geos.clear();
    m_lines->clear();

    m_geos = geos;
    emit surfacesChanged();

    m_lines->setBuffer(lineBuffer);
    m_lines->update();


    if (m_center != center) {
        m_center = center;
        emit centerChanged();
    }
    if (!qFuzzyCompare(m_radius, radius)) {
        m_radius = radius;
        emit radiusChanged();
    }
    emit itemOrColorChanged();
    emit canRenderChanged(canRender());
}

void RenderController::fillVertexBuffers(Part *part, const BrickLink::Color *modelColor,
                                         const BrickLink::Color *baseColor,const QMatrix4x4 &matrix,
                                         bool inverted, QHash<const BrickLink::Color *, QByteArray> &surfaceBuffers,
                                         QByteArray &lineBuffer)
{
    if (!part)
        return;

    bool invertNext = false;
    bool ccw = true;

    static auto addFloatsToByteArray = [](QByteArray &buffer, std::initializer_list<float> fs) {
        qsizetype oldSize = buffer.size();
        size_t size = fs.size() * sizeof(float);
        buffer.resize(oldSize + qsizetype(size));
        float *ptr = reinterpret_cast<float *>(buffer.data() + oldSize);
        memcpy(ptr, fs.begin(), size);
    };

    auto mapColor = [&baseColor, &modelColor](int colorId) -> const BrickLink::Color * {
        auto c = (colorId == 16) ? (baseColor ? baseColor : modelColor)
                                 : BrickLink::core()->colorFromLDrawId(colorId);
        if (!c && colorId >= 256) {
            int newColorId = ((colorId - 256) & 0x0f);
            qCWarning(LogLDraw) << "Dithered colors are not supported, using only one:"
                                << colorId << "->" << newColorId;
            c = BrickLink::core()->colorFromLDrawId(newColorId);
        }
        if (!c) {
            qCWarning(LogLDraw) << "Could not map LDraw color" << colorId;
            c = BrickLink::core()->color(9 /*light gray*/);
        }
        return c;
    };

    auto mapEdgeQColor = [&baseColor, &modelColor](int colorId) -> QColor {
        if (colorId == 24) {
            if (baseColor)
                return baseColor->ldrawEdgeColor();
            else if (modelColor)
                return modelColor->ldrawEdgeColor();
        } else if (auto *c = BrickLink::core()->colorFromLDrawId(colorId)) {
            return c->ldrawColor();
        }
        return Qt::black;
    };

    const auto &elements = part->elements();
    for (const Element *e : elements) {
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
            const auto te = static_cast<const TriangleElement *>(e);
            const auto color = mapColor(te->color());
            const auto p = te->points();
            const auto p0m = matrix.map(p[0]);
            const auto p1m = matrix.map(ccw ? p[2] : p[1]);
            const auto p2m = matrix.map(ccw ? p[1] : p[2]);
            const auto n = QVector3D::normal(p0m, p1m, p2m);

            if (color->hasParticles()) {
                float u[3], v[3];
                const float l1 = p0m.distanceToPoint(p1m) / 24;
                const float l2 = p0m.distanceToPoint(p2m) / 24;
                //const float h2 = p2m.distanceToLine(p0m, p1m - p0m) / 24; // sometimes way off
                const float h2 = QVector3D::crossProduct(p2m - p0m, p2m - p1m).length() / (p1m - p0m).length() / 24;

                QRandomGenerator *rd = QRandomGenerator::global();
                float su = float(rd->generateDouble());
                float sv = float(rd->generateDouble());

                u[0] = su;
                v[0] = sv;
                u[1] = su + l1;
                v[1] = sv;
                u[2] = su + std::sqrt(l2 * l2 - h2 * h2);
                v[2] = sv + h2;

                addFloatsToByteArray(surfaceBuffers[color], {
                    p0m.x(), p0m.y(), p0m.z(), n.x(), n.y(), n.z(), u[0], v[0],
                    p1m.x(), p1m.y(), p1m.z(), n.x(), n.y(), n.z(), u[1], v[1],
                    p2m.x(), p2m.y(), p2m.z(), n.x(), n.y(), n.z(), u[2], v[2] });
            } else {
                addFloatsToByteArray(surfaceBuffers[color], {
                    p0m.x(), p0m.y(), p0m.z(), n.x(), n.y(), n.z(),
                    p1m.x(), p1m.y(), p1m.z(), n.x(), n.y(), n.z(),
                    p2m.x(), p2m.y(), p2m.z(), n.x(), n.y(), n.z() });
            }
            break;
        }
        case Element::Type::Quad: {
            const auto qe = static_cast<const QuadElement *>(e);
            const auto color = mapColor(qe->color());
            const auto p = qe->points();
            const auto p0m = matrix.map(p[0]);
            const auto p1m = matrix.map(p[ccw ? 3 : 1]);
            const auto p2m = matrix.map(p[2]);
            const auto p3m = matrix.map(p[ccw ? 1 : 3]);
            const auto n = QVector3D::normal(p0m, p1m, p2m);

            if (color->hasParticles()) {
                float u[4], v[4];
                const float l1 = p0m.distanceToPoint(p1m) / 24;
                const float l3 = p0m.distanceToPoint(p3m)/ 24;
                QRandomGenerator *rd = QRandomGenerator::global();
                const float su = float(rd->generateDouble());
                const float sv = float(rd->generateDouble());

                u[0] = su;
                v[0] = sv;
                u[1] = su + l1;
                v[1] = sv;
                u[2] = su + l1;
                v[2] = sv + l3;
                u[3] = su;
                v[3] = sv + l3;
                addFloatsToByteArray(surfaceBuffers[color], {
                    p0m.x(), p0m.y(), p0m.z(), n.x(), n.y(), n.z(), u[0], v[0],
                    p1m.x(), p1m.y(), p1m.z(), n.x(), n.y(), n.z(), u[1], v[1],
                    p2m.x(), p2m.y(), p2m.z(), n.x(), n.y(), n.z(), u[2], v[2],
                    p2m.x(), p2m.y(), p2m.z(), n.x(), n.y(), n.z(), u[2], v[2],
                    p3m.x(), p3m.y(), p3m.z(), n.x(), n.y(), n.z(), u[3], v[3],
                    p0m.x(), p0m.y(), p0m.z(), n.x(), n.y(), n.z(), u[0], v[0] });
            } else {
                addFloatsToByteArray(surfaceBuffers[color], {
                    p0m.x(), p0m.y(), p0m.z(), n.x(), n.y(), n.z(),
                    p1m.x(), p1m.y(), p1m.z(), n.x(), n.y(), n.z(),
                    p2m.x(), p2m.y(), p2m.z(), n.x(), n.y(), n.z(),
                    p2m.x(), p2m.y(), p2m.z(), n.x(), n.y(), n.z(),
                    p3m.x(), p3m.y(), p3m.z(), n.x(), n.y(), n.z(),
                    p0m.x(), p0m.y(), p0m.z(), n.x(), n.y(), n.z() });
            }
            break;
        }
        case Element::Type::Line: {
            const auto le = static_cast<const LineElement *>(e);
            const auto c = mapEdgeQColor(le->color());
            const auto p = le->points();
            auto p0m = matrix.map(p[0]);
            auto p1m = matrix.map(p[1]);
            QmlRenderLineInstancing::addLineToBuffer(lineBuffer, c, p0m, p1m);
            break;
        }
        case Element::Type::CondLine: {
            const auto cle = static_cast<const CondLineElement *>(e);
            const auto c = mapEdgeQColor(cle->color());
            const auto p = cle->points();
            auto p0m = matrix.map(p[0]);
            auto p1m = matrix.map(p[1]);
            auto p2m = matrix.map(p[2]);
            auto p3m = matrix.map(p[3]);
            QmlRenderLineInstancing::addConditionalLineToBuffer(lineBuffer, c, p0m, p1m, p2m, p3m);
            break;
        }
        case Element::Type::Part: {
            const auto pe = static_cast<const PartElement *>(e);
            bool matrixReversed = (pe->matrix().determinant() < 0);

            fillVertexBuffers(pe->part(), modelColor, mapColor(pe->color()), matrix * pe->matrix(),
                              inverted ^ invertNext ^ matrixReversed, surfaceBuffers, lineBuffer);
            break;
        }
        default:
            break;
        }

        if (!isBFCCommand || !isBFCInvertNext)
            invertNext = false;
    }
}

QQuick3DTextureData *RenderController::generateMaterialTextureData(const BrickLink::Color *color)
{
    if (color && color->hasParticles()) {
        QImage texImage = s_materialTextureDatas.value(color);

        if (texImage.isNull()) {
            const bool isSpeckle = color->isSpeckle();

            QString cacheName = QLatin1String(isSpeckle ? "Speckle" : "Glitter")
                    + u'_' + color->ldrawColor().name(QColor::HexArgb)
                    + u'_' + color->particleColor().name(QColor::HexArgb)
                    + u'_' + QString::number(double(color->particleMinSize()))
                    + u'_' + QString::number(double(color->particleMaxSize()))
                    + u'_' + QString::number(double(color->particleFraction()));

            static auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            QString cacheFile = cacheDir + u"/ldraw-textures/" + cacheName + u".png";

            if (!texImage.load(cacheFile) || texImage.isNull()) {
                const int particleSize = 50;

                QPixmap particle(particleSize, particleSize);
                if (isSpeckle) {
                    particle.fill(Qt::transparent);
                    QPainter pp(&particle);
                    pp.setRenderHint(QPainter::Antialiasing);
                    pp.setPen(Qt::NoPen);
                    pp.setBrush(color->particleColor());
                    pp.drawEllipse(particle.rect());
                } else {
                    particle.fill(color->particleColor());
                }

                float particleArea = (color->particleMinSize() + color->particleMaxSize()) / 2.f;
                particleArea *= particleArea;
                if (isSpeckle)
                    particleArea *= (float(M_PI) / 4.f);

                const int texSize = 512; // ~ 24 LDU, the width of a 1 x 1 Brick
                const float ldus = 24.f;

                int particleCount = int(std::floor((ldus * ldus * color->particleFraction()) / particleArea));
                int delta = int(std::ceil(color->particleMaxSize() * texSize / ldus));

                QImage img(texSize + delta * 2, texSize + delta * 2, QImage::Format_ARGB32);
                // we need to use .rgba() here - otherwise the alpha channel will be premultiplied to RGB
                img.fill(color->ldrawColor().rgba());

                QList<QPainter::PixmapFragment> fragments;
                fragments.reserve(particleCount);
                QRandomGenerator *rd = QRandomGenerator::global();
                std::uniform_real_distribution<> dis(double(color->particleMinSize()),
                                                     double(color->particleMaxSize()));

                float neededArea = std::floor(texSize * texSize * color->particleFraction());
                float filledArea = 0;

                //TODO: maybe partition the square into a grid and use random noise to offset drawing
                //      into each cell to get a more uniform distribution

                while (filledArea < neededArea) {
                    float x = float(rd->bounded(texSize) + delta);
                    float y = float(rd->bounded(texSize) + delta);
                    float sx = qMax(1.f / (particleSize - 5), texSize / (ldus * particleSize) * dis(*rd));
                    float sy = isSpeckle ? sx : qMax(1.f / (particleSize - 5), texSize / (ldus * particleSize) * dis(*rd));
                    float rot = isSpeckle ? 0.f : rd->bounded(90.f);
                    float opacity = isSpeckle ? 1.f : qBound(0.f, (rd->bounded(.3f) + .7f), 1.f);

                    float area = particleSize * particleSize * sx * sy;
                    if (isSpeckle)
                        area *= (M_PI / 4.f);
                    filledArea += area;

                    fragments << QPainter::PixmapFragment::create({ x, y }, particle.rect(), sx, sy, rot, opacity);

                    // make it seamless
                    if (x < 2 * delta)
                        fragments << QPainter::PixmapFragment::create({ x + texSize, y }, particle.rect(), sx, sy, rot, opacity);
                    else if (x > texSize)
                        fragments << QPainter::PixmapFragment::create({ x - texSize, y }, particle.rect(), sx, sy, rot, opacity);
                    if (y < 2 * delta)
                        fragments << QPainter::PixmapFragment::create({ x, y + texSize }, particle.rect(), sx, sy, rot, opacity);
                    else if (y > texSize)
                        fragments << QPainter::PixmapFragment::create({ x, y - texSize }, particle.rect(), sx, sy, rot, opacity);
                }

                QPainter p(&img);
                p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                p.drawPixmapFragments(fragments.constData(), int(fragments.count()), particle);
                p.end();

                texImage = img.copy(delta, delta, texSize, texSize).rgbSwapped()
                        .scaled(texSize / 2, texSize / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

                QDir(QFileInfo(cacheFile).absolutePath()).mkpath(QLatin1String("."));
                texImage.save(cacheFile);
            }
            s_materialTextureDatas.insert(color, texImage);
        }

        auto texData = new QQuick3DTextureData();
        texData->setFormat(QQuick3DTextureData::RGBA8);
        texData->setSize(texImage.size());
        texData->setHasTransparency(color->ldrawColor().alpha() < 255);
        texData->setTextureData(QByteArray { reinterpret_cast<const char *>(texImage.constBits()),
                                             texImage.sizeInBytes() });
        return texData;
    }
    return nullptr;
}

void RenderController::resetCamera()
{
    emit qmlResetCamera();
}

QVector3D RenderController::center() const
{
    return m_center;
}

float RenderController::radius() const
{
    return m_radius;
}

bool RenderController::isTumblingAnimationActive() const
{
    return m_tumblingAnimationActive;
}

void RenderController::setTumblingAnimationActive(bool active)
{
    if (m_tumblingAnimationActive != active) {
        m_tumblingAnimationActive = active;
        emit tumblingAnimationActiveChanged();
    }
}

const QColor &RenderController::clearColor() const
{
    return m_clearColor;
}

void RenderController::setClearColor(const QColor &newClearColor)
{
    if (m_clearColor != newClearColor) {
        m_clearColor = newClearColor;
        emit clearColorChanged(m_clearColor);
    }
}

} // namespace LDraw

#include "moc_rendercontroller.cpp"
