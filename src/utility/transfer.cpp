// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QThread>
#include <QFile>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QTimer>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QUrlQuery>

#include "transfer.h"

Q_LOGGING_CATEGORY(LogTransfer, "bs.transfer", QtWarningMsg)


TransferJob::~TransferJob()
{
    Q_ASSERT(!m_reply);

    delete m_file;
    delete m_data;
}

TransferJob *TransferJob::get(const QUrl &url, QIODevice *file, uint retries)
{
    Q_ASSERT(retries < 31);

    return create(HttpGet, url, { }, { }, file, false, retries);
}

TransferJob *TransferJob::getIfNewer(const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    return create(HttpGet, url, ifnewer, { }, file, false);
}

TransferJob *TransferJob::getIfDifferent(const QUrl &url, const QString &etag, QIODevice *file)
{
    return create(HttpGet, url, { }, etag, file, false);
}

TransferJob *TransferJob::post(const QUrl &url, QIODevice *file, bool noRedirects)
{
    return create(HttpPost, url, { }, { }, file, noRedirects);
}

TransferJob *TransferJob::postContent(const QUrl &url, const QString &contentType, const QByteArray &content, QIODevice *file, bool noRedirects)
{
    auto job = post(url, file, noRedirects);
    job->m_postContentType = contentType;
    job->m_postContent = content;
    return job;
}

QString TransferJob::errorString() const
{
    return isFailed() ? m_error_string
                      : (isAborted() ? QCoreApplication::translate("Transfer", "Aborted")
                                     : QString { });
}

void TransferJob::abort()
{
    if (m_transfer)
        m_transfer->abortJob(this);
    else
        setStatus(TransferJob::Aborted);
}

void TransferJob::reprioritize(bool highPriority)
{
    if (isInactive() && m_transfer)
        m_transfer->reprioritize(this, highPriority);
}

void TransferJob::resetForReuse(bool applyRedirect)
{
    m_reset_for_reuse = true;
    if (m_file) {
        m_file->seek(0);
        if (auto f = qobject_cast<QFileDevice *>(m_file))
            f->resize(0);
    } else {
        m_data->clear();
    }
    if (applyRedirect) {
        if (m_redirect_url.isEmpty())
            qCWarning(LogTransfer) << "Redirect target for" << m_url << "is empty";
        m_url = m_redirect_url;
    }
    m_respcode = 0;
    m_status = Inactive;
    m_was_not_modified = false;
    m_effective_url.clear();
    m_redirect_url.clear();
    m_error_string.clear();
}

bool TransferJob::abortInternal()
{
    if (m_reply)
        m_reply->abort();
    else
        setStatus(TransferJob::Aborted);
    return true;
}

TransferJob *TransferJob::create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer,
                                 const QString &etag, QIODevice *file, bool noRedirects, uint retries)
{
    if (url.isEmpty())
        return nullptr;

    auto *j = new TransferJob();

    j->m_url = url;
    if (j->m_url.scheme().isEmpty())
        j->m_url.setScheme(u"http"_qs);
    j->m_only_if_newer = ifnewer.toUTC();
    j->m_only_if_different = etag;
    j->m_data = file ? nullptr : new QByteArray();
    j->m_file = file ? file : nullptr;
    j->m_http_method = method;
    j->m_retries_left = retries;
    j->m_no_redirects = noRedirects;

    return j;
}


// ===========================================================================
// ===========================================================================
// ===========================================================================

QString Transfer::s_default_user_agent;
std::function<void()> Transfer::s_threadInitFunction;


Transfer::Transfer(QObject *parent)
    : Transfer(nullptr, parent)
{ }

Transfer::Transfer(QNetworkCookieJar *&&cookieJar, QObject *parent)
    : QObject(parent)
{
    if (s_default_user_agent.isEmpty())
        s_default_user_agent = qApp->applicationName() + u'/' + qApp->applicationVersion();
    m_user_agent = s_default_user_agent;

    m_retriever = new TransferRetriever(this, std::move(cookieJar));
    m_retrieverThread = QThread::create([this]() {
        if (s_threadInitFunction)
            s_threadInitFunction();
        QEventLoop eventLoop;
        int returnCode = eventLoop.exec();
        delete m_retriever;
        return returnCode;
    });
    m_retriever->moveToThread(m_retrieverThread);
    m_retrieverThread->setObjectName(u"TransferRetriever"_qs);
    m_retrieverThread->setParent(this);
    m_retrieverThread->start(QThread::LowPriority);

    connect(m_retriever, &TransferRetriever::started,
            this, &Transfer::started, Qt::QueuedConnection);
    connect(m_retriever, &TransferRetriever::finished,
            this, [this](TransferJob *job) {
        if (!job->isActive()) {
            emit finished(job);

            if (job->m_reset_for_reuse) {
                job->m_reset_for_reuse = false;
                job->m_transfer = nullptr;
                return;
            }
        }
        if (job->autoDelete())
            delete job;
    }, Qt::QueuedConnection);
    connect(m_retriever, &TransferRetriever::progress,
            this, &Transfer::progress, Qt::QueuedConnection);
    connect(m_retriever, &TransferRetriever::overallProgress,
            this, &Transfer::overallProgress, Qt::QueuedConnection);
}

Transfer::~Transfer()
{
    abortAllJobs();
    m_retrieverThread->quit();
    m_retrieverThread->wait();
}

void Transfer::setUserAgent(const QString &ua)
{
    m_user_agent = ua;
}

void Transfer::retrieve(TransferJob *job, bool highPriority)
{
    if (!job)
        return;
    Q_ASSERT(!job->m_transfer);
    job->m_transfer = this;
    job->m_high_priority = highPriority;

    QMetaObject::invokeMethod(m_retriever, [this, job, highPriority]() {
        m_retriever->addJob(job, highPriority);
    }, Qt::QueuedConnection);
}

void Transfer::reprioritize(TransferJob *job, bool highPriority)
{
    if (!job || (job->m_transfer != this) || !job->isInactive())
        return;

    QMetaObject::invokeMethod(m_retriever, [this, job, highPriority]() {
        m_retriever->reprioritizeJob(job, highPriority);
    }, Qt::QueuedConnection);
}

void Transfer::abortJob(TransferJob *job)
{
    QMetaObject::invokeMethod(m_retriever, [this, job]() {
        m_retriever->abortJob(job);
    }, Qt::QueuedConnection);
}

void Transfer::abortAllJobs()
{
    QMetaObject::invokeMethod(m_retriever, &TransferRetriever::abortAllJobs, Qt::BlockingQueuedConnection);
}

QString Transfer::userAgent() const
{
    return m_user_agent;
}

void Transfer::setDefaultUserAgent(const QString &ua)
{
    s_default_user_agent = ua;
}

QString Transfer::defaultUserAgent()
{
    return s_default_user_agent;
}

void Transfer::setInitFunction(const std::function<void ()> &func)
{
    s_threadInitFunction = func;
}


#include "moc_transfer.cpp"

TransferRetriever::TransferRetriever(Transfer *transfer, QNetworkCookieJar *cookieJar)
    : QObject()
    , m_transfer(transfer)
    , m_cookieJar(cookieJar)
    , m_maxConnections(6) // mirror the internal QNAM setting
{
    if (cookieJar)
        cookieJar->setParent(this);
}

TransferRetriever::~TransferRetriever()
{
    abortAllJobs();
    delete m_nam;
}

void TransferRetriever::addJob(TransferJob *job, bool highPriority)
{
    if (job->isAborted()) {
        emit finished(job);
        emit m_transfer->overallProgress(++m_progressDone, ++m_progressTotal);
    } else {
        if (highPriority)
            m_jobs.prepend(job);
        else
            m_jobs.append(job);

        emit m_transfer->overallProgress(m_progressDone, ++m_progressTotal);
        schedule();
    }
}

void TransferRetriever::reprioritizeJob(TransferJob *job, bool highPriority)
{
    if (job->isInactive()) {
        auto idx = m_jobs.indexOf(job);
        if (idx != -1) {
            job->m_high_priority = highPriority;
            m_jobs.move(idx, highPriority ? 0 : m_jobs.size());
        }
    }
}

void TransferRetriever::abortJob(TransferJob *j)
{
    j->abortInternal();

    if (m_jobs.removeOne(j)) {
        emit finished(j);

        m_progressDone++;
        emit overallProgress(m_progressDone, m_progressTotal);
        if (m_progressDone == m_progressTotal)
            m_progressDone = m_progressTotal = 0;
    }
}

void TransferRetriever::abortAllJobs()
{
    for (auto &j : std::as_const(m_jobs)) {
        j->abortInternal();
        emit finished(j);
    }

    m_progressDone += m_jobs.size();
    emit overallProgress(m_progressDone, m_progressTotal);
    if (m_progressDone == m_progressTotal)
        m_progressDone = m_progressTotal = 0;

    m_jobs.clear();

    for (auto &j : std::as_const(m_currentJobs))
        j->abortInternal();
}

void TransferRetriever::schedule()
{
    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        if (m_cookieJar)
            m_nam->setCookieJar(m_cookieJar);
        m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        connect(m_nam, &QNetworkAccessManager::finished,
                this, &TransferRetriever::downloadFinished);
    }

    while ((m_currentJobs.size() < m_maxConnections) && !m_jobs.isEmpty()) {
        auto j = m_jobs.takeFirst();

        bool isget = (j->m_http_method == TransferJob::HttpGet);
        QUrl url = j->url();
        j->m_effective_url = url;

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false); // QTBUG-105043
        req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        req.setHeader(QNetworkRequest::UserAgentHeader, m_transfer->userAgent());
        if (j->m_no_redirects) {
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        }

#if QT_CONFIG(ssl)
        auto ssl = req.sslConfiguration();
        ssl.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);
        QByteArray sslSession = m_sslSessionForHost.value(url.host());
        if (!sslSession.isEmpty())
            ssl.setSessionTicket(sslSession);
        req.setSslConfiguration(ssl);
#endif
        j->setStatus(TransferJob::Active);
        if (isget) {
            if (j->m_only_if_newer.isValid())
                req.setHeader(QNetworkRequest::IfModifiedSinceHeader, j->m_only_if_newer);
            if (!j->m_only_if_different.isEmpty())
                req.setHeader(QNetworkRequest::IfNoneMatchHeader, j->m_only_if_different);
            j->m_reply = m_nam->get(req);
        } else {
            QByteArray postdata;

            if (!j->m_postContentType.isEmpty()) {
                req.setHeader(QNetworkRequest::ContentTypeHeader, j->m_postContentType);
                postdata = j->m_postContent;
            } else {
                req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_qs);
                postdata = url.query(QUrl::FullyEncoded).toLatin1();
                url.setQuery(QUrlQuery());
                req.setUrl(url);
            }
            j->m_reply = m_nam->post(req, postdata);
        }

        qCInfo(LogTransfer) << (isget ? ">> GET" : ">> POST") << req.url();
        if (LogTransfer().isDebugEnabled()) {
            const auto headers = j->m_reply->request().rawHeaderList();
            for (const auto &header : headers)
                qCDebug(LogTransfer()) << header << ":" << j->m_reply->request().rawHeader(header);
        }

        j->m_reply->setProperty("bsJob", QVariant::fromValue(j));

        connect(j->m_reply, &QNetworkReply::downloadProgress, this, [this, j](qint64 recv, qint64 total) {
            emit progress(j, int(recv), int(total));
        });

        connect(j->m_reply, &QNetworkReply::metaDataChanged, this, [j]() {
            qCInfo(LogTransfer) << "<< REPLY" << j->m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt()
                                << j->m_effective_url;
            if (LogTransfer().isDebugEnabled()) {
                const auto headers = j->m_reply->rawHeaderList();
                for (const auto &header : headers)
                    qCDebug(LogTransfer()) << header << ":" << j->m_reply->rawHeader(header);
            }
        });

        m_currentJobs.append(j);
        emit started(j);
    }
}

void TransferRetriever::downloadFinished(QNetworkReply *reply)
{
    auto *j = reply->property("bsJob").value<TransferJob *>();
    auto error = j->m_reply->error();

    j->m_respcode = j->m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt();
    j->m_effective_url = j->m_reply->url();

    if (error != QNetworkReply::NoError) {
        m_sslSessionForHost.remove(j->m_url.host());

        if ((j->m_respcode == 404) && j->m_retries_left) {
            --j->m_retries_left;
            j->m_reply->deleteLater();
            j->m_reply = m_nam->get(j->m_reply->request());
            j->m_reply->setProperty("bsJob", QVariant::fromValue(j));
            qCWarning(LogTransfer) << "Got a 404 on" << j->m_url << "... retrying (still" << j->m_retries_left << "retries left)";
            return;
        } else if ((j->m_respcode == 302) && (error == QNetworkReply::HostNotFoundError)) {
            // this only happens on Windows, starting in April 2021: BL sends a relative redirect,
            // but QNAM thinks it's absolute, leading to a host-not-found error.
            if ((j->m_http_method == TransferJob::HttpGet) && j->m_reply->url().isRelative()) {
                j->m_reply->deleteLater();
                QUrl url = j->m_reply->url();
                url.setHost(j->m_url.host());
                url.setScheme(j->m_url.scheme());
                j->m_reply = m_nam->get(QNetworkRequest(url));
                j->m_reply->setProperty("bsJob", QVariant::fromValue(j));
                return;
            }
        }
        j->m_error_string = j->m_reply->errorString();
        j->setStatus(TransferJob::Failed);
    } else {
#if QT_CONFIG(ssl)
        m_sslSessionForHost.insert(j->m_url.host(), reply->sslConfiguration().sessionTicket());
#endif
        switch (j->m_respcode) {
        case 304:
            if (j->m_only_if_newer.isValid() || !j->m_only_if_different.isEmpty()) {
                j->m_was_not_modified = true;
                j->setStatus(TransferJob::Completed);
            } else {
                j->setStatus(TransferJob::Failed);
            }
            break;

        case 302:
        case 303:
        case 307: {
            j->m_redirect_url = j->m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            if (j->m_redirect_url.isRelative())
                j->m_redirect_url = j->m_reply->url().resolved(j->m_redirect_url);
            j->setStatus(TransferJob::Completed);
            break;
        }
        case 200: {
            auto lastetag = j->m_reply->header(QNetworkRequest::ETagHeader);
            if (lastetag.isValid())
                j->m_last_etag = lastetag.toString();
            auto lastmod = j->m_reply->header(QNetworkRequest::LastModifiedHeader);
            if (lastmod.isValid())
                j->m_last_modified = lastmod.toDateTime();
            if (j->m_data)
                *j->m_data = j->m_reply->readAll();
            else if (j->m_file)
                j->m_file->write(j->m_reply->readAll());
            j->setStatus(TransferJob::Completed);
            break;
        }
        default:
            j->setStatus(TransferJob::Failed);
            break;
        }
    }
    j->m_reply->deleteLater();
    j->m_reply = nullptr;

    emit overallProgress(++m_progressDone, m_progressTotal);
    if (m_progressDone == m_progressTotal)
        m_progressDone = m_progressTotal = 0;

    emit finished(j); // the thread adapter lambda in Transfer will delete the job

    m_currentJobs.removeAll(j);

    QMetaObject::invokeMethod(this, &TransferRetriever::schedule, Qt::QueuedConnection);
}
