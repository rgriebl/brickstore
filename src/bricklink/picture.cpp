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


namespace BrickLink {

Picture::Picture(const Item *item, const Color *color)
    : m_item(item)
    , m_color(color)
{
    m_valid = false;
    m_updateAfterLoad = false;
    m_update_status = UpdateStatus::Ok;
}

Picture::~Picture()
{
    cancelUpdate();
}

const QImage Picture::image() const
{
    return m_image;
}

int Picture::cost() const
{
    if (m_image.isNull())
        return 640*480*4 / 1024;      // ~ 640*480 32bpp
    else
        return int(m_image.sizeInBytes() / 1024);
}

QFile *Picture::readFile() const
{
    bool large = (!m_color);
    bool hasColors = m_item->itemType()->hasColors();

    return core()->dataReadFile(large ? u"large.jpg" : u"normal.png", m_item,
                                (!large && hasColors) ? m_color : nullptr);
}

QSaveFile *Picture::saveFile() const
{
    bool large = (!m_color);
    bool hasColors = m_item->itemType()->hasColors();

    return core()->dataSaveFile(large ? u"large.jpg" : u"normal.png", m_item,
                                (!large && hasColors) ? m_color : nullptr);
}

bool Picture::loadFromDisk(QDateTime &fetched, QImage &image) const
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

void Picture::setIsValid(bool valid)
{
    if (valid != m_valid) {
        m_valid = valid;
        emit isValidChanged(valid);
    }
}

void Picture::setUpdateStatus(UpdateStatus status)
{
    if (status != m_update_status) {
        m_update_status = status;
        emit updateStatusChanged(status);
    }
}

void Picture::setLastUpdated(const QDateTime &dt)
{
    if (dt != m_fetched) {
        m_fetched = dt;
        emit lastUpdatedChanged(dt);
    }
}

quint64 Picture::key(const Item *item, const Color *color)
{
    return (quint64(color ? color->id() : uint(-1)) << 32)
            | (quint64(item->itemTypeId()) << 24)
            | (quint64(item->index() + 1));
}

void Picture::update(bool highPriority)
{
    core()->updatePicture(this, highPriority);
}

void Picture::cancelUpdate()
{
    if (core())
        core()->cancelPictureUpdate(this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Picture
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a picture of a BrickLink item.

    Each picture of an item in the BrickLink catalog is available as a Picture object.

    You cannot create Picture objects yourself, but you can retrieve a Picture object given the
    item and color id via BrickLink::picture() or BrickLink::largePicture().

    \note Pictures aren't readily available, but need to be asynchronously loaded (or even
          downloaded) at runtime. You need to connect to the signal BrickLink::pictureUpdated()
          to know when the data has been loaded.
*/
/*! \qmlproperty bool Picture::isNull
    \readonly
    Returns whether this Picture is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item Picture::item
    \readonly
    The BrickLink item reference this picture is requested for.
*/
/*! \qmlproperty Color Picture::color
    \readonly
    The BrickLink color reference this picture is requested for.
*/
/*! \qmlproperty date Picture::lastUpdated
    \readonly
    Holds the time stamp of the last successful update of this picture.
*/
/*! \qmlproperty UpdateStatus Picture::updateStatus
    \readonly
    Returns the current update status. The available values are:
    \value BrickLink::Ok            The last picture load (or download) was successful.
    \value BrickLink::Loading       BrickStore is currently loading the picture from the local cache.
    \value BrickLink::Updating      BrickStore is currently downloading the picture from BrickLink.
    \value BrickLink::UpdateFailed  The last download from BrickLink failed. isValid might still be
                                    \c true, if there was a valid picture available before the
                                    failed update!
*/
/*! \qmlproperty bool Picture::isValid
    \readonly
    Returns whether the image property currently holds a valid image.
*/
/*! \qmlproperty image Picture::image
    \readonly
    Returns the image if the Picture object isValid, or a null image otherwise.
*/
/*! \qmlmethod Picture::update(bool highPriority = false)
    Tries to re-download the picture from the BrickLink server. If you set \a highPriority to \c
    true the load/download request will be pre-prended to the work queue instead of appended.
*/

} // namespace BrickLink
