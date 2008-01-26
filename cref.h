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
#include <QString>

class CRef;

class CAsciiRefCacheBase {
protected:
    CAsciiRefCacheBase(int cachesize);
    virtual ~CAsciiRefCacheBase();

    bool put(const QString &key, CRef *ref);
    CRef *get(const QString &key);

public:
    virtual void clear();

private:
    void addRefFor(const CRef *ref);
    void releaseFor(const CRef *ref);

    friend class CRef;

private:
    QHash<QString, CRef *>  m_dict;
    QList<const CRef *>     m_no_ref;
    int                     m_cache_size;
};

template <typename T, uint CACHESIZE> class CAsciiRefCache : public CAsciiRefCacheBase {
public:
    CAsciiRefCache() : CAsciiRefCacheBase(CACHESIZE) { }

    bool insert(const QString &key, T *ref)  { return put(key, ref); }
    T *operator [](const QString &key)       { return (T *) get(key); }
};


class CRef {
public:
    CRef() : m_refcnt(0), m_cache(0) { }
    virtual ~CRef();

    void addRef() const   { if (m_cache) { m_refcnt++; m_cache->addRefFor(this); } }
    void release() const  { if (m_cache) { m_refcnt--; m_cache->releaseFor(this); } }

    uint refCount() const { return m_refcnt; }

private:
    friend class CAsciiRefCacheBase;

    mutable uint                m_refcnt;
    mutable CAsciiRefCacheBase *m_cache;
};

#endif

