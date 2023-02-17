// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QDateTime>
#include <QtGui/QImage>
#include <QtQml/qqmlregistration.h>

#include "global.h"
#include "utility/ref.h"

class TransferJob;


namespace BrickLink {

class PictureCache;

class Picture : public QObject, protected Ref
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

    Picture(std::nullptr_t) : Picture(nullptr, nullptr) { } // for scripting only!
    ~Picture() override;

    Q_INVOKABLE void addRef() { Ref::addRef(); }
    Q_INVOKABLE void release() { Ref::release(); }
    Q_INVOKABLE int refCount() const { return Ref::refCount(); }

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
    uint         m_reserved        : 27 = 0;

    TransferJob *m_transferJob = nullptr;

    QImage       m_image;

    static PictureCache *s_cache;

private:
    Picture(const Item *item, const Color *color);

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

    Picture *picture(const Item *item, const Color *color, bool highPriority = false);

    void updatePicture(Picture *pic, bool highPriority = false);
    void cancelPictureUpdate(Picture *pic);

signals:
    void pictureUpdated(BrickLink::Picture *pic);

private:
    PictureCachePrivate *d;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Picture *)


// tell Qt that Pictures are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::Picture>(BrickLink::Picture &c) { return c.refCount() == 0; }
