/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include <cstdlib>

#include <QFileInfo>
#include <QTimer>
#include <QPixmapCache>
#include <QImage>
#include <QUrlQuery>
#include <QRunnable>

#include "bricklink.h"


namespace BrickLink {

class PictureLoaderJob : public QRunnable
{
public:
    explicit PictureLoaderJob(Picture *pic)
        : QRunnable()
        , m_pic(pic)
    {
        pic->m_update_status = UpdateStatus::Loading;
    }

    void run() override;

    Picture *picture() const
    {
        return m_pic;
    }

private:
    Q_DISABLE_COPY(PictureLoaderJob)

    Picture *m_pic;
};

void PictureLoaderJob::run()
{
    if (m_pic) {
        QDateTime fetched;
        QImage image;
        bool valid = m_pic->loadFromDisk(fetched, image);
        auto pic = m_pic;
        QMetaObject::invokeMethod(BrickLink::core(), [=]() {
            pic->m_valid = valid;
            pic->m_update_status = UpdateStatus::Ok;
            if (valid) {
                pic->m_fetched = fetched;
                pic->m_image = image;
            }
            BrickLink::core()->pictureLoaded(pic);
        }, Qt::QueuedConnection);
    }
}

}

QSize BrickLink::Core::standardPictureSize() const
{
    QSize s(80, 60);
    qreal f = BrickLink::core()->itemImageScaleFactor();
    if (!qFuzzyCompare(f, 1))
        s *= f;
    return s;
}

BrickLink::Picture *BrickLink::Core::picture(const Item *item, const BrickLink::Color *color, bool highPriority)
{
    if (!item)
        return nullptr;

    quint64 key = quint64(color ? color->id() : uint(-1)) << 32 | quint64(item->itemType()->id()) << 24 | quint64(item->index() + 1);

    Picture *pic = m_pic_cache[key];

    bool needToLoad = false;

    if (!pic) {
        pic = new Picture(item, color);
        if (!m_pic_cache.insert(key, pic, pic->cost())) {
            qWarning("Can not add picture to cache (cache max/cur: %d/%d, item: %s)",
                     m_pic_cache.maxCost(), m_pic_cache.totalCost(), qPrintable(item->id()));
            return nullptr;
        }
        needToLoad = true;
    }

    if (highPriority) {
        if (!pic->isValid()) {
            pic->m_valid = pic->loadFromDisk(pic->m_fetched, pic->m_image);
            pic->m_update_status = UpdateStatus::Ok;
        }

        if (updateNeeded(pic->isValid(), pic->lastUpdate(), m_pic_update_iv))
            updatePicture(pic, highPriority);

    } else if (needToLoad) {
        pic->addRef();
        m_diskloadPool.start(new PictureLoaderJob(pic));
    }

    return pic;
}

BrickLink::Picture *BrickLink::Core::largePicture(const Item *item, bool high_priority)
{
    if (!item)
        return nullptr;
    return picture(item, nullptr, high_priority);
}

BrickLink::Picture::Picture(const Item *item, const Color *color)
    : m_item(item)
    , m_color(color)
{
    m_valid = false;
    m_updateAfterLoad = false;
    m_update_status = UpdateStatus::Ok;
}

BrickLink::Picture::~Picture()
{
    cancelUpdate();
}

const QImage BrickLink::Picture::image() const
{
    return m_image;
}

int BrickLink::Picture::cost() const
{
    if (m_color)
        return 80*60*4 + 128;  // 80x60 32bpp + data
    else
        return 640*480*4;      // ~ 640*480 32bpp
}

QFile *BrickLink::Picture::file(QIODevice::OpenMode openMode) const
{
    bool large = (!m_color);
    bool hasColors = m_item->itemType()->hasColors();

    return BrickLink::core()->dataFile(large ? u"large.jpg" : u"normal.png", openMode,
                                       m_item, (!large && hasColors) ? m_color : nullptr);
}

bool BrickLink::Picture::loadFromDisk(QDateTime &fetched, QImage &image)
{
    if (!m_item)
        return false;

    QScopedPointer<QFile> f(file(QIODevice::ReadOnly));

    bool isValid = false;

    if (f && f->isOpen()) {
        if (f->size() > 0) {
            QByteArray ba = f->readAll();

            // optimize loading when a lot of QImageIO plugins are available
            // (e.g. when building against Qt from a Linux distro)
            if (ba.startsWith("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"))
                isValid = image.loadFromData(ba, "PNG");
            else if (ba.startsWith("GIF8"))
                isValid = image.loadFromData(ba, "GIF");
            else if (ba.startsWith("\xFF\xD8\xFF"))
                isValid = image.loadFromData(ba, "JPG");
            if (!isValid)
                isValid = image.loadFromData(ba);
        }
        fetched = f->fileTime(QFileDevice::FileModificationTime);
    }
    return isValid;
}

void BrickLink::Picture::update(bool highPriority)
{
    BrickLink::core()->updatePicture(this, highPriority);
}

void BrickLink::Picture::cancelUpdate()
{
    if (BrickLink::core())
        BrickLink::core()->cancelPictureUpdate(this);
}

void BrickLink::Core::pictureLoaded(Picture *pic)
{
    if (pic) {
        if (pic->m_updateAfterLoad
                || updateNeeded(pic->isValid(), pic->lastUpdate(), m_pic_update_iv)) {
            pic->m_updateAfterLoad = false;
            updatePicture(pic, false);
        }
        emit pictureUpdated(pic);
        pic->release();
    }
}

void BrickLink::Core::updatePicture(BrickLink::Picture *pic, bool highPriority)
{
    if (!pic || (pic->m_update_status == UpdateStatus::Updating))
        return;

    if (!m_online || !m_transfer) {
        pic->m_update_status = UpdateStatus::UpdateFailed;
        emit pictureUpdated(pic);
        return;
    }

    if (pic->m_update_status == UpdateStatus::Loading) {
        pic->m_updateAfterLoad = true;
        return;
    }

    pic->m_update_status = UpdateStatus::Updating;
    pic->addRef();

    bool large = (!pic->color());

    QUrl url;

    if (large) {
        url = QString("https://img.bricklink.com/%1L/%2.jpg").arg(pic->item()->itemType()->pictureId()).arg(pic->item()->id());
    }
    else {
        url = QString("https://img.bricklink.com/ItemImage/%1N/%3/%2.png").arg(pic->item()->itemType()->pictureId()).arg(pic->item()->id()).arg(pic->color()->id());
    }

    //qDebug() << "PIC request started for" << url;
    QFile *f = pic->file(QIODevice::WriteOnly | QIODevice::Truncate);
    pic->m_transferJob = TransferJob::get(url, f);
    pic->m_transferJob->setUserData<Picture>('P', pic);
    m_transfer->retrieve(pic->m_transferJob, highPriority);
}

void BrickLink::Core::cancelPictureUpdate(BrickLink::Picture *pic)
{
    if (pic->m_transferJob)
        m_transfer->abortJob(pic->m_transferJob);
}


void BrickLink::Core::pictureJobFinished(TransferJob *j)
{
    if (!j)
        return;
    Picture *pic = j->userData<Picture>('P');
    if (!pic)
        return;

    pic->m_transferJob = nullptr;
    bool large = (!pic->color());

    if (j->isCompleted() && j->file()) {
        j->file()->close();

        // the pic is still ref'ed, so we just forward it to the loader
        pic->m_update_status = UpdateStatus::Loading;
        m_diskloadPool.start(new PictureLoaderJob(pic));
        return;

    } else if (large && (j->responseCode() == 404) && (j->url().path().endsWith(".jpg"))) {
        // There's no large JPG image, so try a GIF image instead (mostly very old sets)
        // We save the GIF with an JPG extension if we succeed, but Qt uses the file header on
        // loading to do the right thing.

        if (!m_transfer) {
            pic->m_update_status = UpdateStatus::UpdateFailed;
        } else {
            pic->m_update_status = UpdateStatus::Updating;

            QUrl url = j->url();
            url.setPath(url.path().replace(".jpg", ".gif"));

            QFile *f = pic->file(QIODevice::WriteOnly | QIODevice::Truncate);
            TransferJob *job = TransferJob::get(url, f);
            job->setUserData<Picture>('P', pic);
            m_transfer->retrieve(job);

            // the pic is still ref'ed: leave it that way for one more loop
            return;
        }
    } else {
        pic->m_update_status = UpdateStatus::UpdateFailed;

        qWarning() << "Image download failed:" << j->errorString() << "(" << j->responseCode() << ")";
    }

    emit pictureUpdated(pic);
    pic->release();
}

