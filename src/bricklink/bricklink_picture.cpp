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
        : QRunnable(), m_pic(pic)
    { }

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
    if (m_pic && !m_pic->valid()) {
        m_pic->loadFromDisk();

        auto blcore = BrickLink::core();
        auto pic = m_pic;
        QMetaObject::invokeMethod(blcore, [pic, blcore]() {
            blcore->pictureLoaded(pic);
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

BrickLink::Picture *BrickLink::Core::picture(const Item *item, const BrickLink::Color *color, bool high_priority)
{
    if (!item)
        return nullptr;

    quint64 key = quint64(color ? color->id() : uint(-1)) << 32 | quint64(item->itemType()->id()) << 24 | quint64(item->index() + 1);

    QMutexLocker lock(&m_corelock);

    Picture *pic = m_pic_cache[key];

    bool need_to_load = false;

    if (!pic) {
        pic = new Picture(item, color);
        if (!m_pic_cache.insert(key, pic, pic->cost())) {
            qWarning("Can not add picture to cache (cache max/cur: %d/%d, item: %s)",
                     m_pic_cache.maxCost(), m_pic_cache.totalCost(), qPrintable(item->id()));
            return nullptr;
        }
        need_to_load = true;
    }

    if (high_priority) {
        if (!pic->valid())
            pic->loadFromDisk();

        if (updateNeeded(pic->valid(), pic->lastUpdate(), m_pic_update_iv))
            updatePicture(pic, high_priority);
    }
    else if (need_to_load) {
        pic->addRef();
        m_pic_diskload.start(new PictureLoaderJob(pic));
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
{
    m_item = item;
    m_color = color;

    m_valid = false;
    m_update_status = UpdateStatus::Ok;
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

    return BrickLink::core()->dataFile(large ? u"large.png" : u"small.png", openMode,
                                       m_item, (!large && hasColors) ? m_color : nullptr);
}

void BrickLink::Picture::loadFromDisk()
{
    if (!m_item)
        return;

    QScopedPointer<QFile> f(file(QIODevice::ReadOnly));

    bool isValid = false;
    QDateTime lastModified;
    QImage image;

    if (f && f->isOpen()) {
        if (f->size() > 0) {
            isValid = image.load(f.data(), "PNG");
        } else {
            if (m_color && m_item && m_item->itemType())
                image = BrickLink::core()->noImage(m_item->itemType()->rawPictureSize());

            isValid = true;
        }
        lastModified = f->fileTime(QFileDevice::FileModificationTime);
    }

    if (isValid) {
        m_image = image;
        m_fetched = lastModified;
    }
    m_valid = isValid;
}

void BrickLink::Picture::update(bool high_priority)
{
    BrickLink::core()->updatePicture(this, high_priority);
}

void BrickLink::Core::pictureLoaded(Picture *pic)
{
   if (pic) {
        if (updateNeeded(pic->valid(), pic->lastUpdate(), m_pic_update_iv))
            updatePicture(pic, false);
        else
            emit pictureUpdated(pic);
        pic->release();
    }
}

void BrickLink::Core::updatePicture(BrickLink::Picture *pic, bool high_priority)
{
    if (!pic || (pic->m_update_status == UpdateStatus::Updating))
        return;

    QMutexLocker lock(&m_corelock);

    if (!m_online || !m_transfer) {
        pic->m_update_status = UpdateStatus::UpdateFailed;
        emit pictureUpdated(pic);
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
    TransferJob *job = TransferJob::get(url);
    job->setUserData<Picture>('P', pic);
    m_transfer->retrieve(job, high_priority);
}


void BrickLink::Core::pictureJobFinished(TransferJob *j)
{
    if (!j || !j->data())
        return;

    auto *pic = j->userData<Picture>('P');

    if (!pic)
        return;

    bool large = (!pic->color());

    QMutexLocker lock(&m_corelock);

    pic->m_update_status = UpdateStatus::UpdateFailed;

    if (j->isCompleted()) {
        QImage img;

        QScopedPointer<QFile> f(pic->file(QIODevice::WriteOnly | QIODevice::Truncate));

        if (f && f->isOpen()) {
            pic->m_update_status = UpdateStatus::Ok;

            if ((j->effectiveUrl().path().indexOf("noimage", 0, Qt::CaseInsensitive) == -1)
                    && !j->data()->isEmpty()
                    && img.loadFromData(*j->data())) {
                img.save(f.data(), "PNG");
            } else {
                // no image ... "write" a 0-byte file
            }
            f->reset();
            pic->loadFromDisk();
        } else {
            qWarning("Couldn't get path to save image");
        }
    } else if (large && (j->responseCode() == 404) && (j->url().path().endsWith(".jpg"))) {
        // no large JPG image ->try a GIF image instead

        if (!m_transfer) {
            pic->m_update_status = UpdateStatus::UpdateFailed;
            return;
        }

        pic->m_update_status = UpdateStatus::Updating;

        QUrl url = j->url();
        QString path = url.path();
        path.chop(3);
        path.append("gif");
        url.setPath(path);

        TransferJob *job = TransferJob::get(url);
        job->setUserData<Picture>('P', pic);
        m_transfer->retrieve(job);
        return;
    }
    else
        qWarning() << "Image download failed:" << j->errorString() << "/ url:" << j->effectiveUrl();

    emit pictureUpdated(pic);
    pic->release();
}

