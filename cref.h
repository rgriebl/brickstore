/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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

#include <qptrlist.h>
#include <qasciidict.h>

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
	QAsciiDict<CRef>      m_dict;
	QPtrList<const CRef>  m_no_ref;
	uint                  m_cache_size;
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

