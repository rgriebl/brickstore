// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QtCore/QLoggingCategory>

#include "bricklink/color.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/picture.h"

#include "qmlimageprovider.h"

Q_DECLARE_LOGGING_CATEGORY(LogQml)


QmlImageProvider::QmlImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{ }

QImage QmlImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // requestedSize is the Image's sourceSize, which is 0 on any dimension that is not known yet -
    // e.g. while a delegate's geometry is still being resolved. QSize::isValid() accepts a 0, so
    // test for a genuinely usable size: generating at a 0 size returns a null image, which
    // QQuickImage then reports as a provider failure for every single delegate.
    // The size is also unbounded from here and gets scaled by the devicePixelRatio, so clamp it:
    // whatever we generate is cached for the whole session.
    constexpr int maxSize = BrickLink::Color::maxSampleImageSize;
    const bool haveSize = !requestedSize.isEmpty();
    const QSize wantedSize = haveSize ? requestedSize.boundedTo({ maxSize, maxSize }) : QSize { };

    const QString route = id.section(u'/', 0, 0);
    QImage image;

    if (route == u"picture") {
        const QString itemTypeId = id.section(u'/', 2, 2);
        const QByteArray itemId = id.section(u'/', 4).toLatin1();

        if (!itemTypeId.isEmpty() && !itemId.isEmpty()) {
            const auto *item = BrickLink::core()->item(itemTypeId.at(0).toLatin1(), itemId);
            const auto *color = BrickLink::core()->color(id.section(u'/', 3, 3).toUInt());

            // Only ever shows what is in the cache already: QmlPicture holds a reference to the
            // Picture and only hands out a URL once there is an image, so this cannot miss.
            if (auto pic = BrickLink::core()->pictureCache()->cachedPicture(item, color))
                image = pic->image();
        }
    } else if (route == u"color") {
        // Swatches are generated at the exact requested size, because the glitter and speckle
        // patterns do not survive scaling. Call sites should set sourceSize.
        const QSize swatchSize = haveSize ? wantedSize : QSize { 32, 32 };
        if (const auto *color = BrickLink::core()->color(id.section(u'/', 1, 1).toUInt()))
            image = color->sampleImage(swatchSize.width(), swatchSize.height());
    } else if (route == u"noimage") {
        // The icon is an SVG, so any size renders crisply: pick something that also looks good
        // when scaled up into a full-screen info widget.
        image = BrickLink::core()->noImage(haveSize ? wantedSize : QSize { 256, 256 });
    } else {
        qCWarning(LogQml) << "QmlImageProvider: cannot serve unknown image id" << id;
    }

    if (size)
        *size = image.size();
    return image;
}
