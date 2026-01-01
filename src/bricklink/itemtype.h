// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QSize>

#include "global.h"
#include "utility/pooledarray.h"


namespace BrickLink {

class Category;

class ItemType
{
public:
    static constexpr char InvalidId = 0;

    char id() const                 { return m_id; }
    inline QString name() const     { return m_name.asQString(); }

    const QVector<const Category *> categories() const;
    bool hasInventories() const     { return m_has_inventories; }
    bool hasColors() const          { return m_has_colors; }
    bool hasWeight() const          { return m_has_weight; }
    bool hasSubConditions() const   { return m_has_subconditions; }
    QSize pictureSize() const;

    ItemType() = default;
    explicit ItemType(std::nullptr_t) : ItemType() { } // for scripting only!

    constexpr std::strong_ordering operator<=>(char id) const { return m_id <=> id; }
    constexpr std::strong_ordering operator<=>(const ItemType &other) const { return *this <=> other.m_id; }
    constexpr bool operator==(char id) const { return (*this <=> id == 0); }

    static char idFromFirstCharInString(const QString &str);

private:
    char  m_id = InvalidId;

    bool  m_has_inventories   : 1;
    bool  m_has_colors        : 1;
    bool  m_has_weight        : 1;
    bool  m_has_subconditions : 1;

    // 6 bytes padding here

    PooledArray<char16_t> m_name;
    PooledArray<quint16> m_categoryIndexes;

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::ItemType *)
