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

#include <QThread>
#include <QFile>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QUrlQuery>

#include "utility.h"
#include "transfer.h"
#include "config.h"


TransferJob::~TransferJob()
{
    Q_ASSERT(!m_reply);

    delete m_file;
    delete m_data;
}

TransferJob *TransferJob::get(const QUrl &url, QIODevice *file)
{
    return create(HttpGet, url, QDateTime(), file, false);
}

TransferJob *TransferJob::getIfNewer(const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    return create(HttpGet, url, ifnewer, file, false);
}

TransferJob *TransferJob::post(const QUrl &url, QIODevice *file, bool noRedirects)
{
    return create(HttpPost, url, QDateTime(), file, noRedirects);
}

bool TransferJob::abort()
{
    if (m_reply)
        m_reply->abort();
    else
        setStatus(TransferJob::Aborted);
    return true;
}

TransferJob *TransferJob::create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer,
                                 QIODevice *file, bool noRedirects)
{
    if (url.isEmpty())
        return nullptr;

    auto *j = new TransferJob();

    j->m_url = url;
    if (j->m_url.scheme().isEmpty())
        j->m_url.setScheme("http"_l1);
    j->m_only_if_newer = ifnewer;
    j->m_data = file ? nullptr : new QByteArray();
    j->m_file = file ? file : nullptr;
    j->m_http_method = method;
    j->m_status = Inactive;
    j->m_respcode = 0;
    j->m_was_not_modified = false;
    j->m_no_redirects = noRedirects;

    return j;
}

// ===========================================================================
// ===========================================================================
// ===========================================================================

QString Transfer::s_default_user_agent;

Transfer::Transfer(QObject *parent)
    : QObject(parent)
{
    if (s_default_user_agent.isEmpty())
        s_default_user_agent = "%1/%2"_l1.arg(qApp->applicationName(), qApp->applicationVersion());
    m_user_agent = s_default_user_agent;


    m_retriever = new TransferRetriever(this);
    m_retrieverThread = QThread::create([this]() {
        QEventLoop eventLoop;
        int returnCode = eventLoop.exec();
        delete m_retriever;
        return returnCode;
    });
    m_retriever->moveToThread(m_retrieverThread);
    m_retrieverThread->setObjectName("TransferRetriever"_l1);
    m_retrieverThread->setParent(this);
    m_retrieverThread->start(QThread::LowPriority);

    connect(m_retriever, &TransferRetriever::finished,
            this, [this](TransferJob *job) {
        if (!job->isActive())
            emit finished(job);
        delete job;
    }, Qt::QueuedConnection);
    connect(m_retriever, &TransferRetriever::jobProgress,
            this, &Transfer::jobProgress, Qt::QueuedConnection);
    connect(m_retriever, &TransferRetriever::progress,
            this, &Transfer::progress, Qt::QueuedConnection);
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

bool Transfer::retrieve(TransferJob *job, bool highPriority)
{
    Q_ASSERT(!job->m_transfer);
    job->m_transfer = this;

    QMetaObject::invokeMethod(m_retriever, [this, job, highPriority]() {
        m_retriever->addJob(job, highPriority);
    }, Qt::QueuedConnection);
    return true;
}

void Transfer::abortJob(TransferJob *job)
{
    QMetaObject::invokeMethod(m_retriever, [this, job]() {
        m_retriever->abortJob(job);
    }, Qt::QueuedConnection);
}

void Transfer::abortAllJobs()
{
    QMetaObject::invokeMethod(m_retriever, &TransferRetriever::abortAllJobs, Qt::QueuedConnection);
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


#include "moc_transfer.cpp"

TransferRetriever::TransferRetriever(Transfer *transfer)
    : QObject()
    , m_transfer(transfer)
    , m_maxConnections(6) // mirror the internal QNAM setting
{ }

TransferRetriever::~TransferRetriever()
{
    abortAllJobs();
}

void TransferRetriever::addJob(TransferJob *job, bool highPriority)
{
    if (highPriority)
        m_jobs.prepend(job);
    else
        m_jobs.append(job);

    emit m_transfer->progress(m_progressDone, ++m_progressTotal);
    schedule();
}

void TransferRetriever::abortJob(TransferJob *j)
{
    j->abort();

    if (m_jobs.removeOne(j)) {
        emit finished(j);

        m_progressDone++;
        emit progress(m_progressDone, m_progressTotal);
        if (m_progressDone == m_progressTotal)
            m_progressDone = m_progressTotal = 0;
    }
}

void TransferRetriever::abortAllJobs()
{
    for (auto &j : qAsConst(m_jobs)) {
        j->abort();
        emit finished(j);
    }

    m_progressDone += m_jobs.size();
    emit progress(m_progressDone, m_progressTotal);
    if (m_progressDone == m_progressTotal)
        m_progressDone = m_progressTotal = 0;

    m_jobs.clear();

    for (auto &j : qAsConst(m_currentJobs))
        j->abort();
}

void TransferRetriever::schedule()
{
    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }

    while ((m_currentJobs.size() <= m_maxConnections) && !m_jobs.isEmpty()) {
        auto j = m_jobs.takeFirst();

        bool isget = (j->m_http_method == TransferJob::HttpGet);
        QUrl url = j->url();
        j->m_effective_url = url;

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, m_transfer->userAgent());
        if (j->m_no_redirects) {
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        }
        j->setStatus(TransferJob::Active);
        if (isget) {
            if (j->m_only_if_newer.isValid())
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
                req.setHeader(QNetworkRequest::IfModifiedSinceHeader, j->m_only_if_newer);
#else
                req.setRawHeader("if-modified-since", QVariant(j->m_only_if_newer).toByteArray());
#endif
            j->m_reply = m_nam->get(req);
        }
        else {
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded"_l1);
            QByteArray postdata = url.query(QUrl::FullyEncoded).toLatin1();
            url.setQuery(QUrlQuery());
            req.setUrl(url);
            j->m_reply = m_nam->post(req, postdata);
        }
        connect(j->m_reply, &QNetworkReply::finished, this, [this, j]() {
            auto error = j->m_reply->error();

            j->m_respcode = j->m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt();
            j->m_effective_url = j->m_reply->url();

            if (error != QNetworkReply::NoError) {
                j->m_error_string = j->m_reply->errorString();
                j->setStatus(TransferJob::Failed);
            } else {
                switch (j->m_respcode) {
                case 304:
                    if (j->m_only_if_newer.isValid()) {
                        j->m_was_not_modified = true;
                        j->setStatus(TransferJob::Completed);
                    }
                    else {
                        j->setStatus(TransferJob::Failed);
                    }
                    break;

                case 200: {
                    auto lastmod = j->m_reply->header(QNetworkRequest::LastModifiedHeader);
                    if (lastmod.isValid())
                        j->m_last_modified = lastmod.toDateTime();
                    if (j->m_data)
                        *j->m_data = j->m_reply->readAll();
                    else if (j->m_file) {
                        j->m_file->write(j->m_reply->readAll());
                        j->m_file->close();
                    }
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

            emit progress(++m_progressDone, m_progressTotal);
            if (m_progressDone == m_progressTotal)
                m_progressDone = m_progressTotal = 0;

            emit finished(j); // the thread adapter lambda in Transfer will delete the job

            m_currentJobs.removeAll(j);

            QMetaObject::invokeMethod(this, &TransferRetriever::schedule, Qt::QueuedConnection);

        });

        connect(j->m_reply, &QNetworkReply::downloadProgress, this, [this, j](qint64 recv, qint64 total) {
            emit jobProgress(j, int(recv), int(total));
        });

        m_currentJobs.append(j);
    }
}
