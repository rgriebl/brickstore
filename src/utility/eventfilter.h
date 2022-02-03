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

#include <algorithm>

#include <QObject>


class EventFilter : public QObject
{
public:
    typedef std::function<bool(QObject *, QEvent *e)> Type;

    inline explicit EventFilter(QObject *o, Type filter)
        : EventFilter(o, filter, o)
    { }
    inline explicit EventFilter(QObject *o, Type filter, QObject *parent)
        : QObject(parent)
        , m_filter(filter)
    {
        Q_ASSERT(o);
        Q_ASSERT(filter);
        if (o)
            o->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        return m_filter ? m_filter(o, e) : false;
    }

private:
    Type m_filter;
};
