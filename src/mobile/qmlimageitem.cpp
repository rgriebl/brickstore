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

#include <QPainter>
#include <QQmlEngine>

#include "qmlimageitem.h"


QmlImageItem::QmlImageItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{ }

QImage QmlImageItem::image() const
{
    return m_image;
}

QImage QmlImageItem::fallbackImage() const
{
    return m_fallbackImage;
}

void QmlImageItem::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void QmlImageItem::setFallbackImage(const QImage &image)
{
    m_fallbackImage = image;
    if (image.isNull())
        update();
}

void QmlImageItem::clear()
{
    setImage({ });
}

void QmlImageItem::paint(QPainter *painter)
{
    QImage image = m_image;

    if (image.isNull())
        image = m_fallbackImage;
    if (image.isNull())
        return;

    QRect br = boundingRect().toRect();
    QSize itemSize = br.size();
    QSize imageSize = image.size();

    imageSize.scale(itemSize, Qt::KeepAspectRatio);

    br.setSize(imageSize);
    br.translate((itemSize.width() - imageSize.width()) / 2,
                 (itemSize.height() - imageSize.height()) / 2);

    painter->drawImage(br, image);
}

#include "moc_qmlimageitem.cpp"

