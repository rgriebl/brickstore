// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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

    for (const auto &p : table) {
        if (d < p.first || !p.first) {
            s = p.second;
            break;
        }
        d /= p.first;
    }
    return ((delta < 0) ? tr("%1 ago") : tr("in %1")).arg(tr(s, nullptr, int(d)));
}

