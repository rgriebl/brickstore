/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __STOPWATCH_H__
#define __STOPWATCH_H__

#include <QtDebug>
#include <ctime>

class stopwatch {
public:
    stopwatch(const char *desc)
    {
        m_label = desc;
        m_start = std::clock();
    }
    ~stopwatch()
    {
        uint msec = uint(std::clock() - m_start) * 1000 / CLOCKS_PER_SEC;
        qWarning("%s: %d'%d [sec]", m_label, msec / 1000, msec % 1000);
    }
private:
    const char *m_label;
    clock_t m_start;
};

#endif
