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
#pragma once

#include <QtQml/QQmlParserStatus>
#include <QtQuick/QQuickPaintedItem>

#include "bricklink/core.h"
#include "bricklink/lot.h"

class Document;
class DocumentModel;
class UpdateDatabase;
class QmlColor;
class QmlCategory;
class QmlItemType;
class QmlItem;
class QmlLot;
class QmlLots;
class QmlPriceGuide;
class QmlPicture;


class QmlImageItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(QImageItem)
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)

public:
    QmlImageItem(QQuickItem *parent = nullptr);

    QImage image() const;
    Q_INVOKABLE void setImage(const QImage &image);
    Q_INVOKABLE void clear();

    void paint(QPainter *painter) override;

signals:
    void imageChanged();

private:
    QImage m_image;
};

class QmlImage // make QImage known to qmllint
{
    Q_GADGET
    QML_FOREIGN(QImage)
    QML_NAMED_ELEMENT(QImage)
    QML_ANONYMOUS
};

Q_DECLARE_METATYPE(QImage)
