/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include <QtCore/QByteArray>
#include <QDebug>

#include "color.h"


namespace BrickLink {

class ColorChangeLogEntry
{
public:
    uint fromColorId() const  { return m_fromColorId; }
    uint toColorId() const  { return m_toColorId; }

    friend bool operator==(const ColorChangeLogEntry &e, uint colorId)
    { return e.m_fromColorId == colorId; }
    friend bool operator<(const ColorChangeLogEntry &e, uint colorId)
    { return e.m_fromColorId < colorId; }
    friend bool operator<(const ColorChangeLogEntry &that, const ColorChangeLogEntry &other)
    { return that.m_fromColorId < other.m_fromColorId; }

    ColorChangeLogEntry(uint fromColorId = Color::InvalidId, uint toColorId = Color::InvalidId)
        : m_fromColorId(fromColorId)
        , m_toColorId(toColorId)
    { }

private:
    uint m_fromColorId;
    uint m_toColorId;

    friend class Core;
};


class ItemChangeLogEntry
{
public:
    char fromItemTypeId() const    { return m_fromTypeAndId.at(0); }
    QByteArray fromItemId() const  { return m_fromTypeAndId.mid(1); }

    char toItemTypeId() const    { return m_toTypeAndId.at(0); }
    QByteArray toItemId() const  { return m_toTypeAndId.mid(1); }

    friend bool operator==(const ItemChangeLogEntry &e, const QByteArray &typeAndId)
    { return e.m_fromTypeAndId == typeAndId; }
    friend bool operator<(const ItemChangeLogEntry &e, const QByteArray &typeAndId)
    { return e.m_fromTypeAndId < typeAndId; }
    friend bool operator<(const ItemChangeLogEntry &that, const ItemChangeLogEntry &other)
    { return that.m_fromTypeAndId < other.m_fromTypeAndId; }

    ItemChangeLogEntry(const QByteArray &fromTypeAndId = { }, const QByteArray &toTypeAndId = { })
        : m_fromTypeAndId(fromTypeAndId)
        , m_toTypeAndId(toTypeAndId)
    { }

private:
    QByteArray m_fromTypeAndId;
    QByteArray m_toTypeAndId;

    friend class Core;
};

} // namespace BrickLink
