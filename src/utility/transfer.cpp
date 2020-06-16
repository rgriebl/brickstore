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

#if 0
// copied from Qt4.4 network access
static QDateTime fromHttpDate(const QByteArray &value)
{
    // HTTP dates have three possible formats:
    //  RFC 1123/822      -   ddd, dd MMM yyyy hh:mm:ss "GMT"
    //  RFC 850           -   dddd, dd-MMM-yy hh:mm:ss "GMT"
    //  ANSI C's asctime  -   ddd MMM d hh:mm:ss yyyy
    // We only handle them exactly.If they deviate, we bail out.

    int pos = value.indexOf(',');
    QDateTime dt;
    if (pos == -1) {
        // no comma ->asctime(3) format
        dt = QDateTime::fromString(QString::fromLatin1(value), Qt::TextDate);
    } else {
        // eat the weekday, the comma and the space following it
        QString sansWeekday = QString::fromLatin1(value.constData() + pos + 2);

        QLocale c = QLocale::c();
        if (pos == 3)
            // must be RFC 1123 date
            dt = c.toDateTime(sansWeekday, QLatin1String("dd MMM yyyy hh:mm:ss 'GMT"));
        else
            // must be RFC 850 date
            dt = c.toDateTime(sansWeekday, QLatin1String("dd-MMM-yy hh:mm:ss 'GMT'"));
    }

    if (dt.isValid())
        dt.setTimeSpec(Qt::UTC);
    return dt;
}

static QByteArray toHttpDate(const QDateTime &dt)
{
    return QLocale::c().toString(dt, QLatin1String("ddd, dd MMM yyyy hh:mm:ss 'GMT'")).toLatin1();
}
#endif

// ===========================================================================
// ===========================================================================
// ===========================================================================
/*

class TransferEngine : public ThreadPoolEngine {
    Q_OBJECT

public:
    TransferEngine(Transfer *trans)
        : ThreadPoolEngine(trans)
    {
        m_user_agent = trans->userAgent();
        m_nam = new QNetworkAccessManager(this);
        m_nam->setProxy(trans->proxy());
    }

protected:

protected slots:
    void responseHeaderReceived(const QHttpResponseHeader &resp)
    {
        TransferJob *j = static_cast<TransferJob *>(job());

        j->m_respcode = resp.statusCode();
        j->m_error_string = resp.reasonPhrase();

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
            QString lm = resp.value(QLatin1String("Last-Modified"));
            if (!lm.isEmpty())
                j->m_last_modified = fromHttpDate(lm.toLatin1());

            QList<QNetworkCookie> cookies;
            foreach (const QString &cookie, resp.allValues(QLatin1String("Set-Cookie")))
                cookies += QNetworkCookie::parseCookies(cookie.toAscii());
            
            if (!cookies.isEmpty()) {
                s_cookie_mutex.lock();
                s_cookies.setCookiesFromUrl(cookies, j->m_effective_url);
                s_cookie_mutex.unlock();
            }
            break;
        }
        default:
            j->setStatus(TransferJob::Failed);
            break;
        }
    }

    void readyRead(const QHttpResponseHeader &resp)
    {
        if (resp.statusCode() != 200)
            return;

        TransferJob *j = static_cast<TransferJob *>(job());

        QByteArray ba = m_http->readAll();
        if (j->m_data)
            j->m_data->append(ba);
        else if (j->m_file)
            j->m_file->write(ba);

    }

    void finished(int id, bool error)
    {
        TransferJob *j = static_cast<TransferJob *>(job());

        if (j && id == m_job_http_id) {
            if (j->isActive()) {
                if (error) {
                    j->m_error_string = m_http->errorString();

                    if (j->m_error_string.isEmpty())
                        j->m_error_string = QLatin1String("Internal Error");
                    j->setStatus(TransferJob::Failed);
                }
                else {
                    j->setStatus(TransferJob::Completed);
                }
            }

            if (!j->isActive()) {
                finish();
            }
        }
    }

protected:
    QNetworkAccessManager *m_nam;
    int m_job_http_id;
    QString m_user_agent;
    QUrl m_last_url;
    
};
*/

// ===========================================================================
// ===========================================================================
// ===========================================================================


// ===========================================================================
// ===========================================================================
// ===========================================================================

TransferJob::TransferJob()
{
}

TransferJob::~TransferJob()
{
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
    //TODO5
    return false;
}

TransferJob *TransferJob::create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    if (url.isEmpty())
        return 0;

    TransferJob *j = new TransferJob();

    j->m_url = url;
    if (j->m_url.scheme().isEmpty())
        j->m_url.setScheme("http");
    j->m_only_if_newer = ifnewer;
    j->m_data = file ? 0 : new QByteArray();
    j->m_file = file ? file : 0;
    j->m_http_method = method;

    j->m_respcode = 0;
    j->m_was_not_modified = false;

    return j;
}

// ===========================================================================
// ===========================================================================
// ===========================================================================

QString Transfer::s_default_user_agent;

Transfer::Transfer(int maxConnections)
    : m_maxConnections(maxConnections)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    if (s_default_user_agent.isEmpty())
        s_default_user_agent = QString("%1/%2").arg(qApp->applicationName()).arg(qApp->applicationVersion());
    m_user_agent = s_default_user_agent;
}

void Transfer::setProxy(const QNetworkProxy &proxy)
{
    m_nam->setProxy(proxy);
}

void Transfer::setUserAgent(const QString &ua)
{
    m_user_agent = ua;
}

bool Transfer::retrieve(TransferJob *job, bool high_priority)
{
    if (high_priority)
        m_jobs.prepend(job);
    else
        m_jobs.append(job);

    schedule();
    return true;
}

void Transfer::abortAllJobs()
{

}

void Transfer::schedule()
{
    if ((m_currentJobs.size() <= m_maxConnections) && !m_jobs.isEmpty()) {
        auto j = m_jobs.takeFirst();

        bool isget = (j->m_http_method == TransferJob::HttpGet);
        QUrl url = j->url();
        j->m_effective_url = url;

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, m_user_agent);

        if (isget) {
            if (j->m_only_if_newer.isValid())
                req.setHeader(QNetworkRequest::IfModifiedSinceHeader, j->m_only_if_newer);
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
            emit progress(m_currentJobs.size(), m_jobs.size() + m_currentJobs.size());

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

