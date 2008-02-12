/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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
#include <stdlib.h>

#include <QFileinfo>
#include <QTimer>
#include <QPixmapCache>
#include <QImage>

#include "bricklink.h"

// directly use C-library str* functions instead of Qt's qstr* function
// to improve runtime performance

#if defined( Q_OS_WIN32 )
#define strdup _strdup
#endif



const QPixmap BrickLink::Picture::pixmap() const
{
    QPixmap p;
    QString key (QString::number(m_image.cacheKey()));

    if (!QPixmapCache::find(key, p)) {
        p = QPixmap::fromImage(m_image);
        QPixmapCache::insert(key, p);
    }
    return p;
}

QSize BrickLink::Core::pictureSize(const ItemType *itt) const
{
    return itt ? itt->pictureSize() : QSize();
}

BrickLink::Picture *BrickLink::Core::picture(const Item *item, const BrickLink::Color *color, bool high_priority)
{
    if (!item)
        return 0;

    quint64 key = 0;
    Picture *pic = 0;


    if (color) {
        key = quint64(color ? color->id() : -1) << 32 | quint64(item->itemType()->id()) << 24 | quint64(item->index() + 1);
        pic = m_pictures.cache [key];
    }

    bool need_to_load = false;

    if (!pic) {
        pic = new Picture(item, color);
        if (color)
            m_pictures.cache.insert(key, pic);
        need_to_load = true;
    }

    if (high_priority || need_to_load) {
        if (!pic->valid())
            pic->load_from_disk();

        if (!pic->valid() || updateNeeded(pic->lastUpdate(), m_pictures.update_iv))
            updatePicture(pic, high_priority);
    }
    else if (false) {
        pic->addRef();
        m_pictures.diskload.append(pic);

        if (m_pictures.diskload.count() == 1)
            QTimer::singleShot(0, BrickLink::inst(), SLOT(pictureIdleLoader()));
    }

    return pic;
}

BrickLink::Picture *BrickLink::Core::largePicture(const Item *item, bool high_priority)
{
    if (!item)
        return 0;
    return picture(item, 0, high_priority);
}

// loading pictures directly when creating Picture objects locks
// up the machine.Loading only one per idle loop generates nearly
// NO cpu usage, but it is very slow.
// Loading 3 pictures per idle loop is a good compromise.

void BrickLink::Core::pictureIdleLoader()
{
    for (int i = 0; i < 3; i++)
        pictureIdleLoader2();

    if (!m_pictures.diskload.isEmpty())
        QTimer::singleShot(0, this, SLOT(pictureIdleLoader()));
}

void BrickLink::Core::pictureIdleLoader2()
{
    Picture *pic = 0;

    while (!m_pictures.diskload.isEmpty()) {
        pic = m_pictures.diskload.takeFirst();

        if (!pic) {
            continue;
        }
        // already loaded? ..or ..nobody listening?
        else if (pic->valid() || (pic->refCount() == 1)) {
            pic->release();
            pic = 0;
            continue;
        }
        else {
            break;
        }
    }

    if (pic) {
        pic->load_from_disk();

        if (!pic->valid() || updateNeeded(pic->lastUpdate(), m_pictures.update_iv))
            updatePicture(pic, false);
        else
            emit pictureUpdated(pic);

        pic->release();
    }
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

void BrickLink::Picture::load_from_disk()
{
    QString path;

    bool large = (!m_color);

    if (!large && m_item->itemType()->hasColors())
        path = BrickLink::inst()->dataPath(m_item, m_color);
    else
        path = BrickLink::inst()->dataPath(m_item);

    if (path.isEmpty())
        return;
    path += large ? "large.png" : "small.png";

    QFileInfo fi(path);

    if (fi.exists()) {
        if (fi.size() > 0) {
            m_valid = m_image.load(path);
        }
        else {
            if (!large && m_item && m_item->itemType())
                m_image = BrickLink::inst()->noImage(m_item->itemType()->pictureSize())->toImage();
            else
                m_image = QImage();

            m_valid = true;
        }

        m_fetched = fi.lastModified();
    }
    else
        m_valid = false;

    if (m_valid && !large && m_image.depth() > 8) {
        // Qt4 always tries to dither when converting images with alpha channel
        //m_image = m_image.convertToFormat ( QImage::Format_Indexed8 );
    }

    QPixmapCache::remove(QString::number(m_image.cacheKey()));
}


void BrickLink::Picture::update(bool high_priority)
{
    BrickLink::inst()->updatePicture(this, high_priority);
}

void BrickLink::Core::updatePicture(BrickLink::Picture *pic, bool high_priority)
{
    if (!pic || (pic->m_update_status == Updating))
        return;

    if (!m_online) {
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
    CTransferJob *job = CTransferJob::get(url);
    job->setUserData<Picture>(pic);
    m_pictures.transfer->retrieve(job, high_priority);
}


void BrickLink::Core::pictureJobFinished(CTransferJob *j)
{
    if (!j || !j->data() || !j->userData<Picture>())
        return;

    Picture *pic = j->userData<Picture>();
    bool large = (!pic->color());

    pic->m_update_status = UpdateFailed;

    if (j->isCompleted()) {
        QString path;
        QImage img;

        if (!large && pic->item()->itemType()->hasColors())
            path = BrickLink::inst()->dataPath(pic->item(), pic->color());
        else
            path = BrickLink::inst()->dataPath(pic->item());

        if (!path.isEmpty()) {
            path.append(large ? "large.png" : "small.png");

            pic->m_update_status = Ok;

            if ((j->effectiveUrl().path().indexOf("noimage", 0, Qt::CaseInsensitive) == -1) && j->data()->size() && img.loadFromData(*j->data())) {
                if (!large) {
                    // Qt4 always tries to dither when converting images with alpha channel
                    //img = img.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither | Qt::ThresholdAlphaDither | Qt::AvoidDither);
                }
                img.save(path, "PNG");
            }
            else {
                QFile f(path);
                f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                f.close();

                qWarning("No image !");
            }

            pic->load_from_disk();
        }
        else {
            qWarning("Couldn't get path to save image");
        }
    }
    else if (large && (j->responseCode() == 404) && (j->url().path().endsWith(".jpg"))) {
        // no large JPG image ->try a GIF image instead

        pic->m_update_status = Updating;

        QUrl url = j->url();
        QString path = url.path();
        path.chop(3);
        path.append("gif");
        url.setPath(path);

        //qDebug ( "PIC request started for %s", (const char *) url );
        CTransferJob *job = CTransferJob::get(url);
        job->setUserData<Picture>(pic);
        m_pictures.transfer->retrieve(job);
        return;
    }
    else
        qWarning("Image download failed");

    emit pictureUpdated(pic);
    pic->release();
}

