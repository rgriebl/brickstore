/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
        m_pic->load_from_disk();

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
            pic->load_from_disk();

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
    auto f = core()->itemImageScaleFactor();
    if (qFuzzyCompare(f, 1))
        return m_image;
    else
        return m_image.scaled(m_image.size() * f, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
                image = BrickLink::core()->noImage(m_item->itemType()->rawPictureSize());

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
        url = "https://www.bricklink.com/getPic.asp";
        // ?itemType=%c&colorID=%d&itemNo=%s", pic->item ( )->itemType ( )->pictureId ( ), pic->color ( )->id ( ), pic->item ( )->id ( ));
        QUrlQuery query;
        query.addQueryItem("itemType", QChar(pic->item()->itemType()->pictureId()));
        query.addQueryItem("colorID",  QString::number(pic->color()->id()));
        query.addQueryItem("itemNo",   pic->item()->id());
        url.setQuery(query);
    }

    //qDebug ( "PIC request started for %s", (const char *) url );
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
        QString path;
        QImage img;

        if (!large && pic->item()->itemType()->hasColors())
            path = BrickLink::core()->dataPath(pic->item(), pic->color());
        else
            path = BrickLink::core()->dataPath(pic->item());

        if (!path.isEmpty()) {
            path.append(large ? "large.png" : "small.png");

            pic->m_update_status = UpdateStatus::Ok;

            // qWarning() << "IMG" << j->data()->size() << j->effectiveUrl();

            if ((j->effectiveUrl().path().indexOf("noimage", 0, Qt::CaseInsensitive) == -1)
                    && !j->data()->isEmpty()
                    && img.loadFromData(*j->data())) {
                img.save(path, "PNG");
            }
            else {
                QFile f(path);
                f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                f.close();

                // qWarning("No image !");
            }

            pic->load_from_disk();
        }
        else {
            qWarning("Couldn't get path to save image");
        }
    }
    else if (large && (j->responseCode() == 404) && (j->url().path().endsWith(".jpg"))) {
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

