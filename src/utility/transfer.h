/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QThread>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LogTransfer)

QT_FORWARD_DECLARE_CLASS(QIODevice)
QT_FORWARD_DECLARE_CLASS(QNetworkAccessManager)
QT_FORWARD_DECLARE_CLASS(QNetworkReply)
class Transfer;
class TransferRetriever;

class TransferJob
{
public:
    ~TransferJob();

    static TransferJob *get(const QUrl &url, QIODevice *file = nullptr, uint retries = 0);
    static TransferJob *getIfNewer(const QUrl &url, const QDateTime &dt, QIODevice *file = nullptr);
    static TransferJob *getIfDifferent(const QUrl &url, const QString &etag, QIODevice *file = nullptr);
    static TransferJob *post(const QUrl &url, QIODevice *file = nullptr, bool noRedirects = false);

    QUrl url() const                 { return m_url; }
    QUrl effectiveUrl() const        { return m_effective_url; }
    QString errorString() const;
    int responseCode() const         { return m_respcode; }
    QUrl redirectUrl() const         { return m_redirect_url; }
    QIODevice *file() const          { return m_file; }
    QByteArray *data() const         { return m_data; }
    QDateTime lastModified() const   { return m_last_modified; }
    QString lastETag() const         { return m_last_etag; }
    bool wasNotModified() const      { return m_was_not_modified; }

    bool isActive() const            { return m_status == Active; }

    bool isCompleted() const         { return m_status == Completed; }
    bool isFailed() const            { return m_status == Failed; }
    bool isAborted() const           { return m_status == Aborted; }

    void setNoRedirects(bool noRedirects) { m_no_redirects = noRedirects; }
    void setUserData(const QByteArray &tag, const QVariant &v) { m_userTag = tag; m_userData = v; }
    QVariant userData(const QByteArray &tag) const             { return m_userTag == tag ? m_userData : QVariant(); }
    QByteArray userTag() const                                 { return m_userTag; }

    Transfer *transfer() const       { return m_transfer; }
    void abort();

    void resetForReuse();

private:
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

    static TransferJob *create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, const QString &etag,
                               QIODevice *file, bool noRedirects, uint retries = 0);

    void setStatus(Status st)  { m_status = st; }
    bool abortInternal();

    TransferJob() = default;
    Q_DISABLE_COPY(TransferJob)

    Transfer *   m_transfer = nullptr;
    QUrl         m_url;
    QUrl         m_effective_url;
    QUrl         m_redirect_url;
    QByteArray * m_data = nullptr;
    QIODevice *  m_file = nullptr;
    QString      m_error_string;
    QString      m_only_if_different;
    QString      m_last_etag;
    QDateTime    m_only_if_newer;
    QDateTime    m_last_modified;
    QNetworkReply *m_reply = nullptr;

    QByteArray   m_userTag;
    QVariant     m_userData;
//    void *       m_user_ptr = nullptr;
//    int          m_user_tag = 0;

    uint         m_respcode         : 16 = 0;
    uint         m_status           : 4 = Inactive;
    uint         m_http_method      : 1;
    int          m_reset_for_reuse  : 1 = false;
    uint         m_retries_left     : 4;
    int          m_was_not_modified : 1 = false;
    int          m_no_redirects     : 1;

    friend class Transfer;
    friend class TransferRetriever;
};

Q_DECLARE_METATYPE(TransferJob *)


class TransferRetriever : public QObject
{
    Q_OBJECT
public:
    TransferRetriever(Transfer *transfer);
    ~TransferRetriever() override;

    void addJob(TransferJob *job, bool highPriority);
    void abortJob(TransferJob *job);
    void abortAllJobs();
    void schedule();

signals:
    void overallProgress(int done, int total);
    void started(TransferJob *job);
    void progress(TransferJob *job, int done, int total);
    void finished(TransferJob *job);

private:
    void downloadFinished(QNetworkReply *reply);

    Transfer *m_transfer;
    QNetworkAccessManager *m_nam = nullptr;
    QVector<TransferJob *> m_jobs;
    QVector<TransferJob *> m_currentJobs;
    int                    m_maxConnections;
    int                    m_progressDone = 0;
    int                    m_progressTotal = 0;
    QByteArray             m_sslSession;
};


class Transfer : public QObject
{
    Q_OBJECT
public:
    Transfer(QObject *parent = nullptr);
    ~Transfer() override;

    void retrieve(TransferJob *job, bool highPriority = false);

    void abortJob(TransferJob *job);
    void abortAllJobs();

    QString userAgent() const;

    static void setDefaultUserAgent(const QString &ua);
    static QString defaultUserAgent();

    static void setInitFunction(std::function<void()> func);

signals:
    void overallProgress(int done, int total);
    void started(TransferJob *);
    void progress(TransferJob *, int done, int total);
    void finished(TransferJob *);

protected:

public slots:
    void setUserAgent(const QString &ua);

private:
    void internalFinished(TransferJob *job);

    QThread *m_retrieverThread;
    TransferRetriever *m_retriever;
    QString m_user_agent;

    static QString s_default_user_agent;
    static std::function<void()> s_threadInitFunction;
};
