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

#if 0

#include <QCache>

#include "cref.h"


template <typename KEY, typename T>
class CRefCache {
public:
    CRefCache(int maxcost)
        : m_cached(maxcost)
    { }

    void clear()
    {
        m_cached.clear();
        qDeleteAll(m_refed);
        m_refed.clear();
    }

    inline bool insert(const KEY &k, T *t, int cost = 1)
    {
        CacheNode *n = new CacheNode(this, k, t, cost);
        return m_cache.insert(k, n);
    }

    inline T *object(const KEY &k) const
    {
        return operator[](k);
    }

    inline T *operator[](const KEY &k) const
    {
        CacheNode *n = m_cached[k];
        if (!n)
            n = m_refed.value(k);
        return n ? n->m_data : 0;
    }

    void pin(T *t)
    {

    }

    void unPin(T *t)
    {
    }

private:
    void addRefFor(const CRef *ref)
    {
        if (ref && (ref->m_cache == this) && (ref->m_refcnt == 1)) {
            //qDebug ( "Moving item [%p] to in-use dict...", (void *) ref );
            m_cached.find
            m_refed.insert(KKK, ref);
        }
    }

    void releaseFor(const CRef *ref)
    {
    if (ref && (ref->m_refcnt == 0) && (ref->m_cache == this)) {
        //qDebug("Moving item [%p] to cache...", (void *) ref);
        m_no_ref.append(ref);

        while (m_no_ref.count() > m_cache_size) {
            const CRef *del = m_no_ref.takeFirst();
            //qDebug("Purging item [%p] from cache...", (void *) del);

            for (QHash<quint64, CRef *>::iterator it = m_dict.begin(); it != m_dict.end(); ++it) {
                if (it.value() == del) {
                    //qDebug ( "Purging item \"%llx\" from cache...", it.key ( ));
                    m_dict.erase(it);
                    break;
                }
            }
            delete del;
        }
    }
    }

private:
    class CacheNode {
    public:
        KEY   m_key;
        T *   m_data;
        quint m_cost;
        quint m_refcnt : 31;
        quint m_refed  : 1;
        CRefCacheBase *m_base;

        CacheNode(CRefCache *cache, const KEY &k, T *t, quint cost)
            : m_key(k), m_data(t), m_cost(cost), m_refcnt(0), m_base(cache)
        { }

        ~Node()
        {
            if (m_refcnt && m_base && !m_refed) {
                m_refed = 1;
                m_base.m_refed.append(this);
            }
        }
    };

    QCache<KEY, Node> m_cached;

    QLinkedList<CacheNode>  m_nodes;

    QHash<KEY, Node> m_hash;
    Node *m_first;
    Node *m_last;

    bool insert(const KEY &k, T *t, quint cost)
    {
        if (cost > m_maxcost) {
            delete t;
            return false;
        }
        if (cost + m_currentcost > m_maxcost) {
            if (!purge(m_maxcost - cost)) {
                delete t;
                return false;
            }
        }

        Node *n = new Node(k, t, cost);
        m_hash.insert(KEY, n);
        n->m_next = 0;
        n->m_prev = m_last;
        m_last->m_next = n;
        m_last = n;
        m_currentcost += cost;
    }

    T *operator[](const KEY &k)
    {
        Node *n = m_hash.value(k);
        if (n) {
            if (n->m_next) {
                n->m_next->m_prev = m_prev;
                n->m_prev->m_next = m_next;
            }
            return n.m_data;
        }
        return 0;
    }

};




CRefCacheBase::CRefCacheBase(int cachesize)
    : m_cache_size(cachesize)
{
}

CRefCacheBase::~CRefCacheBase()
{
    clear();
}

bool CRefCacheBase::put(quint64 key, CRef *ref, quint cost)
{
    if (m_current_cost + cost > m_max_cost)
        purge();
    if (m_current_cost + cost > m_max_cost)
        return false;

    m_hash.insert(key, qMakePair<CRef *, quint>(ref, cost));
    m_current_cost += cost;
    return true;
}

CRef *CRefCacheBase::get(quint64 key)
{
    return m_hash.value(key);
}

CRef *CRefCacheBase::take(quint64 key)
{
    CRef *res = 0;
    QHash<quint64, QPair<CRef *, quint> >::iterator it = find(key);
    if (it != end()) {
        res = it.value().first;
        erase(it);
    }
    return res;
}

void CRefCacheBase::clear()
{
    m_no_ref.clear();
    qDeleteAll(m_hash);
    m_dict.clear();
}


bool CRefCacheBase::put(quint64 key, CRef *ref)
{
    if (ref->m_cache)
        return false;

    ref->m_cache = this;
    m_no_ref.append(ref);
    m_dict.insert(key, ref);
    return true;
}

CRef *CRefCacheBase::get(quint64 key)
{
    return m_dict.value(key);
}

void CRefCacheBase::clear()
{
    m_no_ref.clear();
    qDeleteAll(m_dict);
    m_dict.clear();
}

void CRefCacheBase::addRefFor(const CRef *ref)
{
    //qDebug ( "addRefFor called" );

    if (ref && (ref->m_cache == this) && (ref->m_refcnt == 1)) {
        //qDebug ( "Moving item [%p] to in-use dict...", (void *) ref );
        m_no_ref.removeAll(ref);
    }
}

void CRefCacheBase::releaseFor(const CRef *ref)
{
    if (ref && (ref->m_refcnt == 0) && (ref->m_cache == this)) {
        //qDebug("Moving item [%p] to cache...", (void *) ref);
        m_no_ref.append(ref);

        while (m_no_ref.count() > m_cache_size) {
            const CRef *del = m_no_ref.takeFirst();
            //qDebug("Purging item [%p] from cache...", (void *) del);

            for (QHash<quint64, CRef *>::iterator it = m_dict.begin(); it != m_dict.end(); ++it) {
                if (it.value() == del) {
                    //qDebug ( "Purging item \"%llx\" from cache...", it.key ( ));
                    m_dict.erase(it);
                    break;
                }
            }
            delete del;
        }
    }
}

CRef::~CRef()
{
    if (m_refcnt)
        qWarning("Deleting %p, although refcnt=%d (cache=%p)", this, m_refcnt, m_cache);
}



#endif