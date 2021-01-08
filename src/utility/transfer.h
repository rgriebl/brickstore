/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

QT_FORWARD_DECLARE_CLASS(QIODevice)
class Transfer;


class TransferJob
{
public:
    ~TransferJob();

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

    bool abort();

    bool isActive() const            { return m_status == Active; }
    bool isCompleted() const         { return m_status == Completed; }
    bool isFailed() const            { return m_status == Failed; }
    bool isAborted() const           { return m_status == Aborted; }

    template<typename T>
    void setUserData(int tag, T *t)  { m_user_tag = tag; m_user_ptr = static_cast<void *>(t); }

    template<typename T>
    QPair<int, T *> userData() const { return qMakePair(m_user_tag, static_cast<T *>(m_user_ptr)); }

    template<typename T>
    T *userData(int tag) const       { return tag == m_user_tag ? static_cast<T *>(m_user_ptr) : 0; }

private:
    friend class Transfer;
    friend class TransferThread;

    enum Status : uint {
        Inactive = 0,
        Active,
        Completed,
        Failed,
        Aborted
    };

    enum HttpMethod : uint {
        HttpGet = 0,
        HttpPost = 1
    };

    static TransferJob *create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file);

    void setStatus(Status st)  { m_status = st; }

    TransferJob() = default;
    Q_DISABLE_COPY(TransferJob)

    Transfer *   m_transfer = nullptr;
    void *       m_user_ptr = nullptr;
    int          m_user_tag = 0;
    QUrl         m_url;
    QUrl         m_effective_url;
    QByteArray * m_data = nullptr;
    QIODevice *  m_file = nullptr;
    QString      m_error_string;
    QDateTime    m_only_if_newer;
    QDateTime    m_last_modified;
    QNetworkReply *m_reply = nullptr;

    int          m_respcode         : 16;
    int          m_status           : 4;
    uint         m_http_method      : 1;
    bool         m_was_not_modified : 1;

    friend class TransferEngine;
};


class Transfer : public QObject
{
    Q_OBJECT
public:
    Transfer();
    ~Transfer() override;

    bool retrieve(TransferJob *job, bool high_priority = false);

    void abortAllJobs();

    QString userAgent() const;
    QNetworkAccessManager *networkAccessManager();

    static void setDefaultUserAgent(const QString &ua);
    static QString defaultUserAgent();

signals:
    void progress(int done, int total);
    void finished(TransferJob *);
    void jobProgress(TransferJob *, int done, int total);

protected:

public slots:
    void setUserAgent(const QString &ua);

private:
    QString                m_user_agent;
    int                    m_maxConnections;
    QNetworkAccessManager *m_nam;
    QList<TransferJob *>   m_jobs;
    QList<TransferJob *>   m_currentJobs;
    int                    m_progressDone = 0;
    int                    m_progressTotal = 0;

    static QString s_default_user_agent;
    void schedule();
};
