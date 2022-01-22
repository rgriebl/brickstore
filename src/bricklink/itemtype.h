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

#include "global.h"

namespace BrickLink {

class ItemType
{
public:
    static constexpr uint InvalidId = 0;

    char id() const                 { return m_id; }
    QString name() const            { return m_name; }

    const QVector<const Category *> categories() const;
    bool hasInventories() const     { return m_has_inventories; }
    bool hasColors() const          { return m_has_colors; }
    bool hasYearReleased() const    { return m_has_year; }
    bool hasWeight() const          { return m_has_weight; }
    bool hasSubConditions() const   { return m_has_subconditions; }
    char pictureId() const          { return m_picture_id; }
    QSize pictureSize() const;
    QSize rawPictureSize() const;

    ItemType() = default;
    ItemType(std::nullptr_t) : ItemType() { } // for scripting only!

private:
    char  m_id = InvalidId;
    char  m_picture_id = 0;

    bool  m_has_inventories   : 1;
    bool  m_has_colors        : 1;
    bool  m_has_weight        : 1;
    bool  m_has_year          : 1;
    bool  m_has_subconditions : 1;

    // 5 bytes padding here

    QString m_name;
    std::vector<quint16> m_categoryIndexes;

private:
    static bool lessThan(const ItemType &itt, char id) { return itt.m_id < id; }

    friend class Core;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::ItemType *)
