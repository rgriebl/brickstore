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
#ifndef __CTHREADPOOL_H__
#define __CTHREADPOOL_H__

#include <QObject>
#include <QList>
#include <QMutex>


class CThreadPoolJob;
class CThreadPoolEngine;

class CThreadPool : public QObject {
    Q_OBJECT
public:
    CThreadPool();
    CThreadPool(int threadcount);
    virtual ~CThreadPool();

    void init(int threadcount);

    bool execute(CThreadPoolJob *job, bool high_priority = false);

protected:
    virtual CThreadPoolEngine *createThreadPoolEngine();

public slots:
    bool abortJob(CThreadPoolJob *job);
    void abortAllJobs();

signals:
    void progress(int done, int total);
    void done();

    void finished(CThreadPoolJob *);
    void started(CThreadPoolJob *);
    void jobProgress(CThreadPoolJob *, int done, int total);

private slots:
    void internalJobFinished(QThread *thread, CThreadPoolJob *job);

private:
    void updateProgress(int delta);
    void scheduler();

    void threadIsIdleNow(QThread *thread);
    friend class CThreadEngine;

private:
    int                   m_jobs_total;
    int                   m_jobs_progress;

    QMutex                m_mutex;
    QList<CThreadPoolJob *> m_jobs;
    QList<QThread *>      m_threads;
    QList<QThread *>      m_idle_threads;

    friend class CThreadPoolThread;
    friend class CThreadPoolEngine;
};


////////////////////////////////////////////////////////////////////////////////////////////


class CThreadPoolEngine : public QObject {
    Q_OBJECT

public:
    CThreadPoolEngine(CThreadPool *pool);

protected:
    virtual void run();
    CThreadPoolJob *job();

protected slots:
    void progress(int done, int total);
    void finish();

signals:
    void jobFinished(QThread *thread, CThreadPoolJob *job);
    void jobProgress(CThreadPoolJob *, int, int);

private slots:
    void ready();
    void execute(CThreadPoolJob *job);

private:
    CThreadPool *   m_threadpool;
    CThreadPoolJob *m_job;
};


/////////////////////////////////////////////////////////////////////////////////////


class CThreadPoolJob {
public:
    virtual ~CThreadPoolJob();

    CThreadPool *threadPool() const  { return m_threadpool; }

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

protected:
    CThreadPoolJob();
    void setStatus(Status st)        { m_status = st; }
    virtual void run()               { }

private:
    CThreadPool *m_threadpool;
    void *       m_user_data;
    Status       m_status;

    friend class CThreadPool;
    friend class CThreadPoolEngine;
};

#endif
