// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/qbasicatomic.h>

class Ref
{
public:
    inline Ref()                 { ref = 0; }
    inline void addRef() const   { ref.ref(); }
    inline void release() const  { ref.deref(); }
    inline int refCount() const  { return ref; }

    virtual ~Ref() = default;

private:
    mutable QBasicAtomicInt ref;
};
