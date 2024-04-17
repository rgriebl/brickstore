// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>
#include <QThread>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LogTransfer)

QT_FORWARD_DECLARE_CLASS(QIODevice)
QT_FORWARD_DECLARE_CLASS(QNetworkAccessManager)
QT_FORWARD_DECLARE_CLASS(QNetworkReply)
QT_FORWARD_DECLARE_CLASS(QNetworkCookieJar)
class Transfer;
class TransferRetriever;

class TransferJob
{
public:
    ~TransferJob();

    static TransferJob *get(const QUrl &url, const QUrlQuery &query = { });
    static TransferJob *post(const QUrl &url, const QUrlQuery &query = { });
    static TransferJob *post(const QUrl &url, const QUrlQuery &query, const QString &contentType, const QByteArray &content);

    QUrl url() const                 { return m_url; }
    QUrl effectiveUrl() const        { return m_effective_url; }
    QString errorString() const;
    int responseCode() const         { return m_respcode; }
    QUrl redirectUrl() const         { return m_redirect_url; }
    QIODevice *file() const          { return m_file; }
    QByteArray data() const          { return m_data; }
    QString lastETag() const         { return m_last_etag; }
    bool wasNotModified() const      { return m_was_not_modified; }
    bool isHighPriority() const      { return m_high_priority; }

    bool isInactive() const          { return m_status == Inactive; }
    bool isActive() const            { return m_status == Active; }

    bool isCompleted() const         { return m_status == Completed; }
    bool isFailed() const            { return m_status == Failed; }
    bool isAborted() const           { return m_status == Aborted; }

    void setNoRedirects(bool noRedirects) { m_no_redirects = noRedirects; }
    void setMaximumRetries(uint count)    { m_retries_left = std::max(31u, count); }
    void setOnlyIfDifferent(const QString &etag) { m_only_if_different = etag; }
    void setOutputDevice(QIODevice *output);
    void setUserData(const QByteArray &tag, const QVariant &v) { m_userTag = tag; m_userData = v; }
    QVariant userData(const QByteArray &tag) const             { return m_userTag == tag ? m_userData : QVariant(); }
    QByteArray userTag() const                                 { return m_userTag; }

    Transfer *transfer() const       { return m_transfer; }
    void abort();

    void reprioritize(bool highPriority);
    void resetForReuse(bool applyRedirect = false);

    void setAutoDelete(bool autoDelete) { m_auto_delete = autoDelete; }
    bool autoDelete() const             { return m_auto_delete; }

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

    static TransferJob *create(HttpMethod method, const QUrl &url, const QUrlQuery &query, const QString &contentType, const QByteArray &content);

    void setStatus(Status st)  { m_status = st; }
    bool abortInternal();

    TransferJob() = default;
    Q_DISABLE_COPY(TransferJob)

    Transfer *   m_transfer = nullptr;
    QUrl         m_url;
    QUrl         m_effective_url;
    QUrl         m_redirect_url;
    QByteArray   m_data;
    QIODevice *  m_file = nullptr;
    QString      m_error_string;
    QString      m_only_if_different;
    QString      m_last_etag;
    QNetworkReply *m_reply = nullptr;
    QString      m_postContentType;
    QByteArray   m_postContent;

    QByteArray   m_userTag;
    QVariant     m_userData;

    uint         m_respcode         : 16 = 0;
    Status       m_status           : 4 = Inactive;
    HttpMethod   m_http_method      : 1;
    bool         m_reset_for_reuse  : 1 = false;
    uint         m_retries_left     : 4 = 0;
    bool         m_was_not_modified : 1 = false;
    bool         m_no_redirects     : 1 = false;
    bool         m_high_priority    : 1 = false;
    bool         m_auto_delete      : 1 = true;

    friend class Transfer;
    friend class TransferRetriever;
};

Q_DECLARE_METATYPE(TransferJob *)

class TransferRetriever : public QObject
{
    Q_OBJECT
public:
    TransferRetriever(Transfer *transfer, QNetworkCookieJar *cookieJar);
    ~TransferRetriever() override;

    void setCookieJar(QNetworkCookieJar *cookieJar);

    void addJob(TransferJob *job, bool highPriority);
    void reprioritizeJob(TransferJob *job, bool highPriority);
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
    QNetworkCookieJar *    m_cookieJar = nullptr;
    QVector<TransferJob *> m_jobs;
    QVector<TransferJob *> m_currentJobs;
    int                    m_maxConnections;
    int                    m_progressDone = 0;
    int                    m_progressTotal = 0;
    QHash<QString, QByteArray> m_sslSessionForHost;
};


class Transfer : public QObject
{
    Q_OBJECT
public:
    Transfer(QObject *parent = nullptr);
    // be careful: cookieJar is moved to the retriever thread!
    Transfer(QNetworkCookieJar *&&cookieJar, QObject *parent = nullptr);
    ~Transfer() override;

    void retrieve(TransferJob *job, bool highPriority = false);
    void reprioritize(TransferJob *job, bool highPriority);

    void abortJob(TransferJob *job);
    void abortAllJobs();

    QString userAgent() const;

    static void setDefaultUserAgent(const QString &ua);
    static QString defaultUserAgent();

    static void setInitFunction(const std::function<void ()> &func);

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
