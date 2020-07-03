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

#include <QThread>
#include <QFile>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QUrlQuery>

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
    return create(HttpGet, url, QDateTime(), file);
}

TransferJob *TransferJob::getIfNewer(const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    return create(HttpGet, url, ifnewer, file);
}

TransferJob *TransferJob::post(const QUrl &url, QIODevice *file)
{
    return create(HttpPost, url, QDateTime(), file);
}

bool TransferJob::abort()
{
    if (m_reply)
        m_reply->abort();
    return true;
}

TransferJob *TransferJob::create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    if (url.isEmpty())
        return nullptr;

    auto *j = new TransferJob();

    j->m_url = url;
    if (j->m_url.scheme().isEmpty())
        j->m_url.setScheme("http");
    j->m_only_if_newer = ifnewer;
    j->m_data = file ? nullptr : new QByteArray();
    j->m_file = file ? file : nullptr;
    j->m_http_method = method;

    j->m_respcode = 0;
    j->m_was_not_modified = false;

    return j;
}

// ===========================================================================
// ===========================================================================
// ===========================================================================

QString Transfer::s_default_user_agent;

Transfer::Transfer()
    : m_maxConnections(6) // mirror the internal QNAM setting
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    if (s_default_user_agent.isEmpty())
        s_default_user_agent = QString("%1/%2").arg(qApp->applicationName(), qApp->applicationVersion());
    m_user_agent = s_default_user_agent;
}

Transfer::~Transfer()
{
    abortAllJobs();
}

void Transfer::setUserAgent(const QString &ua)
{
    m_user_agent = ua;
}

bool Transfer::retrieve(TransferJob *job, bool high_priority)
{
    Q_ASSERT(!job->m_transfer);
    job->m_transfer = this;

    if (high_priority)
        m_jobs.prepend(job);
    else
        m_jobs.append(job);

    emit progress(m_progressDone, ++m_progressTotal);

    schedule();
    return true;
}

void Transfer::abortAllJobs()
{
    qDeleteAll(m_jobs);
    m_jobs.clear();
    for (TransferJob *job : m_currentJobs)
        job->abort();
}

void Transfer::schedule()
{
    while ((m_currentJobs.size() <= m_maxConnections) && !m_jobs.isEmpty()) {
        auto j = m_jobs.takeFirst();

        bool isget = (j->m_http_method == TransferJob::HttpGet);
        QUrl url = j->url();
        j->m_effective_url = url;

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, m_user_agent);

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
            req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));
            QByteArray postdata = url.query(QUrl::FullyEncoded).toLatin1();
            j->m_reply = m_nam->post(req, postdata);
        }
        connect(j->m_reply, &QNetworkReply::finished, this, [this, j]() {
            auto error = j->m_reply->error();

            if (error != QNetworkReply::NoError) {
                j->m_error_string = j->m_reply->errorString();
                j->setStatus(TransferJob::Failed);
            } else {
                j->m_respcode = j->m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                j->m_effective_url = j->m_reply->url();

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

            if (!j->isActive())
                emit finished(j);

            m_currentJobs.removeAll(j);
            delete j;

            emit progress(++m_progressDone, m_progressTotal);
            if (m_progressDone == m_progressTotal)
                m_progressDone = m_progressTotal = 0;

            QMetaObject::invokeMethod(this, &Transfer::schedule, Qt::QueuedConnection);

        });

        connect(j->m_reply, &QNetworkReply::downloadProgress, this, [this, j](qint64 recv, qint64 total) {
            emit jobProgress(j, int(recv), int(total));
        });

        m_currentJobs.append(j);
    }
}

QString Transfer::userAgent() const
{
    return m_user_agent;
}

QNetworkAccessManager *Transfer::networkAccessManager()
{
    return m_nam;
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
