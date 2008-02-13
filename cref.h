/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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

#include <QList>
#include <QHash>

class CRef;

class CRefCacheBase {
protected:
    CRefCacheBase(int cachesize);
    virtual ~CRefCacheBase();

    bool put(quint64 key, CRef *ref);
    CRef *get(quint64 key);

public:
    virtual void clear();

private:
    void addRefFor(const CRef *ref);
    void releaseFor(const CRef *ref);

    friend class CRef;

private:
    QHash<quint64, CRef *>  m_dict;
    QList<const CRef *>     m_no_ref;
    int                     m_cache_size;
};

template <typename T, uint CACHESIZE> class CRefCache : public CRefCacheBase {
public:
    CRefCache() : CRefCacheBase(CACHESIZE) { }

    bool insert(quint64 key, T *ref)  { return put(key, ref); }
    T *operator [](quint64 key)       { return (T *) get(key); }
};


class CRef {
public:
    CRef() : m_refcnt(0), m_cache(0) { }
    virtual ~CRef();

    void addRef() const   { if (m_cache) { m_refcnt++; m_cache->addRefFor(this); } }
    void release() const  { if (m_cache) { m_refcnt--; m_cache->releaseFor(this); } }

    uint refCount() const { return m_refcnt; }

private:
    friend class CRefCacheBase;

    mutable int           m_refcnt;
    mutable CRefCacheBase *m_cache;
};

#endif

