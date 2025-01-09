// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


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

void QmlImageItem::setImage(const QImage &image)
{
    m_image = image;
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

