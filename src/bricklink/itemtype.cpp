// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include "bricklink/itemtype.h"
#include "bricklink/category.h"
#include "bricklink/core.h"


namespace BrickLink {

const QVector<const Category *> ItemType::categories() const
{
    QVector<const Category *> result;
    result.reserve(int(m_categoryIndexes.size()));
    for (const quint16 idx : m_categoryIndexes)
        result << &core()->categories()[idx];
    return result;
}

QSize ItemType::pictureSize() const
{
    QSize s(80, 60);
    if (m_id == 'M')
        s.transpose();
    return s;
}

char ItemType::idFromFirstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}

} // namespace BrickLink
