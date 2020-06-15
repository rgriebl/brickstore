/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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

#include <qlist.h>
#include <q3asciidict.h>

class CRef;

class CAsciiRefCacheBase {
protected:
	CAsciiRefCacheBase ( uint dictsize, uint cachesize );
	virtual ~CAsciiRefCacheBase ( );

	bool put ( const char *key, CRef *ref );
	CRef *get ( const char *key );

public:
	virtual void clear ( );

private:
	void addRefFor ( const CRef *ref );
	void releaseFor ( const CRef *ref );

	friend class CRef;

private:
    Q3AsciiDict<CRef>     m_dict;
    QList<const CRef *>   m_no_ref;
    int                   m_cache_size;
};

template <typename T, uint DICTSIZE, uint CACHESIZE> class CAsciiRefCache : public CAsciiRefCacheBase {
public:
	CAsciiRefCache ( ) : CAsciiRefCacheBase ( DICTSIZE, CACHESIZE ) { }

	bool insert ( const char *key, T *ref )  { return put ( key, ref ); }
	T *operator [] ( const char *key )       { return (T *) get ( key ); }
};


class CRef {
public:
	CRef ( ) : m_refcnt ( 0 ), m_cache ( 0 ) { }
	virtual ~CRef ( );

	void addRef ( ) const   { if ( m_cache ) { m_refcnt++; m_cache-> addRefFor ( this ); } }
	void release ( ) const  { if ( m_cache ) { m_refcnt--; m_cache-> releaseFor ( this ); } }

	uint refCount ( ) const { return m_refcnt; }

private:
	friend class CAsciiRefCacheBase;

	mutable uint                m_refcnt;
	mutable CAsciiRefCacheBase *m_cache;
};

#endif

