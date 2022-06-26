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
