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

#include <QObject>
#include <QList>
#include <QMutex>
#include <QDateTime>
#include <QUrl>
#include <QNetworkProxy>

class QIODevice;
class CTransfer;
class CTransferThread;

class CTransferJob {
public:
    ~CTransferJob();

    CTransfer *transfer() const      { return m_transfer; }

    static CTransferJob *get(const QUrl &url, QIODevice *file = 0);
    static CTransferJob *getIfNewer(const QUrl &url, const QDateTime &dt, QIODevice *file = 0);
    static CTransferJob *post(const QUrl &url, QIODevice *file = 0);

    enum Status {
        Inactive = 0,
        Active,
        Completed,
        Failed,
        Aborted
    };

    Status status() const            { return m_status; }
    bool isActive() const            { return m_status == Active; }
	bool isCompleted() const         { return m_status == Completed; }
	bool isFailed() const            { return m_status == Failed; }
	bool isAborted() const           { return m_status == Aborted; }
    bool abort();

    template<typename T>
    void setUserData(T *t)           { m_user_data = static_cast<void *>(t); }

    template<typename T>
	T *userData() const              { return static_cast<T *>(m_user_data); }

	QUrl url() const                 { return m_url; }
	QUrl effectiveUrl() const        { return m_effective_url; }
	QString errorString() const      { return isFailed() ? m_error_string : QString::null; }
	int responseCode() const         { return m_respcode; }
	QIODevice *file() const          { return m_file; }
	QByteArray *data() const         { return m_data; }
	QDateTime lastModified() const   { return m_last_modified; }
	bool wasNotModifiedSince() const { return m_was_not_modified; }

private:
	friend class CTransfer;
	friend class CTransferThread;

    enum HttpMethod {
        HttpGet = 0,
        HttpPost = 1
    };

    static CTransferJob *create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file);
	CTransferJob();

private:
	CTransfer *  m_transfer;

	QUrl         m_url;
	QUrl         m_effective_url;
	QByteArray * m_data;
	QIODevice *  m_file;
	QString      m_error_string;
	void *       m_user_data;
	QDateTime    m_only_if_newer;
	QDateTime    m_last_modified;

	int          m_respcode         : 16;
    Status       m_status           : 3;
	HttpMethod   m_http_method      : 1;
	bool         m_was_not_modified : 1;

	friend class CTransferEngine;
};





class CTransfer : public QObject {
	Q_OBJECT
public:
	CTransfer(int threadcount);
	virtual ~CTransfer ( );

	bool retrieve(CTransferJob *job, bool high_priority = false);

public slots:
    bool abortJob(CTransferJob *job);
	void abortAllJobs();
	void setProxy(const QNetworkProxy &proxy);

signals:
	void progress(int done, int total);
	void done();

    void finished(CTransferJob *);
	void started(CTransferJob *);
    void dataReadProgress(CTransferJob *, int done, int total);
    void dataSendProgress(CTransferJob *, int done, int total);

private slots:
    void internalJobFinished(QThread *thread, CTransferJob *job);

private:
    void updateProgress ( int delta );
    void scheduler();

    void threadIsIdleNow(QThread *thread);
    friend class CTransferEngine;

private:
//	int   m_file_total;
//	int   m_file_progress;

    int                   m_bytes_per_sec;
    int                   m_jobs_total;
    int                   m_jobs_progress;


	QMutex                m_mutex;
    QNetworkProxy         m_proxy;
	QList<CTransferJob *> m_jobs;
	QList<QThread *>      m_threads;
    QList<QThread *>      m_idle_threads;

};

#endif
