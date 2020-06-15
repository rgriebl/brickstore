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

#include "cref.h"

CAsciiRefCacheBase::CAsciiRefCacheBase ( uint dictsize, uint cachesize )
	: m_dict ( dictsize ), m_cache_size ( cachesize )
{
	m_dict. setAutoDelete ( true );
}

CAsciiRefCacheBase::~CAsciiRefCacheBase ( )
{
	clear ( );
}

bool CAsciiRefCacheBase::put ( const char *key, CRef *ref )
{
	if ( ref-> m_cache )
		return false;

	ref-> m_cache = this;
	m_no_ref. append ( ref );
	m_dict. insert ( key, ref );
	return true;
}

CRef *CAsciiRefCacheBase::get ( const char *key )
{
	return m_dict [key];
}

void CAsciiRefCacheBase::clear ( )
{
	m_no_ref. clear ( );
	m_dict. clear ( );
}

void CAsciiRefCacheBase::addRefFor ( const CRef *ref )
{
	//qDebug ( "addRefFor called" );

	if ( ref && ( ref-> m_cache == this ) && ( ref-> m_refcnt == 1 )) {
		//qDebug ( "Moving item [%p] to in-use dict...", (void *) ref );
        m_no_ref. removeOne ( ref );
	}
}

void CAsciiRefCacheBase::releaseFor ( const CRef *ref )
{
	if ( ref && ( ref-> m_refcnt == 0 ) && ( ref-> m_cache == this )) {
		//qDebug ( "Moving item [%p] to cache...", (void *) ref );
		m_no_ref. append ( ref );

		while ( m_no_ref. count ( ) > m_cache_size ) {
            const CRef *del = m_no_ref. takeAt ( 0 );

			for ( Q3AsciiDictIterator<CRef> it ( m_dict ); it. current ( ); ++it ) {
				if ( it. current ( ) == del ) {
					//qDebug ( "Purging item \"%s\" from cache...", it. currentKey ( ));
					m_dict. remove ( it. currentKey ( ));
					break;
				}
			}
		}
	}
}

CRef::~CRef ( )
{
	if ( m_refcnt )
		qWarning ( "Deleting %p, although refcnt=%d", this, m_refcnt );
}
