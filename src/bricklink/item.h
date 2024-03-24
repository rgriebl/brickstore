// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <span>

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "bricklink/global.h"
#include "bricklink/category.h"
#include "bricklink/color.h"
#include "bricklink/dimensions.h"
#include "bricklink/itemtype.h"
#include "utility/pooledarray.h"


namespace BrickLink {

using AppearsInItem = QPair<int, const Item *>;
using AppearsInColor =  QVector<AppearsInItem>;
using AppearsIn = QHash<const Color *, AppearsInColor>;


class Item
{
public:
    inline QByteArray id() const           { return m_id.asQByteArray(); }
    inline QString name() const            { return m_name.asQString(); }
    char itemTypeId() const;
    QByteArray itemTypeAndId() const       { return itemTypeId() + id(); }
    const ItemType *itemType() const;
    const Category *category() const;
    const QVector<const Category *> categories(bool includeMainCategory = false) const;
    inline bool hasInventory() const       { return !m_consists_of.isEmpty(); }
    const Color *defaultColor() const;
    double weight() const                  { return double(m_weight); }
    int yearReleased() const               { return m_year_from ? m_year_from + 1900 : 0; }
    int yearLastProduced() const           { return m_year_to ? m_year_to + 1900 : yearReleased(); }
    bool hasDimensions() const             { return !m_dimensions.isEmpty(); }
    std::span<const Dimensions, std::dynamic_extent> dimensions() const;
    bool hasKnownColor(const Color *col) const;
    QVector<const Color *> knownColors() const;
    bool hasAlternateIds() const           { return !m_alternateIds.isEmpty(); }
    QByteArray alternateIds() const        { return m_alternateIds.asQByteArray(); }

    AppearsIn appearsIn(const Color *color = nullptr) const;

    class ConsistsOf {
    public:
        const Item *item() const;
        const Color *color() const;
        int quantity() const        { return m_quantity; }
        bool isExtra() const        { return m_extra; }
        bool isAlternate() const    { return m_isalt; }
        uint alternateId() const    { return m_altid; }
        bool isCounterPart() const  { return m_cpart; }

        uint itemIndex() const      { return m_itemIndex; }  // only for internal use
        uint colorIndex() const     { return m_colorIndex; } // only for internal use

    private:
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

        friend class Item;
        friend class Database;
        friend class TextImport;
    };
    Q_STATIC_ASSERT(sizeof(ConsistsOf) == 8);

    std::span<const ConsistsOf, std::dynamic_extent> consistsOf() const;
    QVector<const RelationshipMatch *> relationshipMatches() const;
    PartOutTraits partOutTraits() const;

    class PCC {
    public:
        inline uint pcc() const  { return m_pcc; }
        const Color *color() const;

    private:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint32  m_pcc;
        quint32  m_colorIndex : 12;
        quint32  m_reserved   : 20 = 0;
#else
        quint32  m_reserved   : 20 = 0;
        quint32  m_colorIndex : 12;
        quint32  m_pcc;
#endif
        friend class Item;
        friend class Core;
        friend class Database;
        friend class TextImport;
    };
    Q_STATIC_ASSERT(sizeof(PCC) == 8);

    std::span<const PCC, std::dynamic_extent> pccs() const;
    const Color *hasPCC(uint pcc) const;

    uint index() const;   // only for internal use (picture/price guide hashes)

    Item() = default;
    explicit Item(std::nullptr_t) : Item() { } // for scripting only!

    inline std::strong_ordering operator<=>(const std::pair<char, QByteArray> &ids) const
    {
        int d = (itemTypeId() - ids.first);
        return d == 0 ? (id().compare(ids.second) <=> 0) : (d <=> 0);
    }
    inline std::strong_ordering operator<=>(const Item &other) const
    {
        return *this <=> std::make_pair(other.itemTypeId(), other.id());
    }
    inline bool operator==(const std::pair<char, QByteArray> &ids) const
    {
        return (*this <=> ids == 0);
    }

private:
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
    };

    Q_STATIC_ASSERT(sizeof(AppearsInRecord) == 4);

    PooledArray<char16_t> m_name;
    PooledArray<char8_t>  m_id;

    quint16    m_itemTypeIndex     : 4 = 0xf;
    quint16    m_defaultColorIndex : 12 = 0xfff;
    quint8     m_year_from = 0;
    quint8     m_year_to = 0;
    float      m_weight = 0;

    PooledArray<quint16> m_categoryIndexes;
    PooledArray<quint16> m_knownColorIndexes;
    PooledArray<AppearsInRecord> m_appears_in;
    PooledArray<ConsistsOf> m_consists_of;
    PooledArray<quint16> m_relationshipMatchIds;
    PooledArray<Dimensions> m_dimensions;
    PooledArray<PCC> m_pccs;
    PooledArray<char8_t> m_alternateIds;

private:
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
