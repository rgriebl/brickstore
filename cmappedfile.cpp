/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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


#include <QDataStream>


#include "cmappedfile.h"


CMappedFile::CMappedFile ( const QString &filename )
	: m_file ( filename ), m_memptr ( 0 ), m_ds ( 0 )
{ 
#if defined( Q_OS_WIN32 )
		m_maphandle = 0;
#endif
}

CMappedFile::~CMappedFile ( )
{ 
	close ( ); 
}

QDataStream *CMappedFile::open ( )
{
	if ( m_file. isOpen ( ))
		close ( );

	if ( m_file. open ( QFile::ReadOnly )) {
		m_filesize = m_file. size ( );

		if ( m_filesize ) {
#if defined( Q_OS_WIN32 )
			QT_WA( {
				m_maphandle = CreateFileMappingW ((HANDLE) _get_osfhandle ( m_file. handle ( )), 0, PAGE_READONLY, 0, 0, 0 );
			}, {
				m_maphandle = CreateFileMappingA ((HANDLE) _get_osfhandle ( m_file. handle ( )), 0, PAGE_READONLY, 0, 0, 0 );
			} )

			if ( m_maphandle ) {		
				m_memptr = (const char *) MapViewOfFile ( m_maphandle, FILE_MAP_READ, 0, 0, 0 );
#else
			if ( true ) {
				m_memptr = (const char *) mmap ( 0, m_filesize, PROT_READ, MAP_SHARED, m_file. handle ( ), 0 );

#endif
				if ( m_memptr ) {
					m_mem = QByteArray::fromRawData ( m_memptr, m_filesize );

					m_ds = new QDataStream ( &m_mem, QIODevice::ReadOnly );
					return m_ds;
				}
			}
		}
	}
	close ( );
	return 0;
}

void CMappedFile::close ( )
{
	delete m_ds;
	m_ds = 0;
	if ( m_memptr ) {
#if defined( Q_OS_WIN32 )
		UnmapViewOfFile ( m_memptr );
#else
		munmap ((void *) m_memptr, m_filesize );
#endif
		m_memptr = 0;
	}
#if defined( Q_OS_WIN32 )
	if ( m_maphandle ) {
		CloseHandle ( m_maphandle );
		m_maphandle = 0;
	}
#endif
	m_file. close ( );
}

quint32 CMappedFile::size ( ) const
{
	return m_filesize;
}
