/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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

#include <qfileinfo.h>
#include <qtimer.h>
#include <qpixmapcache.h>
#include <qpixmap.h>
#include <qtextstream.h>
#include <qjson_p.h>

#include "bricklink.h"

// directly use C-library str* functions instead of Qt's qstr* function
// to improve runtime performance

#if defined( Q_OS_WIN32 )
#define strdup _strdup
#endif

#define QUOTE(string)  _QUOTE(string)
#define _QUOTE(string) #string

const QPixmap BrickLink::Picture::pixmap() const
{
	QPixmap p;

    if (!QPixmapCache::find(m_key, p))
    {
        p.convertFromImage(m_image);
        QPixmapCache::insert(m_key, p);
    }

    return p;
}        

BrickLink::Picture *BrickLink::picture(const Item *item, const BrickLink::Color *color, bool high_priority)
{
    if (!item)
    {
        return 0;
    }

    QString key;
    if (color)
    {
        key.sprintf("%c@%d@%d", item->itemType()->pictureId(), item->index(), color->id());
    }
    else
    {
        key.sprintf("%c@%d", item->itemType()->pictureId(), item->index());
    }

    Picture *pic = m_pictures.cache[key];
    bool need_to_load = false;

    if (!pic)
    {
        pic = new Picture(item, color, key);

        if (color)
        {
            m_pictures.cache.insert(key, pic);
        }

        need_to_load = true;
    }

    if (high_priority)
    {
        if (!pic->valid())
        {
            pic->load_from_disk();
        }

        if (updateNeeded(pic->valid(), pic->lastUpdate(), m_pictures.update_iv))
        {
            addPictureToUpdate(pic, high_priority);
        }
    }
    else if (need_to_load)
    {
        pic->addRef();
        m_pictures.diskload.append(pic);

        if (m_pictures.diskload.count() == 1)
        {
            QTimer::singleShot(0, BrickLink::inst(), SLOT(pictureIdleLoader()));
        }
	}

    return pic;
}

BrickLink::Picture *BrickLink::largePicture(const Item *item, bool high_priority)
{
    if (!item)
    {
        return 0;
    }

    return picture(item, 0, high_priority);
}
void BrickLink::addPictureToUpdate(BrickLink::Picture *pic, bool flush)
{
    if (!pic || pic->m_update_status == Updating || m_pictures_to_update->contains(pic))
    {
        return;
    }

    if (!m_online)
    {
        pic->m_update_status = UpdateFailed;
        emit pictureUpdated(pic);
        return;
    }

    pic->m_update_status = Updating;
    pic->addRef();

    if (!pic->color())
    {
        // Immediately update large pictures
        QString url;
        url.sprintf("http://img.bricklink.com/%cL/%s.jpg", pic->item()->itemType()->pictureId(), pic->item()->id());
        updatePicture(pic, url, flush);
        return;
    }

    m_pictures_to_update->append(pic);

    if (flush || m_pictures_to_update->size() == 500)
    {
        updatePicturesTimeOut();
        return;
    }
    else if (!m_pictureTimer)
    {
        m_pictureTimer = new QTimer();
        connect(m_pictureTimer, SIGNAL(timeout()), this, SLOT(updatePicturesTimeOut()));
    }

    m_pictureTimer->start(500, true);
}

void BrickLink::updatePicturesTimeOut()
{
    if (m_pictureTimer)
    {
        delete m_pictureTimer;
        m_pictureTimer = 0;
    }

    updatePictures(m_pictures_to_update);
    m_pictures_to_update = new QVector<BrickLink::Picture *>();
}

// loading pictures directly when creating Picture objects locks
// up the machine. Loading only one per idle loop generates nearly
// NO cpu usage, but it is very slow.
// Loading 3 pictures per idle loop is a good compromise.

void BrickLink::pictureIdleLoader()
{
    for (int i = 0; i < 3; i++)
    {
        pictureIdleLoader2();
    }

    if (!m_pictures.diskload.isEmpty())
    {
        QTimer::singleShot(0, this, SLOT(pictureIdleLoader()));
    }
}

void BrickLink::pictureIdleLoader2()
{
    Picture *pic = 0;

    while (!m_pictures.diskload.isEmpty())
    {
        pic = m_pictures.diskload.takeAt(0);

        if (!pic)
        {
            continue;
        }
        else if (pic->valid() || pic->refCount() == 1)
        {
            // already loaded? .. or .. nobody listening?
            pic->release();
            pic = 0;
            continue;
        }
        else
        {
            break;
        }
    }

    if (pic)
    {
        pic->load_from_disk();

        if (updateNeeded(pic->valid(), pic->lastUpdate(), m_pictures.update_iv))
        {
            addPictureToUpdate(pic, false);
        }
        else
        {
            emit pictureUpdated(pic);
        }

        pic->release();
    }
}

BrickLink::Picture::Picture(const Item *item, const Color *color, const char *key)
{
    m_item = item;
    m_color = color;

    m_valid = false;
    m_update_status = Ok;
    m_key = key ? strdup(key) : 0;
}

BrickLink::Picture::~Picture()
{
    if (m_key)
    {
        QPixmapCache::remove(m_key);
        free(m_key);
    }
}

void BrickLink::Picture::load_from_disk()
{
    QString path;
    bool large = !m_color;

    if (!large && m_item->itemType()->hasColors())
    {
        path = BrickLink::inst()->dataPath(m_item, m_color);
    }
    else
    {
        path = BrickLink::inst()->dataPath(m_item);
    }

    if (path.isEmpty())
    {
        return;
    }

    path += large ? "large.png" : "small.png";

    QFileInfo fi(path);
    if (fi.exists())
    {
        if (fi.size() > 0)
        {
            m_valid = m_image.load(path);
        }
        else
        {
            if (!large && m_item && m_item->itemType())
            {
                m_image = BrickLink::inst()->noImage(m_item->itemType()->imageSize())->convertToImage();
            }
            else
            {
                m_image.create(0, 0, 32);
            }

            m_valid = true;
        }

        m_fetched = fi.lastModified();
    }
    else
    {
        m_valid = false;
    }

    QPixmapCache::remove(m_key);
}

void BrickLink::Picture::update(bool high_priority)
{
    BrickLink::inst()->addPictureToUpdate(this, high_priority);
}

void BrickLink::updatePictures(QVector<BrickLink::Picture *> *pictures, bool high_priority)
{
    QString url = "https://api.bricklink.com/api/affiliate/v1/item_image_list?use_default=false&api_key=" + QString( QUOTE(BS_BLAPIKEY) );

    QJsonArray json;
    foreach (Picture *pic, *pictures)
    {
        QJsonObject pgItem;
        QJsonObject item;
        item["no"] = QString(pic->item()->id());
        item["type"] = pic->item()->itemType()->apiName();
        pgItem["item"] = item;
        pgItem["color_id"] = (int)pic->color()->id();

        json.append(pgItem);
    }

    QJsonDocument doc(json);
    QString strJson(doc.toJson(QJsonDocument::Compact));

    m_pictures.transfer->postJson(url, strJson, pictures, high_priority);
}

void BrickLink::updatePicture(BrickLink::Picture *pic, QString url, bool high_priority)
{
    m_pictures.transfer->get(url, CKeyValueList(), 0, pic, high_priority);
}

void BrickLink::pictureJobFinished(CTransfer::Job *j)
{
    if (!j || !j->data() || !j->userObject())
    {
        return;
    }

    if (!j->contentType().isEmpty())
    {
        // Image names fetched, now fetch actual images

        bool ok = false;
        QMap<QString, QJsonObject> pictureItems;
        if (!j->failed())
        {
            QJsonParseError jerror;
            QJsonDocument jsonResponse = QJsonDocument::fromJson(*(j->data()), &jerror);
            ok = jerror.error == QJsonParseError::NoError && jsonResponse.object()["meta"].toObject()["code"].toInt() == 200;

            if (ok)
            {
                QJsonArray pgData = jsonResponse.object()["data"].toArray();
                foreach (QJsonValue value, pgData)
                {
                    QJsonObject obj = value.toObject();
                    QJsonObject item = obj["item"].toObject();

                    QString key;
                    QTextStream (&key) << "" << item["type"].toString() << "@" << item["no"].toString() << "@" << obj["color_id"].toInt();

                    pictureItems.insert(key.toLower(), obj, true);
                }
            }
        }

        QVector<BrickLink::Picture *> *pictures = static_cast<QVector<BrickLink::Picture *> *>(j->userObject());

        foreach (Picture *pic, *pictures)
        {
            if (ok)
            {
                QString key;
                QTextStream (&key) << "" << pic->item()->itemType()->apiName() << "@" << pic->item()->id() << "@" << pic->color()->id();

                QJsonObject pictureItem = pictureItems[key.toLower()];
                if (pictureItem["image_url"].isString())
                {
                    QString imageUrl = pictureItem["image_url"].toString();

                    if (imageUrl == "N/A")
                    {
                        bool large = !pic->color();
                        QString path;

                        if (!large && pic->item()->itemType()->hasColors())
                        {
                            path = BrickLink::inst()->dataPath(pic->item(), pic->color());
                        }
                        else
                        {
                            path = BrickLink::inst()->dataPath(pic->item());
                        }

                        path.append(large ? "large.png" : "small.png");
                        pic-> m_update_status = Ok;

                        QFile f(path);
                        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                        f.close();

                        pic->load_from_disk();

                        return;
                    }

                    m_pictures.transfer->get("http:" + imageUrl, CKeyValueList(), 0, pic);
                }
            }
        }

        return;
    }

    Picture *pic = static_cast<Picture *>(j->userObject());
    bool large = !pic->color();

    pic->m_update_status = UpdateFailed;

    if (!j->failed())
    {
        QString path;
        QImage img;

        if (!large && pic->item()->itemType()->hasColors())
        {
            path = BrickLink::inst()->dataPath(pic->item(), pic->color());
        }
        else
        {
            path = BrickLink::inst()->dataPath(pic->item());
        }

        if (!path.isEmpty())
        {
            path.append(large ? "large.png" : "small.png");
            pic-> m_update_status = Ok;

            if (j->effectiveUrl().toLower().find("noimage", 0) == -1 && j->data()->size() && img.loadFromData(*j->data()))
            {
                if (!large)
                {
                    img = img.convertToFormat(QImage::Format_ARGB32, Qt::ColorOnly | Qt::DiffuseDither | Qt::DiffuseAlphaDither);
                }

                img.save(path, "PNG");
            }
            else
            {
                QFile f(path);
                f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                f.close();

                qWarning("No image !");
            }

            pic->load_from_disk();
        }
        else
        {
            qWarning("Couldn't get path to save image");
        }
    }
    else if (large && j->responseCode() == 404 && j->url().right(3) == "jpg")
    {
        // no large JPG image -> try a GIF image instead
        pic->m_update_status = Updating;

        QString url = j->url();
        url.replace(url.length() - 3, 3, "gif");

        m_pictures.transfer->get(url, CKeyValueList(), 0, pic);
        return;
    }
    else
    {
        qWarning("Image download failed");
    }

    emit pictureUpdated(pic);
    pic->release();
}
