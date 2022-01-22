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

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "bricklink/global.h"
#include "bricklink/category.h"
#include "bricklink/color.h"
#include "bricklink/itemtype.h"


namespace BrickLink {

class Item
{
public:
    inline QByteArray id() const           { return m_id; }
    inline QString name() const            { return m_name; }
    inline char itemTypeId() const         { return m_itemTypeId; }
    const ItemType *itemType() const;
    const Category *category() const;
    inline bool hasInventory() const       { return (m_lastInventoryUpdate >= 0); }
    QDateTime inventoryUpdated() const;
    const Color *defaultColor() const;
    double weight() const                  { return double(m_weight); }
    int yearReleased() const               { return m_year ? m_year + 1900 : 0; }
    bool hasKnownColor(const Color *col) const;
    const QVector<const Color *> knownColors() const;

    AppearsIn appearsIn(const Color *color = nullptr) const;

    class ConsistsOf {
    public:
        const Item *item() const;
        const Color *color() const;
        int quantity() const        { return m_qty; }
        bool isExtra() const        { return m_extra; }
        bool isAlternate() const    { return m_isalt; }
        uint alternateId() const    { return m_altid; }
        bool isCounterPart() const  { return m_cpart; }

    private:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint64  m_qty      : 12;
        quint64  m_itemIndex  : 20;
        quint64  m_colorIndex : 12;
        quint64  m_extra    : 1;
        quint64  m_isalt    : 1;
        quint64  m_altid    : 6;
        quint64  m_cpart    : 1;
        quint64  m_reserved : 11;
#else
        quint64  m_reserved : 11;
        quint64  m_cpart    : 1;
        quint64  m_altid    : 6;
        quint64  m_isalt    : 1;
        quint64  m_extra    : 1;
        quint64  m_colorIndex : 12;
        quint64  m_itemIndex  : 20;
        quint64  m_qty      : 12;
#endif

        friend class TextImport;
        friend class Core;
    };
    Q_STATIC_ASSERT(sizeof(ConsistsOf) == 8);

    const QVector<ConsistsOf> &consistsOf() const;

    uint index() const;   // only for internal use (picture/priceguide hashes)

    Item() = default;
    Item(std::nullptr_t) : Item() { } // for scripting only!

private:
    QString    m_name;
    QByteArray m_id;
    qint16     m_itemTypeIndex = -1;
    qint16     m_categoryIndex = -1;
    qint16     m_defaultColorIndex = -1;
    char       m_itemTypeId; // the same itemType()->id()
    quint8     m_year = 0;
    qint64     m_lastInventoryUpdate = -1;
    float      m_weight = 0;
    // 4 bytes padding here
    std::vector<quint16> m_knownColorIndexes;

    struct AppearsInRecord {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint32  m12  : 12;
        quint32  m20  : 20;
#else
        quint32  m20  : 20;
        quint32  m12  : 12;
#endif
    };
    Q_STATIC_ASSERT(sizeof(AppearsInRecord) == 4);

    std::vector<AppearsInRecord> m_appears_in;
    QVector<ConsistsOf> m_consists_of;

private:
    void setAppearsIn(const QHash<uint, QVector<QPair<int, uint>>> &appearHash);
    void setConsistsOf(const QVector<ConsistsOf> &items);


    static int compare(const Item **a, const Item **b);
    static bool lessThan(const Item &item, const std::pair<char, QByteArray> &ids);

    friend class Core;
    friend class ItemType;
    friend class TextImport;
};

class Incomplete
{
public:
    QByteArray m_item_id;
    char       m_itemtype_id = ItemType::InvalidId;
    uint       m_category_id = Category::InvalidId;
    uint       m_color_id = Color::InvalidId;
    QString    m_item_name;
    QString    m_itemtype_name;
    QString    m_category_name;
    QString    m_color_name;

    bool operator==(const Incomplete &other) const = default;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Item *)
