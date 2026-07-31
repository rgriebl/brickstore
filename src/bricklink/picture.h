// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>

#include <QtCore/QDateTime>
#include <QtGui/QImage>

#include "global.h"

class TransferJob;


namespace BrickLink {

class PictureCache;
class Picture;

// Pictures live in the PictureCache, but they are shared: whoever displays one keeps it alive for as
// long as it is needed, whether it is still cached or not. Always hold on to a picture via a
// PictureRef, never via a raw pointer.
using PictureRef = std::shared_ptr<Picture>;

// Not exposed to QML: BrickLink::QmlPicture is the QML facing half, as QML needs an object whose
// lifetime it can manage in order to hold on to a reference at all.
class Picture : public QObject
{
    Q_OBJECT

    struct Private { };

public:
    // Both return nullptr once the database has been swapped out from under us: the pointers are
    // raw and the objects they pointed at are long gone by then. See isStale().
    const Item *item() const;
    const Color *color() const;
    QDateTime lastUpdated() const     { return m_lastUpdated; }

    // A picture outlives its cache entry whenever someone still holds a reference, and a database
    // update can happen in between. Such a picture cannot be used for anything anymore - it is not
    // even possible to tell which item it belonged to.
    bool isStale() const;

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_updateStatus; }

    const QImage image() const;

    int cost() const;

    Picture(Private, const Item *item, const Color *color);
    ~Picture() override;
    Q_DISABLE_COPY_MOVE(Picture)

signals:
    void isValidChanged(bool newIsValid);
    void lastUpdatedChanged(const QDateTime &newLastUpdated);
    void updateStatusChanged(BrickLink::UpdateStatus newUpdateStatus);
    void imageChanged(const QImage &newImage);

private:
    const Item * m_item;
    const Color *m_color;

    QDateTime    m_lastUpdated;

    quint32      m_generation      = 0; // of the database the item/color pointers point into

    bool         m_valid           : 1 = false;
    bool         m_updateAfterLoad : 1 = false;
    UpdateStatus m_updateStatus    : 3 = UpdateStatus::Ok;
    uint         m_reserved        : 27 = 0;

    TransferJob *m_transferJob = nullptr;

    QImage       m_image;

private:
    void setIsValid(bool valid);
    void setUpdateStatus(UpdateStatus status);
    void setLastUpdated(const QDateTime &dt);
    void setImage(const QImage &newImage);

    friend class PictureCache;
    friend class PictureCachePrivate;
};

class PictureCachePrivate;

class PictureCache : public QObject
{
    Q_OBJECT

public:
    explicit PictureCache(Core *core, quint64 physicalMem);
    ~PictureCache() override;

    void setUpdateInterval(int interval);
    void clearCache();
    QPair<int, int> cacheStats() const;

    PictureRef picture(const Item *item, const Color *color, bool highPriority = false);

    // Unlike picture(), this neither creates a cache entry, nor starts a download. A stale picture
    // counts as a miss: it cannot be attributed to an item anymore.
    PictureRef cachedPicture(const Item *item, const Color *color);

    void updatePicture(const PictureRef &pic, bool highPriority = false);
    void cancelPictureUpdate(const PictureRef &pic);
    void cancelAllPictureUpdates();

signals:
    // carries a reference, so a slot cannot be handed a picture that dies while it runs
    void pictureUpdated(const BrickLink::PictureRef &pic);

private:
    PictureCachePrivate *d;
};

} // namespace BrickLink

// std::shared_ptr, unlike QSharedPointer, has no automatic metatype: needed for TransferJob's
// user data, which is what keeps a picture alive while it is being downloaded.
Q_DECLARE_METATYPE(BrickLink::PictureRef)
