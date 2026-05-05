// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "global.h"


namespace BrickLink {

// deprecated with DB v11 and only kept around to generate older DB versions

class PartColorCode
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    PartColorCode() = default;

    constexpr std::strong_ordering operator<=>(const PartColorCode &other) const { return m_id <=> other.m_id; }

private:
    uint m_id = InvalidId;
    signed int m_itemIndex  : 20 = -1;
    signed int m_colorIndex : 12 = -1;

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink
