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
#include <QCoreApplication>

#include "ref.h"

Q_DECLARE_LOGGING_CATEGORY(LogCache)


QVector<Ref *> Ref::s_zombieRefs = { };
QTimer *Ref::s_zombieCleaner = nullptr;

void Ref::addZombieRef(Ref *ref)
{
    s_zombieRefs.append(ref);

    if (!s_zombieCleaner) {
        s_zombieCleaner = new QTimer(qApp);
        s_zombieCleaner->setInterval(60 * 1000);

        QObject::connect(s_zombieCleaner, &QTimer::timeout,
                         s_zombieCleaner, []() {
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
            } else {
                delete s_zombieCleaner;
                s_zombieCleaner = nullptr;
            }
        });
    }
    if (!s_zombieCleaner->isActive())
        s_zombieCleaner->start();
}
