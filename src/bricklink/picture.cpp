/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include <QFile>

#include "bricklink/picture.h"
#include "bricklink/item.h"
#include "bricklink/core.h"


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
    if (m_image.isNull())
        return 640*480*4 / 1024;      // ~ 640*480 32bpp
    else
        return int(m_image.sizeInBytes() / 1024);
}

QFile *BrickLink::Picture::readFile() const
{
    bool large = (!m_color);
    bool hasColors = m_item->itemType()->hasColors();

    return core()->dataReadFile(large ? u"large.jpg" : u"normal.png", m_item,
                                (!large && hasColors) ? m_color : nullptr);
}

QSaveFile *BrickLink::Picture::saveFile() const
{
    bool large = (!m_color);
    bool hasColors = m_item->itemType()->hasColors();

    return core()->dataSaveFile(large ? u"large.jpg" : u"normal.png", m_item,
                                (!large && hasColors) ? m_color : nullptr);
}

bool BrickLink::Picture::loadFromDisk(QDateTime &fetched, QImage &image) const
{
    if (!m_item)
        return false;

    std::unique_ptr<QFile> f(readFile());

    bool isValid = false;

    if (f && f->isOpen()) {
        if (f->size() > 0) {
            try {
                QByteArray ba = f->readAll();
                QImage img;

                // optimize loading when a lot of QImageIO plugins are available
                // (e.g. when building against Qt from a Linux distro)
                if (ba.startsWith("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"))
                    isValid = img.loadFromData(ba, "PNG");
                else if (ba.startsWith("GIF8"))
                    isValid = img.loadFromData(ba, "GIF");
                else if (ba.startsWith("\xFF\xD8\xFF"))
                    isValid = img.loadFromData(ba, "JPG");
                if (!isValid)
                    isValid = img.loadFromData(ba);
                image = img;
            } catch (const std::bad_alloc &) {
                image = { };
                isValid = false;
            }
        }
        fetched = f->fileTime(QFileDevice::FileModificationTime);
    }
    return isValid;
}

quint64 BrickLink::Picture::key(const Item *item, const Color *color)
{
    return (quint64(color ? color->id() : uint(-1)) << 32)
            | (quint64(item->itemTypeId()) << 24)
            | (quint64(item->index() + 1));
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
