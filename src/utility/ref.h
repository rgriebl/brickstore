// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
