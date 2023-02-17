// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "global.h"


namespace BrickLink {

class PartColorCode
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const             { return m_id; }
    const Item *item() const;
    const Color *color() const;

    PartColorCode() = default;

    constexpr std::strong_ordering operator<=>(uint id) const { return m_id <=> id; }
    constexpr std::strong_ordering operator<=>(const PartColorCode &other) const { return *this <=> other.m_id; }
    constexpr bool operator==(uint id) const { return (*this <=> id == 0); }

private:
    uint m_id = InvalidId;
    signed int m_itemIndex  : 20 = -1;
    signed int m_colorIndex : 12 = -1;

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink
