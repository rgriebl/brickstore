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

#include "threadpool.h"


ThreadPoolEngine::ThreadPoolEngine(ThreadPool *pool)
    : QObject(), m_threadpool(pool), m_job(0)
{
    setObjectName("Engine");

    connect(this, SIGNAL(jobProgress(ThreadPoolJob *, int, int)), m_threadpool, SIGNAL(jobProgress(ThreadPoolJob *, int, int)), Qt::QueuedConnection);
    connect(this, SIGNAL(jobFinished(QThread *, ThreadPoolJob *)), m_threadpool, SLOT(internalJobFinished(QThread *, ThreadPoolJob *)), Qt::QueuedConnection);
    QTimer::singleShot(0, this, SLOT(ready()));
}

void ThreadPoolEngine::ready()
{
    // we are ready to accept jobs now
    m_threadpool->threadIsIdleNow(thread());
}

void ThreadPoolEngine::execute(ThreadPoolJob *job)
{
    m_job = job;
    m_job->setStatus(ThreadPoolJob::Active);
    run();
}

ThreadPoolJob *ThreadPoolEngine::job()
{
    return m_job;
}

void ThreadPoolEngine::run()
{
    if (m_job)
        m_job->run();
    finish();
}

void ThreadPoolEngine::progress(int done, int total)
{
    emit jobProgress(m_job, done, total);
}

void ThreadPoolEngine::finish()
{
    emit jobFinished(thread(), m_job);
    m_job = 0;
}


///////////////////////////////////////////////////////////////////////////////////


class ThreadPoolThread : public QThread {
    Q_OBJECT

public:
    ThreadPoolThread(ThreadPool *pool)
        : QThread(pool)
    { }

    void execute(ThreadPoolJob *job)
    {
        emit engineExecute(job);
    }

signals:
    void engineExecute(ThreadPoolJob *);

protected:
    virtual void run()
    {
        ThreadPoolEngine *engine = qobject_cast<ThreadPool *>(parent())->createThreadPoolEngine();
        connect(this, SIGNAL(engineExecute(ThreadPoolJob *)), engine, SLOT(execute(ThreadPoolJob *)), Qt::QueuedConnection);
        exec();
    }
};


////////////////////////////////////////////////////////////////

ThreadPoolJob::ThreadPoolJob()
    : m_threadpool(0), m_user_tag(0), m_user_ptr(0), m_status(Inactive)
{ }

ThreadPoolJob::~ThreadPoolJob()
{ }

bool ThreadPoolJob::abort()
{
    if (!threadPool())
        return false;
    return threadPool()->abortJob(this);
}


/////////////////////////////////////////////////////////////////


ThreadPool::ThreadPool()
{
    init(0);
}

ThreadPool::ThreadPool(int threadcount)
{
    init(threadcount);
}

void ThreadPool::init(int threadcount)
{
    m_jobs_total = m_jobs_progress = 0;

    if (threadcount < 0 || threadcount > 32)
        threadcount = QThread::idealThreadCount();

    while (threadcount--) {
        QThread *thread = new ThreadPoolThread(this);
        m_threads.append(thread);

        thread->start(QThread::IdlePriority);
    }
}

ThreadPool::~ThreadPool()
{
    abortAllJobs();

    foreach(QThread *thread, m_threads) {
        thread->quit();
        thread->wait();
    }
}

void ThreadPool::threadIsIdleNow(QThread *thread)
{
    QMutexLocker locker(&m_mutex);
    m_idle_threads.append(thread);

    scheduler();
}

bool ThreadPool::execute(ThreadPoolJob *job, bool high_priority)
{
    if (!job || job->threadPool() || m_threads.isEmpty())
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

void ThreadPool::scheduler()
{
    // has to called in the LOCKED state !!!!!!!!!!!!

    if (!m_idle_threads.isEmpty() && !m_jobs.isEmpty()) {
        QThread *thread = m_idle_threads.takeFirst();
        ThreadPoolJob *job = m_jobs.takeFirst();

        qobject_cast<ThreadPoolThread *>(thread)->execute(job);
        emit started(job);
    }
}

void ThreadPool::internalJobFinished(QThread *thread, ThreadPoolJob *job)
{
    QMutexLocker locker(&m_mutex);

    m_idle_threads.append(thread);

    locker.unlock();
    emit finished(job);
    locker.relock();
    delete job;

    updateProgress(-1);
    scheduler();
}

bool ThreadPool::abortJob(ThreadPoolJob *job)
{
    m_mutex.lock();

    int idx = m_jobs.indexOf(job);
    if (idx < 0)
        return false;
    m_jobs.removeAt(idx);

    m_mutex.unlock();

    job->m_status = ThreadPoolJob::Aborted;
    emit finished(job);
    delete job;

    updateProgress(-1);
    return true;
}


void ThreadPool::abortAllJobs()
{
    // we have to copy the job list to avoid problems with
    // recursive mutex locking (abort emits signals, which
    // can lead to slots calling another Transfer function)
    m_mutex.lock();
    QList<ThreadPoolJob *> inactive = m_jobs;
    m_jobs.clear();
    m_mutex.unlock();

    foreach(ThreadPoolJob *job, inactive) {
        job->m_status = ThreadPoolJob::Aborted;
        emit finished(job);
        delete job;

        updateProgress(-1);
    }
}

void ThreadPool::updateProgress(int delta)
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

ThreadPoolEngine *ThreadPool::createThreadPoolEngine()
{
    return new ThreadPoolEngine(this);
}

#include "threadpool.moc"
