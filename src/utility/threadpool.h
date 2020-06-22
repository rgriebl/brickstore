/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#pragma once

#include <QObject>
#include <QList>
#include <QMutex>
#include <QPair>

class ThreadPoolJob;
class ThreadPoolEngine;


class ThreadPool : public QObject
{
    Q_OBJECT
public:
    ThreadPool();
    ThreadPool(int threadcount);
    virtual ~ThreadPool();

    void init(int threadcount);

    bool execute(ThreadPoolJob *job, bool high_priority = false);

protected:
    virtual ThreadPoolEngine *createThreadPoolEngine();

public slots:
    bool abortJob(ThreadPoolJob *job);
    void abortAllJobs();

signals:
    void progress(int done, int total);
    void done();

    void finished(ThreadPoolJob *);
    void started(ThreadPoolJob *);
    void jobProgress(ThreadPoolJob *, int done, int total);

private slots:
    void internalJobFinished(QThread *thread, ThreadPoolJob *job);

private:
    void updateProgress(int delta);
    void scheduler();

    void threadIsIdleNow(QThread *thread);
    friend class ThreadEngine;

private:
    int                   m_jobs_total;
    int                   m_jobs_progress;

    QMutex                m_mutex;
    QList<ThreadPoolJob *> m_jobs;
    QList<QThread *>      m_threads;
    QList<QThread *>      m_idle_threads;

    friend class ThreadPoolThread;
    friend class ThreadPoolEngine;
};


////////////////////////////////////////////////////////////////////////////////////////////


class ThreadPoolEngine : public QObject
{
    Q_OBJECT

public:
    ThreadPoolEngine(ThreadPool *pool);
    void execute(ThreadPoolJob *job);

protected:
    virtual void run();
    ThreadPoolJob *job();

protected slots:
    void progress(int done, int total);
    void finish();

signals:
    void jobFinished(QThread *thread, ThreadPoolJob *job);
    void jobProgress(ThreadPoolJob *, int, int);

private slots:
    void ready();

private:
    ThreadPool *   m_threadpool;
    ThreadPoolJob *m_job;
};


/////////////////////////////////////////////////////////////////////////////////////


class ThreadPoolJob
{
public:
    virtual ~ThreadPoolJob();

    ThreadPool *threadPool() const  { return m_threadpool; }

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
    virtual bool abort();

    template<typename T>
    void setUserData(int tag, T *t)  { m_user_tag = tag; m_user_ptr = static_cast<void *>(t); }

    template<typename T>
    QPair<int, T *> userData() const { return qMakePair(m_user_tag, static_cast<T *>(m_user_ptr)); }

    template<typename T>
    T *userData(int tag) const       { return tag == m_user_tag ? static_cast<T *>(m_user_ptr) : 0; }

protected:
    ThreadPoolJob();
    void setStatus(Status st)        { m_status = st; }
    virtual void run()               { }

private:
    Q_DISABLE_COPY(ThreadPoolJob)

    ThreadPool *m_threadpool;
    void *       m_user_ptr;
    int          m_user_tag;
    Status       m_status;

    friend class ThreadPool;
    friend class ThreadPoolEngine;
};
