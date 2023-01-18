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

typedef QPair<int, const Item *>             AppearsInItem;
typedef QVector<AppearsInItem>               AppearsInColor;
typedef QHash<const Color *, AppearsInColor> AppearsIn;


class Item
{
public:
    inline QByteArray id() const           { return m_id; }
    inline QString name() const            { return m_name; }
    inline char itemTypeId() const         { return m_itemTypeId; }
    const ItemType *itemType() const;
    const Category *category() const;
    const QVector<const Category *> additionalCategories(bool includeMainCategory = false) const;
    inline bool hasInventory() const       { return (m_lastInventoryUpdate >= 0); }
    QDateTime inventoryUpdated() const;
    const Color *defaultColor() const;
    double weight() const                  { return double(m_weight); }
    int yearReleased() const               { return m_year_from ? m_year_from + 1900 : 0; }
    int yearLastProduced() const           { return m_year_to ? m_year_to + 1900 : yearReleased(); }
    bool hasKnownColor(const Color *col) const;
    const QVector<const Color *> knownColors() const;

    AppearsIn appearsIn(const Color *color = nullptr) const;

    class ConsistsOf {
    public:
        const Item *item() const;
        const Color *color() const;
        int quantity() const        { return m_bits.m_quantity; }
        bool isExtra() const        { return m_bits.m_extra; }
        bool isAlternate() const    { return m_bits.m_isalt; }
        uint alternateId() const    { return m_bits.m_altid; }
        bool isCounterPart() const  { return m_bits.m_cpart; }

        uint itemIndex() const      { return m_bits.m_itemIndex; }  // only for internal use
        uint colorIndex() const     { return m_bits.m_colorIndex; } // only for internal use

    private:
        union {
            struct {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                quint64  m_quantity   : 12;
                quint64  m_itemIndex  : 20;
                quint64  m_colorIndex : 12;
                quint64  m_extra      : 1;
                quint64  m_isalt      : 1;
                quint64  m_altid      : 6;
                quint64  m_cpart      : 1;
                quint64  m_reserved   : 11;
#else
                quint64  m_reserved   : 11;
                quint64  m_cpart      : 1;
                quint64  m_altid      : 6;
                quint64  m_isalt      : 1;
                quint64  m_extra      : 1;
                quint64  m_colorIndex : 12;
                quint64  m_itemIndex  : 20;
                quint64  m_quantity   : 12;
#endif
            } m_bits;
            quint64 m_data = 0;
        };

        friend class Core;
        friend class Database;
        friend class TextImport;
    };
    Q_STATIC_ASSERT(sizeof(ConsistsOf) == 8);

    const QVector<ConsistsOf> &consistsOf() const;

    PartOutTraits partOutTraits() const;

    uint index() const;   // only for internal use (picture/priceguide hashes)

    Item() = default;
    explicit Item(std::nullptr_t) : Item() { } // for scripting only!

    constexpr std::strong_ordering operator<=>(const std::pair<char, QByteArray> &ids) const
    {
        int d = (m_itemTypeId - ids.first);
        return d == 0 ? (m_id.compare(ids.second) <=> 0) : (d <=> 0);
    }
    std::strong_ordering operator<=>(const Item &other) const
    {
        return *this <=> std::make_pair(other.m_itemTypeId, other.m_id);
    }
    constexpr bool operator==(const std::pair<char, QByteArray> &ids) const
    {
        return (*this <=> ids == 0);
    }

private:
    QString    m_name;
    QByteArray m_id;
    qint16     m_itemTypeIndex = -1;
    qint16     m_categoryIndex = -1;
    qint16     m_defaultColorIndex = -1;
    quint8     m_year_from = 0;
    quint8     m_year_to = 0;
    qint64     m_lastInventoryUpdate = -1;
    float      m_weight = 0;
    char       m_itemTypeId; // the same itemType()->id()
    // 3 bytes padding here
    std::vector<qint16> m_additionalCategoryIndexes;
    std::vector<quint16> m_knownColorIndexes;

    union AppearsInRecord {
        struct {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            quint32  m_colorIndex : 12;
            quint32  m_colorSize  : 20;
#else
            quint32  m_colorSize  : 20;
            quint32  m_colorIndex : 12;
#endif
        } m_colorBits;
        struct {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            quint32  m_quantity  : 12;
            quint32  m_itemIndex : 20;
#else
            quint32  m_itemIndex : 20;
            quint32  m_quantity  : 12;
#endif
        } m_itemBits;
        quint32 m_data = 0;
    };

    Q_STATIC_ASSERT(sizeof(AppearsInRecord) == 4);

    std::vector<AppearsInRecord> m_appears_in;
    QVector<ConsistsOf> m_consists_of;

private:
    void setAppearsIn(const QHash<uint, QVector<QPair<int, uint>>> &appearHash);
    void setConsistsOf(const QVector<ConsistsOf> &items);

    friend class Core;
    friend class Database;
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
Q_DECLARE_METATYPE(const BrickLink::AppearsInItem *)
Q_DECLARE_TYPEINFO(BrickLink::Item::ConsistsOf, Q_PRIMITIVE_TYPE);
