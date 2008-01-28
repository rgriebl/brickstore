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

#include <QThread>
#include <QFile>
#include <QLocale>
#include <QHttp>
#include <QTimer>

#include "capplication.h"
#include "ctransfer.h"



// copied from Qt4.4 network access
static QDateTime fromHttpDate(const QByteArray &value)
{
    // HTTP dates have three possible formats:
    //  RFC 1123/822      -   ddd, dd MMM yyyy hh:mm:ss "GMT"
    //  RFC 850           -   dddd, dd-MMM-yy hh:mm:ss "GMT"
    //  ANSI C's asctime  -   ddd MMM d hh:mm:ss yyyy
    // We only handle them exactly.If they deviate, we bail out.

    int pos = value.indexOf(',');
    QDateTime dt;
    if (pos == -1) {
        // no comma ->asctime(3) format
        dt = QDateTime::fromString(QString::fromLatin1(value), Qt::TextDate);
    } else {
        // eat the weekday, the comma and the space following it
        QString sansWeekday = QString::fromLatin1(value.constData() + pos + 2);

        QLocale c = QLocale::c();
        if (pos == 3)
            // must be RFC 1123 date
            dt = c.toDateTime(sansWeekday, QLatin1String("dd MMM yyyy hh:mm:ss 'GMT"));
        else
            // must be RFC 850 date
            dt = c.toDateTime(sansWeekday, QLatin1String("dd-MMM-yy hh:mm:ss 'GMT'"));
    }

    if (dt.isValid())
        dt.setTimeSpec(Qt::UTC);
    return dt;
}

static QByteArray toHttpDate(const QDateTime &dt)
{
    return QLocale::c().toString(dt, QLatin1String("ddd, dd MMM yyyy hh:mm:ss 'GMT'")).toLatin1();
}


// ===========================================================================
// ===========================================================================
// ===========================================================================


class CTransferEngine : public QObject {
    Q_OBJECT

public:
    CTransferEngine(CTransfer *trans)
        : QObject(), m_transfer(trans)
    {
        setObjectName("Engine");

        m_user_agent = cApp->appName() + "/" +
                       cApp->appVersion() + " (" +
                       cApp->sysName() + " " +
                       cApp->sysVersion() + "; http://" +
                       cApp->appURL() + ")";

        m_http = new QHttp(this);
        connect(m_http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), this, SLOT(responseHeaderReceived(const QHttpResponseHeader &)));
        connect(m_http, SIGNAL(readyRead(const QHttpResponseHeader &)), this, SLOT(readyRead(const QHttpResponseHeader &)));
        connect(m_http, SIGNAL(dataReadProgress(int, int)), this, SLOT(relayDataReadProgress(int, int)));
        connect(m_http, SIGNAL(dataSendProgress(int, int)), this, SLOT(relayDataSendProgress(int, int)));
        connect(m_http, SIGNAL(requestFinished(int, bool)), this, SLOT(finished(int, bool)));

        connect(this, SIGNAL(dataReadProgress(CTransferJob *, int, int)), m_transfer, SIGNAL(dataReadProgress(CTransferJob *, int, int)), Qt::QueuedConnection);
        connect(this, SIGNAL(dataSendProgress(CTransferJob *, int, int)), m_transfer, SIGNAL(dataSendProgress(CTransferJob *, int, int)), Qt::QueuedConnection);
        connect(this, SIGNAL(jobFinished(QThread *, CTransferJob *)), m_transfer, SLOT(internalJobFinished(QThread *, CTransferJob *)), Qt::QueuedConnection);
        QTimer::singleShot(0, this, SLOT(ready()));
    }

public slots:
    void execute(CTransferJob *job)
    {
        m_job = job;
        m_job->m_effective_url = m_job->m_url;
        m_job->m_status = CTransferJob::Active;

        download();
    }

signals:
    void jobFinished(QThread *thread, CTransferJob *job);
    void dataReadProgress(CTransferJob *, int, int);
    void dataSendProgress(CTransferJob *, int, int);

protected:
    void download()
    {
        bool isget = (m_job->m_http_method == CTransferJob::HttpGet);
        QUrl url = m_job->effectiveUrl();

        if (url.host() != m_last_url.host() || url.port() != m_last_url.port())
            m_http->setHost(url.host(),url.port(80));

        QHttpRequestHeader req;
        QByteArray postdata;

        if (isget) {
            QString path = url.path();
            if (url.hasQuery())
                path = path + "?" + url.encodedQuery();

            req.setRequest("GET", path);

            if (m_job->m_only_if_newer.isValid())
                req.setValue("If-Modified-Since", toHttpDate(m_job->m_only_if_newer));
        }
        else {
            req.setRequest("POST", url.path());
            QByteArray postdata = url.encodedQuery();
        }
        req.setValue("Host", url.host());
        req.setValue("User-Agent", m_user_agent);

        //qDebug() << req.toString();

        m_job_http_id = m_http->request(req, postdata, m_job->m_file);

        m_last_url = url;
    }


protected slots:
    void ready()
    {
        // we are ready to accept jobs now
        m_transfer->threadIsIdleNow(thread());
    }

    void responseHeaderReceived(const QHttpResponseHeader &resp)
    {
        m_job->m_respcode = resp.statusCode();
        m_job->m_error_string = resp.reasonPhrase();

        switch (m_job->m_respcode) {
        case 301:
        case 302:
        case 303:
        case 307: {
            QUrl redirect = resp.value("Location");
            if (!redirect.isEmpty()) {
                m_job->m_effective_url = m_job->m_effective_url.resolved(redirect);
                download();
                //not finished yet!
            }
            else {
                m_job->m_status = CTransferJob::Failed;
            }
            break;
        }
        case 304: if (m_job->m_only_if_newer.isValid()) {
                m_job->m_was_not_modified = true;
                m_job->m_status = CTransferJob::Completed;
            }
            else {
                m_job->m_status = CTransferJob::Failed;
            }
            break;

        case 200: m_job->m_last_modified = fromHttpDate(resp.value("Last-Modified").toLatin1());
            break;

        default : m_job->m_status = CTransferJob::Failed;
            break;
        }
    }

    void readyRead(const QHttpResponseHeader &resp)
    {
        if (resp.statusCode() != 200)
            return;

        QByteArray ba = m_http->readAll();
        if (m_job->m_data)
            m_job->m_data->append(ba);
        else if (m_job->m_file)
            m_job->m_file->write(ba);

    }

    void relayDataReadProgress(int done, int total)
    {
        emit dataReadProgress(m_job, done, total);
    }

    void relayDataSendProgress(int done, int total)
    {
        emit dataSendProgress(m_job, done, total);
    }

    void finished(int id, bool error)
    {
        if (m_job && id == m_job_http_id) {
            if (m_job->isActive()) {
                if (error) {
                    m_job->m_error_string = m_http->errorString();

                    if (m_job->m_error_string.isEmpty())
                        m_job->m_error_string = QLatin1String("Internal Error");
                    m_job->m_status = CTransferJob::Failed;
                }
                else {
                    m_job->m_status = CTransferJob::Completed;
                }
            }

            if (!m_job->isActive()) {
                emit jobFinished(thread(), m_job);
                m_job = 0;
            }
        }
    }

protected:
    QHttp *m_http;
    CTransfer *m_transfer;
    CTransferJob *m_job;
    int m_job_http_id;
    QString m_user_agent;
    QUrl m_last_url;
};

// ===========================================================================
// ===========================================================================
// ===========================================================================


class CTransferThread : public QThread {
    Q_OBJECT

public:
    CTransferThread(CTransfer *trans)
        : QThread(trans)
    { }

    void execute(CTransferJob *job)
    {
        emit engineExecute(job);
    }

signals:
    void engineExecute(CTransferJob *);

protected:
    virtual void run()
    {
        CTransferEngine *engine = new CTransferEngine(qobject_cast<CTransfer *>(parent()));
        connect(this, SIGNAL(engineExecute(CTransferJob *)), engine, SLOT(execute(CTransferJob *)), Qt::QueuedConnection);
        exec();
    }
};

// ===========================================================================
// ===========================================================================
// ===========================================================================

CTransferJob::CTransferJob()
{
}

CTransferJob::~CTransferJob()
{
    delete m_file;
    delete m_data;
}

CTransferJob *CTransferJob::get(const QUrl &url, QIODevice *file)
{
    return create(HttpGet, url, QDateTime(), file);
}

CTransferJob *CTransferJob::getIfNewer(const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    return create(HttpGet, url, ifnewer, file);
}

CTransferJob *CTransferJob::post(const QUrl &url, QIODevice *file)
{
    return create(HttpPost, url, QDateTime(), file);
}

CTransferJob *CTransferJob::create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    if (url.isEmpty())
        return 0;

    CTransferJob *j = new CTransferJob();

    j->m_url = url;
    j->m_only_if_newer = ifnewer;
    j->m_data = file ? 0 : new QByteArray();
    j->m_file = file ? file : 0;
    j->m_user_data = 0;
    j->m_http_method = method;
    j->m_transfer = 0;

    j->m_respcode = 0;
    j->m_was_not_modified = false;
    j->m_status = Inactive;

    return j;
}

bool CTransferJob::abort()
{
    if (!transfer())
        return false;
    return transfer()->abortJob(this);
}

// ===========================================================================
// ===========================================================================
// ===========================================================================


CTransfer::CTransfer(int threadcount)
{
    if (threadcount < 0 || threadcount > 32)
        threadcount = QThread::idealThreadCount();

    while (threadcount--) {
        QThread *thread = new CTransferThread(this);
        m_threads.append(thread);

        thread->start(QThread::IdlePriority);
    }

    m_jobs_total = m_jobs_progress = 0;
}

CTransfer::~CTransfer()
{
    //qDebug ( "stopping CTransfer thread" );

    abortAllJobs();

    foreach(QThread *thread, m_threads) {
        thread->quit();
        thread->wait();
    }
}

void CTransfer::setProxy(const QNetworkProxy &proxy)
{
    m_mutex.lock();
    m_proxy = proxy;
    m_mutex.unlock();
}

void CTransfer::threadIsIdleNow(QThread *thread)
{
    QMutexLocker locker(&m_mutex);
    m_idle_threads.append(thread);

    scheduler();
}

bool CTransfer::retrieve(CTransferJob *job, bool high_priority)
{
    if (!job || job->transfer())
        return false;

    job->m_transfer = this;

    QMutexLocker locker(&m_mutex);

    if (high_priority)
        m_jobs.prepend(job);
    else
        m_jobs.append(job);

    scheduler();
    updateProgress(+1);

    return true;
}

void CTransfer::scheduler()
{
    // has to called in the LOCKED state !!!!!!!!!!!!

    if (!m_idle_threads.isEmpty() && !m_jobs.isEmpty()) {
        QThread *thread = m_idle_threads.takeFirst();
        CTransferJob *job = m_jobs.takeFirst();

        qobject_cast<CTransferThread *>(thread)->execute(job);
        emit started(job);
    }
}

void CTransfer::internalJobFinished(QThread *thread, CTransferJob *job)
{
    QMutexLocker locker(&m_mutex);

    m_idle_threads.append(thread);

    locker.unlock();
    emit finished(job);
    locker.relock();
    delete job;

    scheduler();
}

bool CTransfer::abortJob(CTransferJob *job)
{
    m_mutex.lock();

    int idx = m_jobs.indexOf(job);
    if (idx < 0)
        return false;
    m_jobs.removeAt(idx);

    m_mutex.unlock();

    job->m_status = CTransferJob::Aborted;
    emit finished(job);
    delete job;

    updateProgress(-1);
    return true;
}


void CTransfer::abortAllJobs()
{
    // we have to copy the job list to avoid problems with
    // recursive mutex locking (abort emits signals, which
    // can lead to slots calling another CTransfer function)
    m_mutex.lock();
    QList<CTransferJob *> inactive = m_jobs;
    m_jobs.clear();
    m_mutex.unlock();

    foreach(CTransferJob *job, inactive) {
        job->m_status = CTransferJob::Aborted;
        emit finished(job);
        delete job;

        updateProgress(-1);
    }
}

void CTransfer::updateProgress(int delta)
{
    if (delta > 0)
        m_jobs_total += delta;
    else
        m_jobs_progress -= delta;

    emit progress(m_jobs_progress, m_jobs_total);

    if (m_jobs_total == m_jobs_progress) {
        m_jobs_total = m_jobs_progress = 0;

        emit progress(0, 0);
        emit done();
    }
}


#include "ctransfer.moc"
