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
#ifndef __DISABLEUPDATES__
#define __DISABLEUPDATES__

#include <QScrollArea>

class DisableUpdates {
public:
    inline DisableUpdates(QWidget *w) : m_w(w), m_reenabled(false)
    {
        QScrollArea *sa = ::qobject_cast<QScrollArea *>(w);
        m_w = sa ? sa->viewport() : w;
        m_upd_enabled = m_w->updatesEnabled();
        m_w->setUpdatesEnabled(false);
    }

    inline ~DisableUpdates()
    {
        reenable();
    }

    inline void reenable()
    {
        if (!m_reenabled) {
            m_w->setUpdatesEnabled(m_upd_enabled);
            m_reenabled = true;
        }
    }

private:
    QWidget * m_w;
    bool      m_upd_enabled : 1;
    bool      m_reenabled   : 1;
};

#endif
