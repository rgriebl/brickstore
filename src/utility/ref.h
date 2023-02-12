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

#include <QtCore/qbasicatomic.h>

#include "utility/q3cache.h"

QT_FORWARD_DECLARE_CLASS(QTimer)


class Ref
{
public:
    inline Ref()                 { ref = 0; }
    inline void addRef() const   { ref.ref(); }
    inline void release() const  { ref.deref(); }
    inline int refCount() const  { return ref; }

    virtual ~Ref() = default;

    static void addZombieRef(Ref *ref);

private:
    mutable QBasicAtomicInt ref;

    static QVector<Ref *> s_zombieRefs;
    static QTimer *s_zombieCleaner;
};

// tell Qt that Refs are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<Ref>(Ref &r) { return r.refCount() == 0; }
