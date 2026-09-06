// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>

#include <QtCore/QDebug>

#include "ldraw/partimagerenderer.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)

namespace LDraw {

QCoro::Task<QImage> renderPartImage(const BrickLink::Item *, const BrickLink::Color *,
                                    PartImageOptions)
{
    qWarning() << "renderPartImage: the offscreen renderer needs Qt 6.6 or newer";
    co_return { };
}

} // namespace LDraw

#else // QT_VERSION >= 6.6

#include <QtCore/QCoreApplication>
#include <QtCore/QScopeGuard>
#include <QtCore/QUrl>
#include <QtGui/QPainter>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickRenderTarget>
#include <QtQuick/QQuickWindow>

#include <rhi/qrhi.h>

#include <QCoro/QCoroFuture>
#include <QCoro/QCoroSignal>

#include "bricklink/color.h"
#include "bricklink/item.h"
#include "common/application.h"
#include "ldraw/library.h"
#include "ldraw/part.h"
#include "ldraw/rendercontroller.h"
#include "ldraw/rendersettings.h"

using namespace std::chrono_literals;

namespace LDraw {

// the bounding box of everything that is not fully transparent
static QRect alphaBoundingBox(const QImage &img)
{
    Q_ASSERT(img.format() == QImage::Format_ARGB32_Premultiplied);

    int top = -1, bottom = -1;
    int left = img.width(), right = -1;

    for (int y = 0; y < img.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        int rowLeft = -1, rowRight = -1;

        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x])) {
                if (rowLeft < 0)
                    rowLeft = x;
                rowRight = x;
            }
        }
        if (rowLeft >= 0) {
            if (top < 0)
                top = y;
            bottom = y;
            left = std::min(left, rowLeft);
            right = std::max(right, rowRight);
        }
    }
    if (top < 0)
        return { };
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

// crop to the silhouette, scale it down so the longer side fills the target minus the
// margin, then composite it onto a canvas of the background color. The render is always
// transparent, so this is the only place the background exists.
// Scaling down first, and while the image is still premultiplied, is what keeps the
// filtering right and the memory bounded: QImage::scaled only dispatches to the
// area-averaging scaler for the premultiplied formats - everything else goes through a
// bilinear painter path unless the image shrinks by more than 2x - and padding out before
// scaling would allocate up to 1/(1-2*margin) squared times the rendered image.
static QImage fitToSilhouette(const QImage &img, QSize size, qreal marginPercent,
                              const QColor &background)
{
    const QRect bbox = alphaBoundingBox(img);
    if (bbox.isEmpty())
        return { };

    const qreal margin = qBound(qreal(0), marginPercent, qreal(45));
    const qreal keep = 1. - 2. * margin / 100.;
    const QSize content { qMax(1, qRound(size.width() * keep)),
                          qMax(1, qRound(size.height() * keep)) };

    const QImage scaled = img.copy(bbox).scaled(content, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    {
        QPainter p(&image);
        p.drawImage(QPoint((size.width() - scaled.width()) / 2,
                           (size.height() - scaled.height()) / 2), scaled);
    }

    // an opaque background means the PNG does not need an alpha channel at all
    return image.convertToFormat((background.alpha() == 255) ? QImage::Format_RGB32
                                                             : QImage::Format_ARGB32);
}

QCoro::Task<QImage> renderPartImage(const BrickLink::Item *item, const BrickLink::Color *color,
                                    PartImageOptions opts)
{
    if (!item || !library()->isValid())
        co_return { };

    if (opts.size.isEmpty() || (opts.supersample <= 0)) {
        qWarning() << "renderPartImage: invalid size" << opts.size << "or supersample factor"
                   << opts.supersample;
        co_return { };
    }
    // the real limit is the GPU's, checked below once there is a QRhi to ask; this one only
    // keeps the products from overflowing on the way there
    const int longestEdge = qMax(opts.size.width(), opts.size.height());
    if ((qint64(longestEdge) * opts.supersample) > 65536) {
        qWarning() << "renderPartImage:" << longestEdge << "pixels with" << opts.supersample
                   << "times super-sampling is not a plausible image size";
        co_return { };
    }

    // resolve the part before touching the renderer: an item without an LDraw mapping would
    // otherwise just never produce geometry
    const PartRef part = co_await library()->partFromBrickLinkId(item->id());
    if (!part) {
        qWarning() << "renderPartImage: part" << item->id() << "is not renderable";
        co_return { };
    }

    QQuickRenderControl rc;
    QQuickWindow window(&rc);
    window.setColor(Qt::transparent);

    QQmlComponent component(Application::inst()->qmlEngine(),
                            QUrl(u"qrc:/LDraw/PartRenderer.qml"_qs));
    std::unique_ptr<QObject> rootObject(component.create());
    auto *rootItem = qobject_cast<QQuickItem *>(rootObject.get());
    if (!rootItem) {
        qWarning() << "renderPartImage: could not create PartRenderer:" << component.errorString();
        co_return { };
    }
    auto *controller = rootItem->property("renderController").value<RenderController *>();
    if (!controller) {
        qWarning() << "renderPartImage: PartRenderer has no RenderController";
        co_return { };
    }

    rootItem->setParentItem(window.contentItem());

    if (!rc.initialize()) {
        qWarning() << "renderPartImage: could not initialize an offscreen render control";
        co_return { };
    }
    QRhi *rhi = rc.rhi();

    // the super-sample factor is a quality knob, not a request: give up as much of it as the
    // GPU's texture limit demands rather than failing on a size the user is allowed to ask for
    const int rhiMaxEdge = rhi->resourceLimit(QRhi::TextureSizeMax);
    if (longestEdge > rhiMaxEdge) {
        qWarning() << "renderPartImage:" << longestEdge
                   << "pixels exceeds the maximum texture size of" << rhiMaxEdge;
        co_return { };
    }
    const int wantedSupersample = opts.supersample;
    while ((opts.supersample > 1) && ((longestEdge * opts.supersample) > rhiMaxEdge))
        --opts.supersample;
    if (opts.supersample != wantedSupersample) {
        qWarning() << "renderPartImage: reduced the super-sample factor from" << wantedSupersample
                   << "to" << opts.supersample << "to stay within the maximum texture size of"
                   << rhiMaxEdge;
    }
    const QSize renderSize = opts.size * opts.supersample;

    // The View3D's SSAA pass is a supersample of its own - VeryHigh renders the layer at 2x and
    // scales it back - so on top of our factor it costs 4x the pixels to do a job we are
    // already doing with a better filter. Keep it only when there is nothing to downscale.
    const bool ssaa = (opts.supersample == 1) && ((longestEdge * 2) <= rhiMaxEdge);

    rootItem->setSize(QSizeF(renderSize));
    window.contentItem()->setSize(rootItem->size());
    window.setGeometry(QRect(QPoint(0, 0), renderSize));

    // pin down what a still image must not inherit from the user's persisted settings. The
    // material, lighting and AO values stay as they are - that tuning is the renderer's look,
    // and matching it is the point.
    rootItem->setProperty("renderLines", opts.renderLines);
    rootItem->setProperty("antiAliasing", int(ssaa ? RenderSettings::AntiAliasing::VeryHigh
                                                   : RenderSettings::AntiAliasing::No));
    rootItem->setProperty("modelRotation", QVariant::fromValue(
                              opts.rotation.value_or(RenderSettings::inst()->defaultRotation())));

    controller->setClearColor(Qt::transparent);
    controller->setItemAndColor(item, color);

    // setItemAndColor() ends in applyRenderData({}), which emits itemOrColorChanged with an
    // empty surface list - so wait for a non-empty one, not just for the first emission
    while (controller->surfaces().isEmpty()) {
        const auto emitted = co_await qCoro(controller, &RenderController::itemOrColorChanged, 30s);
        if (!emitted) {
            qWarning() << "renderPartImage: timed out waiting for the geometry of" << item->id();
            co_return { };
        }
    }
    // the geometry arrived after the size was set, so re-fit
    QMetaObject::invokeMethod(rootItem, "scaleToFit");

    // sample count 1: the antialiasing comes from the View3D's SSAA pass and from the
    // supersampled downscale, neither of which needs an MSAA render target
    std::unique_ptr<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, renderSize, 1,
                                                     QRhiTexture::RenderTarget
                                                     | QRhiTexture::UsedAsTransferSource));
    std::unique_ptr<QRhiRenderBuffer> ds(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                                              renderSize, 1));
    if (!tex->create() || !ds->create()) {
        qWarning() << "renderPartImage: could not create a" << renderSize << "render target";
        co_return { };
    }
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(tex.get()));
    rtDesc.setDepthStencilBuffer(ds.get());
    std::unique_ptr<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());
    if (!rt->create()) {
        qWarning() << "renderPartImage: could not create a" << renderSize << "render target";
        co_return { };
    }

    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(rt.get()));

    // declared after the QRhi objects, so it runs before they are destroyed
    const auto releaseTarget = qScopeGuard([&window, &rc]() {
        window.setRenderTarget({ });
        rc.invalidate();
    });

    QImage rendered;

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    rc.polishItems();
    rc.beginFrame();
    rc.sync();
    rc.render();

    QRhiReadbackResult readResult;
    readResult.completed = [&readResult, &rendered, rhi]() {
        const QImage wrapper(reinterpret_cast<const uchar *>(readResult.data.constData()),
                             readResult.pixelSize.width(), readResult.pixelSize.height(),
                             QImage::Format_RGBA8888_Premultiplied);
        rendered = (rhi->isYUpInFramebuffer() ? wrapper.mirrored(false, true) : wrapper)
                   .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    };
    QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
    rub->readBackTexture(tex.get(), &readResult);
    rc.commandBuffer()->resourceUpdate(rub);

    // offscreen frames are synchronous, so the readback has completed once this returns
    rc.endFrame();

    if (rendered.isNull()) {
        qWarning() << "renderPartImage: the readback produced no image";
        co_return { };
    }

    const QImage image = fitToSilhouette(rendered, opts.size, opts.margin, opts.background);
    if (image.isNull())
        qWarning() << "renderPartImage: nothing was rendered for" << item->id();
    co_return image;
}

} // namespace LDraw

#endif // QT_VERSION >= 6.6
