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
#include <qfile.h>
//Added by qt3to4:
#include <qevent.h>

#include "capplication.h"
#include "ctransfer.h"

bool CTransfer::s_global_init = false;
QMutex CTransfer::s_share_lock;
CURLSH *CTransfer::s_curl_share = 0;
uint CTransfer::s_instance_counter = 0;

CTransfer::CTransfer ( )
{
	m_curl = 0;
	m_total = m_progress = 0;
	m_file_total = m_file_progress = 0;
	m_stop = false;

	m_use_proxy = false;
	m_proxy_port = 0;

	m_active_job = 0;

	s_instance_counter++;
}

CTransfer::~CTransfer ( )
{
	cancelAllJobs ( );

	m_stop = true;
	m_queue_cond. wakeOne ( );
	wait ( );

	if ( m_curl )
		::curl_easy_cleanup ( m_curl );

	s_instance_counter--;

	if ( s_instance_counter == 0 ) {
		if ( s_curl_share ) {
			::curl_share_cleanup ( s_curl_share );
			s_curl_share = 0;
		}

		if ( s_global_init ) {
			::curl_global_cleanup ( );
			s_global_init = false;
		}
	}

}

void CTransfer::setProxy ( bool enable, const QString &name, int port )
{
	m_queue_lock. lock ( );

	m_use_proxy = enable;
    m_proxy_name = name;
	m_proxy_port = port;

	m_queue_lock. unlock ( );
}

CTransfer::Job *CTransfer::get ( const QString &url, const CKeyValueList &query, QFile *file, void *userobject, bool high_priority )
{
	return retrieve ( true, url, query, 0, file, userobject, high_priority );
}

CTransfer::Job *CTransfer::getIfNewer (const QString &url, const CKeyValueList &query, const QDateTime &dt, QFile *file, void *userobject, bool high_priority )
{
	return retrieve ( true, url, query, dt. isValid ( ) ? dt. toTime_t ( ) : 0, file, userobject, high_priority );
}

CTransfer::Job *CTransfer::post ( const QString &url, const CKeyValueList &query, QFile *file, void *userobject, bool high_priority )
{
	return retrieve ( false, url, query, 0, file, userobject, high_priority );
}

QString CTransfer::buildQueryString( const CKeyValueList &kvl )
{
    QString query;

	for ( CKeyValueList::ConstIterator it = kvl. begin ( ); it != kvl. end ( ); ++it ) {
        const char *tmp1 = ( *it ). first. ascii ( );
        const char *tmp2 = ( *it ). second. ascii ( );

		char *etmp1 = curl_escape ( tmp1 ? tmp1 : "", 0 );
		char *etmp2 = curl_escape ( tmp2 ? tmp2 : "", 0 );

		if ( etmp1 && *etmp1 ) {
			if ( !query. isEmpty ( ))
				query += '&';
			query += etmp1;
			query += '=';

			if ( etmp2 && *etmp2 )
				query += etmp2;
		}
		curl_free ( etmp2 );
		curl_free ( etmp1 );
	}
	return query;
}

CTransfer::Job *CTransfer::retrieve ( bool get, const QString &url, const CKeyValueList &query, time_t ifnewer, QFile *file, void *userobject, bool high_priority )
{
	if ( url. isEmpty ( )) //  || ( file && ( !file-> isOpen ( ) || !file-> isWritable ( ))))
		return 0;

	Job *j = new Job ( );
	j-> m_url = url;
	j-> m_query = buildQueryString ( query );
	j-> m_ifnewer = ifnewer;

	j-> m_data = file ? 0 : new QByteArray ( );
	j-> m_file = file;
	j-> m_userobject = userobject;
	j-> m_get_or_post = get;

	j-> m_trans = this;
	j-> m_result = CURLE_OK;
	j-> m_respcode = 0;
	j-> m_finished = false;
	j-> m_filetime = 0;
	j-> m_not_modified = false;
	j-> m_failed = true;

	m_queue_lock. lock ( );
	if ( high_priority )
		m_in_queue. prepend ( j );
	else
		m_in_queue. append ( j );
	m_queue_cond. wakeOne ( );

//	qDebug ( "waking up thread... (%d in list)", m_queue. count ( ));

	m_queue_lock. unlock ( );

	updateProgress ( +1 );

	return j;
}

bool CTransfer::init ( )
{
	if ( !s_global_init ) {
		if ( ::curl_global_init ( CURL_GLOBAL_DEFAULT ) == CURLE_OK ) {
			s_curl_share = ::curl_share_init ( );

			if ( s_curl_share ) {
				::curl_share_setopt ( s_curl_share, CURLSHOPT_LOCKFUNC,   lock_curl );
				::curl_share_setopt ( s_curl_share, CURLSHOPT_UNLOCKFUNC, unlock_curl );
				::curl_share_setopt ( s_curl_share, CURLSHOPT_USERDATA,   0 );
				::curl_share_setopt ( s_curl_share, CURLSHOPT_SHARE,      CURL_LOCK_DATA_DNS );

				s_global_init = true;
			}
		}
	}

	if ( s_global_init )
		m_curl = ::curl_easy_init ( );

	if ( m_curl ) {
		::curl_easy_setopt ( m_curl, CURLOPT_SHARE, s_curl_share );

		start ( IdlePriority );
	}

	return ( m_curl );
}

void CTransfer::run ( )
{
	QString ua = cApp-> appName ( ) + "/" + 
	             cApp-> appVersion ( ) + " (" + 
	             cApp-> sysName ( ) + " " + 
				 cApp-> sysVersion ( ) + "; http://" + 
	             cApp-> appURL ( ) + ")";

    ::curl_easy_setopt ( m_curl, CURLOPT_COOKIEFILE, "");
	::curl_easy_setopt ( m_curl, CURLOPT_VERBOSE, 0 );
	::curl_easy_setopt ( m_curl, CURLOPT_NOPROGRESS, 0 );
	::curl_easy_setopt ( m_curl, CURLOPT_PROGRESSFUNCTION, progress_curl );
	::curl_easy_setopt ( m_curl, CURLOPT_PROGRESSDATA, this );
	::curl_easy_setopt ( m_curl, CURLOPT_NOSIGNAL, 1 );
	::curl_easy_setopt ( m_curl, CURLOPT_FOLLOWLOCATION, 1);
//	::curl_easy_setopt ( m_curl, CURLOPT_TIMEOUT, 120 );
	::curl_easy_setopt ( m_curl, CURLOPT_DNS_CACHE_TIMEOUT, 5*60 );
	::curl_easy_setopt ( m_curl, CURLOPT_WRITEFUNCTION, write_curl );
	::curl_easy_setopt ( m_curl, CURLOPT_WRITEDATA, this );
    ::curl_easy_setopt ( m_curl, CURLOPT_USERAGENT, ua. ascii ( ));
	::curl_easy_setopt ( m_curl, CURLOPT_ENCODING, "" );
	::curl_easy_setopt ( m_curl, CURLOPT_FILETIME, 1 ); 

    QString url, query;

	while ( !m_stop ) {
		Job *j = 0;

		m_queue_lock. lock ( );
		m_active_job = 0;

		if ( m_in_queue. isEmpty ( ))
			m_queue_cond. wait ( &m_queue_lock );
		if ( !m_in_queue. isEmpty ( ))
            j = m_in_queue. takeAt ( 0 );

		if ( j ) {
			m_active_job = j;

			if ( m_use_proxy ) {
				::curl_easy_setopt ( m_curl, CURLOPT_PROXY, m_proxy_name. ascii ( ));
				::curl_easy_setopt ( m_curl, CURLOPT_PROXYPORT, m_proxy_port );
			}
			else
				::curl_easy_setopt ( m_curl, CURLOPT_PROXY, 0 );

			//qDebug ( "Running job: %p", j-> m_userobject );

			if ( j-> m_get_or_post ) {
                url = j-> m_url;

				if ( !j-> m_query. isEmpty ( )) {
					url += '?';
					url += j-> m_query;
				}
				query = "";
				
				::curl_easy_setopt ( m_curl, CURLOPT_HTTPGET, 1 );
                ::curl_easy_setopt ( m_curl, CURLOPT_URL, url.ascii ( ));
				::curl_easy_setopt ( m_curl, CURLOPT_TIMEVALUE, j-> m_ifnewer );
				::curl_easy_setopt ( m_curl, CURLOPT_TIMECONDITION, j-> m_ifnewer ? CURL_TIMECOND_IFMODSINCE : CURL_TIMECOND_NONE );
				
				//qDebug ( "CTransfer::get [%s]", url. data ( ));
			}
			else {
				url = j-> m_url. copy ( );
				query = j-> m_query. copy ( );
				
				::curl_easy_setopt ( m_curl, CURLOPT_POST, 1 );
                ::curl_easy_setopt ( m_curl, CURLOPT_URL, url.ascii ( ));
                ::curl_easy_setopt ( m_curl, CURLOPT_POSTFIELDS, query.ascii ( ));
                ::curl_easy_setopt ( m_curl, CURLOPT_POSTFIELDSIZE, j-> m_query. length ( ) );
				
				//qDebug ( "CTransfer::post [%s] - form-data [%s]", url. data ( ), query. data ( ));
			}
		}

		m_queue_lock. unlock ( );

		if ( m_stop )
			break;
		if ( !j )
			continue;

		CURLcode res = ::curl_easy_perform ( m_curl );
		long respcode = 0;
		char *effurl = 0;
		long filetime = -1;

		m_file_total = m_file_progress = -1;
		QApplication::postEvent ( this, new QCustomEvent ((QEvent::Type) TransferProgressEvent, 0 ));

		if ( res == CURLE_OK ) {
			::curl_easy_getinfo ( m_curl, CURLINFO_RESPONSE_CODE, &respcode );
			::curl_easy_getinfo ( m_curl, CURLINFO_EFFECTIVE_URL, &effurl );
			::curl_easy_getinfo ( m_curl, CURLINFO_FILETIME, &filetime );
		}

		m_queue_lock. lock ( );

		if ( m_active_job == j ) {
			j-> m_result   = res;
			if ( res != CURLE_OK )
				j-> m_error = ::curl_easy_strerror ( res );
			j-> m_respcode = respcode;
			j-> m_effective_url = effurl;
			j-> m_filetime = time_t(( filetime != -1 ) ? filetime : 0 );

			if ( j-> m_ifnewer && (( respcode == 304 ) || ( filetime != -1 && filetime < j-> m_ifnewer ))) 
				j-> m_not_modified = true;

			j-> m_failed = ( res != CURLE_OK ) || !(( respcode == 200 ) || ( respcode == 304 ));
		}
		else if ( m_active_job == 0 ) {
			j-> m_result = CURLE_ABORTED_BY_CALLBACK;
			j-> m_error = "Aborted";
			j-> m_respcode = 0;
			j-> m_failed = true;
		}
		j-> m_finished = true;
		m_out_queue. append ( j );

		m_active_job = 0;
		m_queue_lock. unlock ( );

        QApplication::postEvent ( this, new QEvent( (QEvent::Type) TransferFinishedEvent ), 0);
	}
}

void CTransfer::customEvent ( QEvent *ev )
{
	if ( int( ev-> type ( )) == TransferFinishedEvent ) {
		// we have to copy the job list to avoid problems with
		// recursive mutex locking (we emit signals, which 
		// can lead to slots calling another CTransfer function)
        QList<Job *> finish;

		m_queue_lock. lock ( );

		while ( !m_out_queue. isEmpty ( ))
            finish. append ( m_out_queue. takeAt ( 0 ));

		m_queue_lock. unlock ( );

		while ( !finish. isEmpty ( )) {
            Job *j = finish. takeAt ( 0 );
		
			emit finished ( j );
			updateProgress ( -1 );

			delete j-> file ( );
			delete j-> data ( );
			delete j;
		}
	}
	else if ( int( ev-> type ( )) == TransferProgressEvent ) {
        Job *j = static_cast <Job *> ( ((QCustomEvent*)ev)-> data ( ));

        if ( j )
            emit progress ( j, m_file_progress, m_file_total );
	}
}

void CTransfer::updateProgress ( int delta )
{
	if ( delta > 0 )
		m_total += delta;
	else
		m_progress -= delta;

	emit progress ( m_progress, m_total );

	if ( m_total == m_progress ) {
		m_total = m_progress = 0;

		emit done ( );
		emit progress ( 0, 0 );
	}
}

void CTransfer::cancel ( Job *j )
{
	j-> m_result = CURLE_ABORTED_BY_CALLBACK;
	j-> m_respcode = 0;
	j-> m_finished = true;
	emit finished ( j );
	updateProgress ( -1 );

	delete j-> file ( );
	delete j-> data ( );
	delete j;
}

void CTransfer::cancelAllJobs ( )
{
	// we have to copy the job list to avoid problems with
	// recursive mutex locking (cancel emits signals, which 
	// can lead to slots calling another CTransfer function)
    QList<Job *> canceled;

	m_queue_lock. lock ( );

	while ( !m_out_queue. isEmpty ( ))
        canceled. append ( m_out_queue. takeAt ( 0 ));

	if ( m_active_job ) {
		// we can't delete the Job data right away, since curl_write may be accessing it at any time
		//canceled. append ( m_active_job );

		// just tell the worker thread, that the active job doesn't matter anymore...
		m_active_job = 0;
	}
	
	while ( !m_in_queue. isEmpty ( ))
        canceled. prepend ( m_in_queue. takeAt ( 0 ));
	
	m_queue_lock. unlock ( );

	while ( !canceled. isEmpty ( ))
        cancel ( canceled. takeAt ( 0 ));
}

size_t CTransfer::write_curl ( void *ptr, size_t size, size_t nmemb, void *stream )
{
	CTransfer *that = static_cast <CTransfer *> ( stream );
	Job *j = that-> m_active_job;

	if ( j && j-> m_data ) {
		uint oldsize = j-> m_data-> size ( );

		j-> m_data-> resize ( oldsize + size * nmemb );
		::memcpy ( j-> m_data-> data ( ) + oldsize, ptr, size * nmemb );

		return size * nmemb;
	}
	else if ( j && j-> m_file ) {
		Q_LONG res = j-> m_file-> writeBlock ((const char *) ptr, size * nmemb );

		if ( res < 0 ) {
			// set error code
			res = 0;
		}
		return res;
	}
	else
		return 0;
}

int CTransfer::progress_curl ( void *stream, double dltotal, double dlnow, double /*ultotal*/, double /*ulnow*/ )
{
	CTransfer *that = static_cast <CTransfer *> ( stream );
	
	that-> m_file_progress = int( dlnow );
	that-> m_file_total    = int( dltotal );
    QApplication::postEvent ( that, new QCustomEvent ((QEvent::Type) TransferProgressEvent, that-> m_active_job ));
	
	return 0;
}

void CTransfer::lock_curl ( CURL * /*handle*/, curl_lock_data /*data*/, curl_lock_access /*access*/, void * /*userptr*/ )
{
	s_share_lock. lock ( );
}

void CTransfer::unlock_curl ( CURL * /*handle*/, curl_lock_data /*data*/, void * /*userptr*/ )
{
	s_share_lock. unlock ( );
}
