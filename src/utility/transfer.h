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

#include <QDateTime>
#include <QUrl>
#include <QNetworkAccessManager>

#include "threadpool.h"

class QIODevice;
class Transfer;
class TransferThread;


class TransferJob : public ThreadPoolJob // not correct anymore, but it keeps the API straight
{
public:
    ~TransferJob() override;

    static TransferJob *get(const QUrl &url, QIODevice *file = nullptr);
    static TransferJob *getIfNewer(const QUrl &url, const QDateTime &dt, QIODevice *file = nullptr);
    static TransferJob *post(const QUrl &url, QIODevice *file = nullptr);

    QUrl url() const                 { return m_url; }
    QUrl effectiveUrl() const        { return m_effective_url; }
    QString errorString() const      { return isFailed() ? m_error_string : QString(); }
    int responseCode() const         { return m_respcode; }
    QIODevice *file() const          { return m_file; }
    QByteArray *data() const         { return m_data; }
    QDateTime lastModified() const   { return m_last_modified; }
    bool wasNotModifiedSince() const { return m_was_not_modified; }

    bool abort() override;

private:
    friend class Transfer;
    friend class TransferThread;

    enum HttpMethod : uint {
        HttpGet = 0,
        HttpPost = 1
    };

    static TransferJob *create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file);

    TransferJob() = default;
    Q_DISABLE_COPY(TransferJob)

    QUrl         m_url;
    QUrl         m_effective_url;
    QByteArray * m_data;
    QIODevice *  m_file;
    QString      m_error_string;
    QDateTime    m_only_if_newer;
    QDateTime    m_last_modified;
    QNetworkReply *m_reply = nullptr;

    int          m_respcode         : 16;
    HttpMethod   m_http_method      : 1;
    bool         m_was_not_modified : 1;

    friend class TransferEngine;
};


class Transfer : public QObject
{
    Q_OBJECT
public:
    Transfer();

    bool retrieve(TransferJob *job, bool high_priority = false);

    void abortAllJobs();

    QString userAgent() const;
    QNetworkAccessManager *networkAccessManager();

    static void setDefaultUserAgent(const QString &ua);
    static QString defaultUserAgent();

signals:
    void progress(int done, int total);
    void finished(ThreadPoolJob *);
    //void started(TransferJob *);
    void jobProgress(ThreadPoolJob *, int done, int total);

protected:

public slots:
    void setUserAgent(const QString &ua);

private:
    QString                m_user_agent;
    int                    m_maxConnections;
    QNetworkAccessManager *m_nam;
    QList<TransferJob *>   m_jobs;
    QList<TransferJob *>   m_currentJobs;

    static QString s_default_user_agent;
    void schedule();
};
