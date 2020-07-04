/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include "humanreadabletimedelta.h"


QString HumanReadableTimeDelta::toString(const QDateTime &from, const QDateTime &to)
{
    qint64 delta = from.secsTo(to);

    if (!to.isValid() || !from.isValid())
        return tr("never");
    if (delta == 0)
        return tr("right now");

    const char *s = nullptr;
    static const std::vector<std::pair<int, const char *>> table = {
        { 60, QT_TR_N_NOOP("%n second(s)") },
        { 60, QT_TR_N_NOOP("%n minute(s)") },
        { 24, QT_TR_N_NOOP("%n hour(s)") },
        { 31, QT_TR_N_NOOP("%n day(s)") },
        { 12, QT_TR_N_NOOP("%n month(s)") },
        {  0, QT_TR_N_NOOP("%n year(s)") }
    };
    auto d = std::abs(delta);

    for (const auto p : table) {
        if (d < p.first || !p.first) {
            s = p.second;
            break;
        }
        d /= p.first;
    }
    return ((delta < 0) ? tr("%1 ago") : tr("in %1")).arg(tr(s, nullptr, d));
}

