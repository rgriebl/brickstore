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

#include <QtDebug>
#include <QElapsedTimer>


class stopwatch
{
public:
    stopwatch(const char *desc)
    {
        m_label = desc;
        m_timer.start();
    }
    ~stopwatch()
    {
        qint64 micros = m_timer.nsecsElapsed() / 1000;

        int sec = 0;
        if (micros > 1000 * 1000) {
            sec = int(micros / (1000 * 1000));
            micros %= (1000 * 1000);
        }
        int msec = int(micros / 1000);
        int usec = micros % 1000;

        qWarning("%s: %d'%03d.%03d", m_label, sec, msec, usec);
        m_timer.restart();
    }
private:
    Q_DISABLE_COPY(stopwatch)

    const char *m_label;
    QElapsedTimer m_timer;
};
