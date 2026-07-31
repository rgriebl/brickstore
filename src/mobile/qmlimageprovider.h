// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtQuick/QQuickImageProvider>


// Serves all BrickLink images to QML, so that a plain Image can be used instead of a custom item:
// QQuickPixmapCache then shares one decoded image and one texture per URL between all Images
// showing the same picture, which matters a lot for the item grids and tables.
//
// Registered as "bricklink" (see BrickLink::QmlPicture::imageProviderId), understanding these ids:
//   picture/<lastUpdated>/<itemTypeId>/<colorId>/<itemId>   as built by QmlPicture::imageUrl()
//   color/<colorId>                                         a Color::sampleImage() swatch
//   noimage                                                 the "no image available" placeholder
//
// This is a synchronous provider without ForceAsynchronousImageLoading, so requestImage() runs on
// the GUI thread as long as the Image is not marked asynchronous - which it must not be, because
// BrickLink::Core and its caches are not thread-safe.

class QmlImageProvider : public QQuickImageProvider
{
public:
    QmlImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
