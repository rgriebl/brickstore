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
#include <QTimer>
#include <QLoggingCategory>

#include "ref.h"

Q_DECLARE_LOGGING_CATEGORY(LogCache)


QVector<Ref *> Ref::s_zombieRefs = { };

void Ref::addZombieRef(Ref *ref)
{
    s_zombieRefs.append(ref);

    QTimer::singleShot(1000 * 60, []() {
        for (auto it = s_zombieRefs.begin(); it != s_zombieRefs.end(); ) {
            if ((*it)->refCount() == 0) {
                delete *it;
                it = s_zombieRefs.erase(it);
            } else {
                ++it;
            }
        }
        if (!s_zombieRefs.isEmpty()) {
            qCWarning(LogCache) << "After 1 minute, there are still" << s_zombieRefs.size()
                                << "Refs that cannot be deleted";
        }
    });
}
