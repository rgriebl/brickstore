/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __TRANSFER_H__
#define __TRANSFER_H__

#include <QDateTime>
#include <QUrl>
#include <QNetworkProxy>

#include "threadpool.h"


class QIODevice;
class Transfer;
class TransferThread;

class TransferJob : public ThreadPoolJob {
public:
    ~TransferJob();

    Transfer *transfer() const      { return qobject_cast<Transfer *>(threadPool()); }

    static TransferJob *get(const QUrl &url, QIODevice *file = 0);
    static TransferJob *getIfNewer(const QUrl &url, const QDateTime &dt, QIODevice *file = 0);
    static TransferJob *post(const QUrl &url, QIODevice *file = 0);

    QUrl url() const                 { return m_url; }
    QUrl effectiveUrl() const        { return m_effective_url; }
    QString errorString() const      { return isFailed() ? m_error_string : QString::null; }
    int responseCode() const         { return m_respcode; }
    QIODevice *file() const          { return m_file; }
    QByteArray *data() const         { return m_data; }
    QDateTime lastModified() const   { return m_last_modified; }
    bool wasNotModifiedSince() const { return m_was_not_modified; }

private:
    friend class Transfer;
    friend class TransferThread;

    enum HttpMethod {
        HttpGet = 0,
        HttpPost = 1
    };

    static TransferJob *create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file);
    TransferJob();

private:
    QUrl         m_url;
    QUrl         m_effective_url;
    QByteArray * m_data;
    QIODevice *  m_file;
    QString      m_error_string;
    QDateTime    m_only_if_newer;
    QDateTime    m_last_modified;

    int          m_respcode         : 16;
    HttpMethod   m_http_method      : 1;
    bool         m_was_not_modified : 1;

    friend class TransferEngine;
};


class Transfer : public ThreadPool {
    Q_OBJECT
public:
    Transfer(int threadcount);

    inline bool retrieve(TransferJob *job, bool high_priority = false)
    { return execute(job, high_priority); }

    QNetworkProxy proxy() const  { return m_proxy; }
    QString userAgent() const    { return m_user_agent; }

    static void setDefaultUserAgent(const QString &ua)   { s_default_user_agent = ua; }
    static QString defaultUserAgent()                    { return s_default_user_agent; }

signals:
    void finished(TransferJob *);
    void started(TransferJob *);
    void jobProgress(TransferJob *, int done, int total);

protected:
    virtual ThreadPoolEngine *createThreadPoolEngine();

public slots:
    void setProxy(const QNetworkProxy &proxy);
    void setUserAgent(const QString &ua);

private:
    QNetworkProxy m_proxy;
    QString       m_user_agent;

    static QString s_default_user_agent;
};

#endif
