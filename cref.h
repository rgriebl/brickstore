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
#ifndef __CREF_H__
#define __CREF_H__

#include <QBasicAtomicInt>

class CRef {
public:
    inline CRef()                { ref = 0; }
    inline void addRef() const   { ref.ref(); }
    inline void release() const  { ref.deref(); }
    inline int refCount() const  { return ref; }

    virtual ~CRef()
    {
        if (ref != 0)
            qWarning("Deleting %p, although refCount=%d", this, int(ref));
    }

private:
    mutable QBasicAtomicInt ref;
};

#endif

