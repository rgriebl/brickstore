// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>

#include <QtCore/QDateTime>
#include <QtGui/QImage>
#include <QtQml/qqmlregistration.h>

#include "global.h"

class TransferJob;


namespace BrickLink {

class PictureCache;
class Picture;

// Pictures live in the PictureCache, but they are shared: whoever displays one keeps it alive for as
// long as it is needed, whether it is still cached or not. Always hold on to a picture via a
// PictureRef, never via a raw pointer.
using PictureRef = std::shared_ptr<Picture>;

class Picture : public QObject, public std::enable_shared_from_this<Picture>
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(const BrickLink::Item *item READ item CONSTANT FINAL)
    Q_PROPERTY(const BrickLink::Color *color READ color CONSTANT FINAL)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged FINAL)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged FINAL)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(QImage image READ image NOTIFY imageChanged FINAL)

    struct Private { };

public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    Q_INVOKABLE void update(bool highPriority = false);
    QDateTime lastUpdated() const      { return m_lastUpdated; }
    Q_INVOKABLE void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_updateStatus; }

    const QImage image() const;

    int cost() const;

    Picture(Private, const Item *item, const Color *color);
    ~Picture() override;
    Q_DISABLE_COPY_MOVE(Picture)

    // The QML API cannot hold a PictureRef, so it still has to pin its pictures by hand. Both of
    // these go away once QML gets a handle owning element of its own.
    Q_INVOKABLE void addRef();
    Q_INVOKABLE void release();

signals:
    void isValidChanged(bool newIsValid);
    void lastUpdatedChanged(const QDateTime &newLastUpdated);
    void updateStatusChanged(BrickLink::UpdateStatus newUpdateStatus);
    void imageChanged(const QImage &newImage);

private:
    const Item * m_item;
    const Color *m_color;

    QDateTime    m_lastUpdated;

    bool         m_valid           : 1 = false;
    bool         m_updateAfterLoad : 1 = false;
    UpdateStatus m_updateStatus    : 3 = UpdateStatus::Ok;
    uint         m_qmlPinCount     : 27 = 0;

    TransferJob *m_transferJob = nullptr;

    QImage       m_image;
    PictureRef   m_qmlPin; // see addRef() - a deliberate self reference, owned by the QML API

    static PictureCache *s_cache;

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

Q_DECLARE_METATYPE(BrickLink::Picture *)
// std::shared_ptr, unlike QSharedPointer, has no automatic metatype: needed for TransferJob's
// user data, which is what keeps a picture alive while it is being downloaded.
Q_DECLARE_METATYPE(BrickLink::PictureRef)
