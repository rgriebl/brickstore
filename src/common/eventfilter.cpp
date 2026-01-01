// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDebug>
#include <QThread>

#include "eventfilter.h"


bool EventFilter::eventFilter(QObject *o, QEvent *e)
{
    if (!m_filter)
        return false;
    if (!m_types.isEmpty() && !m_types.contains(e->type()))
        return false;

    auto r = m_filter(o, e);

    if (r.testFlag(ResultFlag::RemoveEventFilter))
        o->removeEventFilter(this);
    if (r.testFlag(ResultFlag::DeleteEventFilter))
        deleteLater();
    return r.testFlag(ResultFlag::StopEventProcessing);
}
