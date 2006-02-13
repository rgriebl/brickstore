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
#ifndef __CTRANSFER_H__
#define __CTRANSFER_H__

#include <time.h>

#include <qobject.h>
#include <qthread.h>
#include <qptrlist.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qvaluelist.h>
#include <qpair.h>
#include <qdatetime.h>

#include <curl/curl.h>

class QFile;

typedef QPair<QString, QString>  CKeyValue;
typedef QValueList <CKeyValue>   CKeyValueList;

class CTransfer : public QObject, public QThread {
	Q_OBJECT
public:
	class Job {
	public:
		QCString url ( ) const           { return m_url; }
		QCString effectiveUrl ( ) const  { return m_effective_url; }
		QString errorString ( ) const    { return failed ( ) ? m_error : QString::null; }
		bool failed ( ) const            { return m_finished && m_failed; }
		int responseCode ( ) const       { return m_respcode; }
		bool finished ( ) const          { return m_finished; }
		QFile *file ( ) const            { return m_file; }
		QByteArray *data ( ) const       { return m_data; }
		void *userObject ( ) const       { return m_userobject; }
		QDateTime lastModified ( ) const { QDateTime d; d.setTime_t ( m_filetime ); return d; }
		bool notModifiedSince ( ) const  { return m_not_modified; }

	private:
		friend class CTransfer;

		Job ( ) { }

	private:
		CTransfer *  m_trans;

		QCString     m_url;
		QCString     m_query;
		QCString     m_effective_url;
		QByteArray * m_data;
		QFile *      m_file;
		QString      m_error;
		void *       m_userobject;
		time_t       m_ifnewer;
		time_t       m_filetime;

		CURLcode     m_result;
		int          m_respcode     : 16;
		bool         m_finished     : 1;
		bool         m_get_or_post  : 1;
		bool         m_not_modified : 1;
		bool         m_failed       : 1;
	};

	CTransfer ( );
	virtual ~CTransfer ( );

	bool init ( );

	Job *get ( const QCString &url, const CKeyValueList &query, QFile *file = 0, void *userobject = 0, bool high_priority = false );
	Job *getIfNewer ( const QCString &url, const CKeyValueList &query, const QDateTime &ifnewer, QFile *file = 0, void *userobject = 0, bool high_priority = false );
	Job *post ( const QCString &url, const CKeyValueList &query, QFile *file = 0, void *userobject = 0, bool high_priority = false );

public slots:
	void cancelAllJobs ( );
	void setProxy ( bool enable, const QString &name, int port );

signals:
	void progress ( int total, int progress );
	void progress ( CTransfer::Job *, int total, int progress );
	void finished ( CTransfer::Job * );
	void started ( CTransfer::Job * );

	void done ( );

protected:
	enum {
		TransferStartedEvent  = QEvent::User + 0x42,
		TransferFinishedEvent = QEvent::User + 0x43,
		TransferProgressEvent = QEvent::User + 0x44
	};

	virtual void customEvent ( QCustomEvent * );

private:
	virtual void run ( );
	Job *retrieve ( bool get, const QCString &url, const CKeyValueList &query, time_t ifnewer = 0, QFile *file = 0, void *userobject = 0, bool high_priority = false );
	void cancel ( Job *j );
	void updateProgress ( int delta );

	static QCString buildQueryString ( const CKeyValueList &kvl );
	
	static size_t write_curl ( void *ptr, size_t size, size_t nmemb, void *stream );
	static int progress_curl ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow );

	static void lock_curl ( CURL * /*handle*/, curl_lock_data /*data*/, curl_lock_access /*access*/, void * /*userptr*/ );
	static void unlock_curl ( CURL * /*handle*/, curl_lock_data /*data*/, void * /*userptr*/ );

private:
	CURL *m_curl;
	int   m_total;
	int   m_progress;
	int   m_file_total;
	int   m_file_progress;
	bool  m_stop;

	bool     m_use_proxy;
	QCString m_proxy_name;
	int      m_proxy_port;

	Job *          m_active_job;
	QPtrList<Job>  m_in_queue;
	QPtrList<Job>  m_out_queue;
	QMutex         m_queue_lock;
	QWaitCondition m_queue_cond;

	static uint    s_instance_counter;
	static bool    s_global_init;
	static QMutex  s_share_lock;
	static CURLSH *s_curl_share;
};

#endif
