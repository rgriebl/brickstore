/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __REF_H__
#define __REF_H__

#include <QtCore/qbasicatomic.h>

class Ref {
public:
    inline Ref()                 { ref = 0; }
    inline void addRef() const   { ref.ref(); }
    inline void release() const  { ref.deref(); }
    inline int refCount() const  { return ref; }

    virtual ~Ref()
    {
        if (ref != 0)
            qWarning("Deleting %p, although refCount=%d", this, int(ref));
    }

private:
    mutable QBasicAtomicInt ref;
};

// tell Qt that Refs are shared and can't simply be deleted
// (QCache will use that function to determine what can really be purged from the cache)

//TODO5 REMOVED template<> inline bool qIsDetached<Ref>(Ref &r) { return r.refCount() == 0; }

#endif

