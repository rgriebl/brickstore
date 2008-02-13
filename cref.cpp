/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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

#include "cref.h"

CRefCacheBase::CRefCacheBase(int cachesize)
    : m_cache_size(cachesize)
{
}

CRefCacheBase::~CRefCacheBase()
{
    clear();
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
        qWarning("Deleting %p, although refcnt=%d, cache=%p", this, m_refcnt, m_cache);
}
