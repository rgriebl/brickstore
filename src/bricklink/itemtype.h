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

#include <vector>

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "bricklink/qmlwrapperbase.h"


namespace BrickLink {

class Category;

class ItemType
{
public:
    static constexpr char InvalidId = 0;

    char id() const                 { return m_id; }
    QString name() const            { return m_name; }

    const QVector<const Category *> categories() const;
    bool hasInventories() const     { return m_has_inventories; }
    bool hasColors() const          { return m_has_colors; }
    bool hasWeight() const          { return m_has_weight; }
    bool hasSubConditions() const   { return m_has_subconditions; }
    QSize pictureSize() const;
    QSize rawPictureSize() const;

    ItemType() = default;
    ItemType(std::nullptr_t) : ItemType() { } // for scripting only!

private:
    char  m_id = InvalidId;

    bool  m_has_inventories   : 1;
    bool  m_has_colors        : 1;
    bool  m_has_weight        : 1;
    bool  m_has_subconditions : 1;

    // 6 bytes padding here

    QString m_name;
    std::vector<quint16> m_categoryIndexes;

private:
    static bool lessThan(const ItemType &itt, char id) { return itt.m_id < id; }

    friend class Core;
    friend class Database;
    friend class TextImport;
};


class QmlItemType : public QmlWrapperBase<const ItemType>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasInventories READ hasInventories CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasColors READ hasColors CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasWeight READ hasWeight CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasSubConditions READ hasSubConditions CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QSize pictureSize READ pictureSize CONSTANT)

public:
    QmlItemType(const ItemType *itt = nullptr);

    QString id() const;
    QVariantList categories() const;

    friend class QmlBrickLink;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::ItemType *)
Q_DECLARE_METATYPE(BrickLink::QmlItemType)
