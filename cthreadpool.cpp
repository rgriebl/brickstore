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
#include <QTimer>
#include <QThread>

#include "cthreadpool.h"


CThreadPoolEngine::CThreadPoolEngine(CThreadPool *pool)
    : QObject(), m_threadpool(pool), m_job(0)
{
    setObjectName("Engine");

    connect(this, SIGNAL(jobProgress(CThreadPoolJob *, int, int)), m_threadpool, SIGNAL(jobProgress(CThreadPoolJob *, int, int)), Qt::QueuedConnection);
    connect(this, SIGNAL(jobFinished(QThread *, CThreadPoolJob *)), m_threadpool, SLOT(internalJobFinished(QThread *, CThreadPoolJob *)), Qt::QueuedConnection);
    QTimer::singleShot(0, this, SLOT(ready()));
}

void CThreadPoolEngine::ready()
{
    // we are ready to accept jobs now
    m_threadpool->threadIsIdleNow(thread());
}

void CThreadPoolEngine::execute(CThreadPoolJob *job)
{
    m_job = job;
    m_job->setStatus(CThreadPoolJob::Active);
    run();
}

CThreadPoolJob *CThreadPoolEngine::job()
{
    return m_job;
}

void CThreadPoolEngine::run()
{
    if (m_job)
        m_job->run();
    finish();
}

void CThreadPoolEngine::progress(int done, int total)
{
    emit jobProgress(m_job, done, total);
}

void CThreadPoolEngine::finish()
{
    emit jobFinished(thread(), m_job);
    m_job = 0;
}


///////////////////////////////////////////////////////////////////////////////////


class CThreadPoolThread : public QThread {
    Q_OBJECT

public:
    CThreadPoolThread(CThreadPool *pool)
        : QThread(pool)
    { }

    void execute(CThreadPoolJob *job)
    {
        emit engineExecute(job);
    }

signals:
    void engineExecute(CThreadPoolJob *);

protected:
    virtual void run()
    {
        CThreadPoolEngine *engine = qobject_cast<CThreadPool *>(parent())->createThreadPoolEngine();
        connect(this, SIGNAL(engineExecute(CThreadPoolJob *)), engine, SLOT(execute(CThreadPoolJob *)), Qt::QueuedConnection);
        exec();
    }
};


////////////////////////////////////////////////////////////////

CThreadPoolJob::CThreadPoolJob()
    : m_threadpool(0), m_user_data(0), m_status(Inactive)
{ }

CThreadPoolJob::~CThreadPoolJob()
{ }

bool CThreadPoolJob::abort()
{
    if (!threadPool())
        return false;
    return threadPool()->abortJob(this);
}


/////////////////////////////////////////////////////////////////


CThreadPool::CThreadPool(int threadcount)
{
    if (threadcount < 0 || threadcount > 32)
        threadcount = QThread::idealThreadCount();

    while (threadcount--) {
        QThread *thread = new CThreadPoolThread(this);
        m_threads.append(thread);

        thread->start(QThread::IdlePriority);
    }

    m_jobs_total = m_jobs_progress = 0;
}

CThreadPool::~CThreadPool()
{
    abortAllJobs();

    foreach(QThread *thread, m_threads) {
        thread->quit();
        thread->wait();
    }
}

void CThreadPool::threadIsIdleNow(QThread *thread)
{
    QMutexLocker locker(&m_mutex);
    m_idle_threads.append(thread);

    scheduler();
}

bool CThreadPool::execute(CThreadPoolJob *job, bool high_priority)
{
    if (!job || job->threadPool())
        return false;

    job->m_threadpool = this;

    QMutexLocker locker(&m_mutex);

    if (high_priority)
        m_jobs.prepend(job);
    else
        m_jobs.append(job);

    scheduler();
    updateProgress(+1);

    return true;
}

void CThreadPool::scheduler()
{
    // has to called in the LOCKED state !!!!!!!!!!!!

    if (!m_idle_threads.isEmpty() && !m_jobs.isEmpty()) {
        QThread *thread = m_idle_threads.takeFirst();
        CThreadPoolJob *job = m_jobs.takeFirst();

        qobject_cast<CThreadPoolThread *>(thread)->execute(job);
        emit started(job);
    }
}

void CThreadPool::internalJobFinished(QThread *thread, CThreadPoolJob *job)
{
    QMutexLocker locker(&m_mutex);

    m_idle_threads.append(thread);

    locker.unlock();
    emit finished(job);
    locker.relock();
    delete job;

    scheduler();
}

bool CThreadPool::abortJob(CThreadPoolJob *job)
{
    m_mutex.lock();

    int idx = m_jobs.indexOf(job);
    if (idx < 0)
        return false;
    m_jobs.removeAt(idx);

    m_mutex.unlock();

    job->m_status = CThreadPoolJob::Aborted;
    emit finished(job);
    delete job;

    updateProgress(-1);
    return true;
}


void CThreadPool::abortAllJobs()
{
    // we have to copy the job list to avoid problems with
    // recursive mutex locking (abort emits signals, which
    // can lead to slots calling another CTransfer function)
    m_mutex.lock();
    QList<CThreadPoolJob *> inactive = m_jobs;
    m_jobs.clear();
    m_mutex.unlock();

    foreach(CThreadPoolJob *job, inactive) {
        job->m_status = CThreadPoolJob::Aborted;
        emit finished(job);
        delete job;

        updateProgress(-1);
    }
}

void CThreadPool::updateProgress(int delta)
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

CThreadPoolEngine *CThreadPool::createThreadPoolEngine()
{
    return new CThreadPoolEngine(this);
}

#include "cthreadpool.moc"
