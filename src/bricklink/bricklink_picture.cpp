/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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

#include "threadpool.h"

#include "bricklink.h"

// directly use C-library str* functions instead of Qt's qstr* function
// to improve runtime performance

#if defined(Q_OS_WIN)
#  define strdup _strdup
#endif

namespace BrickLink {

class PictureLoaderJob : public ThreadPoolJob {
public:
    PictureLoaderJob(Picture *pic)
        : ThreadPoolJob(), m_pic(pic)
    { }

    virtual void run()
    {
        if (m_pic && !m_pic->valid())
            m_pic->load_from_disk();
    }

    Picture *picture() const
    {
        return m_pic;
    }

private:
    Picture *m_pic;
};

}

const QPixmap BrickLink::Picture::pixmap() const
{
    QPixmap p;
    QString k = key();

    if (!QPixmapCache::find(k, p)) {
        p = QPixmap::fromImage(m_image);
        QPixmapCache::insert(k, p);
    }
    return p;
}

QSize BrickLink::Core::pictureSize(const ItemType *itt) const
{
    if (!itt)
        itt = itemType('P');
    return itt ? itt->pictureSize() : QSize();
}

BrickLink::Picture *BrickLink::Core::picture(const Item *item, const BrickLink::Color *color, bool high_priority)
{
    if (!item)
        return 0;

    quint64 key = quint64(color ? color->id() : uint(-1)) << 32 | quint64(item->itemType()->id()) << 24 | quint64(item->index() + 1);

    QMutexLocker lock(&m_corelock);

    Picture *pic = m_pic_cache[key];

    bool need_to_load = false;

    if (!pic) {
        pic = new Picture(item, color);
        if (!m_pic_cache.insert(key, pic, pic->cost())) {
            qWarning("Can not add picture to cache (cache max/cur: %d/%d, cost: %d)", m_pic_cache.maxCost(), m_pic_cache.totalCost(), pic->cost());
            return 0;
        }
        need_to_load = true;
    }

    if (high_priority) {
        if (!pic->valid())
            pic->load_from_disk();

        if (updateNeeded(pic->valid(), pic->lastUpdate(), m_pic_update_iv))
            updatePicture(pic, high_priority);
    }
    else if (need_to_load) {
        pic->addRef();
        m_pic_diskload.execute(new PictureLoaderJob(pic));
    }

    return pic;
}

BrickLink::Picture *BrickLink::Core::largePicture(const Item *item, bool high_priority)
{
    if (!item)
        return 0;
    return picture(item, 0, high_priority);
}

BrickLink::Picture::Picture(const Item *item, const Color *color)
{
    m_item = item;
    m_color = color;

    m_valid = false;
    m_update_status = Ok;
}

BrickLink::Picture::~Picture()
{
    QPixmapCache::remove(key());
}

int BrickLink::Picture::cost() const
{
    if (m_color)
        return 80*60*2 + 128;  // 80x60 16bpp + data
    else
        return 640*480*4 / 2;    // max. 640*480 32bpp + data, most are smaller
}

void BrickLink::Picture::load_from_disk()
{
    QString path;

    bool large = (!m_color);

    if (!large && m_item->itemType()->hasColors())
        path = BrickLink::core()->dataPath(m_item, m_color);
    else
        path = BrickLink::core()->dataPath(m_item);

    if (path.isEmpty())
        return;
    path += large ? "large.png" : "small.png";

    QFileInfo fi(path);

    bool is_valid = false;
    QImage image;
    QDateTime lastmod;

    if (fi.exists()) {
        if (fi.size() > 0) {
            is_valid = image.load(path);
        }
        else {
            if (!large && m_item && m_item->itemType())
                image = BrickLink::core()->noImage(m_item->itemType()->pictureSize());

            is_valid = true;
        }

        lastmod = fi.lastModified();
    }
    else
        is_valid = false;

    // this will decrease the image a quality a bit (8bit/channel -> 5bit/channel),
    // but it will only use half the ram (read: you can cache twice as many images)
    if (is_valid && !large && image.depth() > 16)
        image = image.convertToFormat(QImage::Format_RGB16);

    if (is_valid) {
        m_image = image;
        m_fetched = lastmod;
    }
    m_valid = is_valid;
}


void BrickLink::Picture::update(bool high_priority)
{
    BrickLink::core()->updatePicture(this, high_priority);
}

void BrickLink::Core::pictureLoaded(ThreadPoolJob *pj)
{
    Picture *pic = static_cast<PictureLoaderJob *>(pj)->picture();

   if (pic) {
        if (updateNeeded(pic->valid(), pic->lastUpdate(), m_pic_update_iv))
            updatePicture(pic, false);
        else
            emit pictureUpdated(pic);

        QPixmapCache::remove(pic->key());
        pic->release();
    }
}

void BrickLink::Core::updatePicture(BrickLink::Picture *pic, bool high_priority)
{
    if (!pic || (pic->m_update_status == Updating))
        return;

    QMutexLocker lock(&m_corelock);

    if (!m_online || !m_transfer) {
        pic->m_update_status = UpdateFailed;
        emit pictureUpdated(pic);
        return;
    }

    pic->m_update_status = Updating;
    pic->addRef();

    bool large = (!pic->color());

    QUrl url;

    if (large) {
        url = QString("http://www.bricklink.com/%1L/%2.jpg").arg(pic->item()->itemType()->pictureId()).arg(pic->item()->id());
    }
    else {
        url = "http://www.bricklink.com/getPic.asp";
        // ?itemType=%c&colorID=%d&itemNo=%s", pic->item ( )->itemType ( )->pictureId ( ), pic->color ( )->id ( ), pic->item ( )->id ( ));

        url.addQueryItem("itemType", QChar(pic->item()->itemType()->pictureId()));
        url.addQueryItem("colorID",  QString::number(pic->color()->id()));
        url.addQueryItem("itemNo",   pic->item()->id());
    }

    //qDebug ( "PIC request started for %s", (const char *) url );
    TransferJob *job = TransferJob::get(url);
    job->setUserData<Picture>('P', pic);
    m_transfer->retrieve(job, high_priority);
}


void BrickLink::Core::pictureJobFinished(ThreadPoolJob *pj)
{
    TransferJob *j = static_cast<TransferJob *>(pj);

    if (!j || !j->data())
        return;

    Picture *pic = j->userData<Picture>('P');

    if (!pic)
        return;

    bool large = (!pic->color());

    QMutexLocker lock(&m_corelock);

    pic->m_update_status = UpdateFailed;

    if (j->isCompleted()) {
        QString path;
        QImage img;

        if (!large && pic->item()->itemType()->hasColors())
            path = BrickLink::core()->dataPath(pic->item(), pic->color());
        else
            path = BrickLink::core()->dataPath(pic->item());

        if (!path.isEmpty()) {
            path.append(large ? "large.png" : "small.png");

            pic->m_update_status = Ok;

            if ((j->effectiveUrl().path().indexOf("noimage", 0, Qt::CaseInsensitive) == -1) && j->data()->size() && img.loadFromData(*j->data())) {
                img.save(path, "PNG");
            }
            else {
                QFile f(path);
                f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                f.close();

                qWarning("No image !");
            }

            pic->load_from_disk();
            QPixmapCache::remove(pic->key());
        }
        else {
            qWarning("Couldn't get path to save image");
        }
    }
    else if (large && (j->responseCode() == 404) && (j->url().path().endsWith(".jpg"))) {
        // no large JPG image ->try a GIF image instead

        if (!m_transfer) {
            pic->m_update_status = UpdateFailed;
            return;
        }

        pic->m_update_status = Updating;

        QUrl url = j->url();
        QString path = url.path();
        path.chop(3);
        path.append("gif");
        url.setPath(path);

        //qDebug ( "PIC request started for %s", (const char *) url );
        TransferJob *job = TransferJob::get(url);
        job->setUserData<Picture>('P', pic);
        m_transfer->retrieve(job);
        return;
    }
    else
        qWarning("Image download failed");

    emit pictureUpdated(pic);
    pic->release();
}

