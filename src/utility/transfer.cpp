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

#include <QThread>
#include <QFile>
#include <QLocale>
#include <QHttp>
#include <QTimer>
#include <QNetworkCookieJar>
#include <QCoreApplication>

#include "transfer.h"


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


// ===========================================================================
// ===========================================================================
// ===========================================================================


class TransferEngine : public ThreadPoolEngine {
    Q_OBJECT

public:
    TransferEngine(Transfer *trans)
        : ThreadPoolEngine(trans)
    {
        m_user_agent = trans->userAgent();
        m_http = new QHttp(this);
        m_http->setProxy(trans->proxy());

        connect(m_http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), this, SLOT(responseHeaderReceived(const QHttpResponseHeader &)));
        connect(m_http, SIGNAL(readyRead(const QHttpResponseHeader &)), this, SLOT(readyRead(const QHttpResponseHeader &)));
        connect(m_http, SIGNAL(dataReadProgress(int, int)), this, SLOT(progress(int, int)));
        connect(m_http, SIGNAL(dataSendProgress(int, int)), this, SLOT(progress(int, int)));
        connect(m_http, SIGNAL(requestFinished(int, bool)), this, SLOT(finished(int, bool)));
    }

protected:
    virtual void run()
    {
        TransferJob *j = static_cast<TransferJob *>(job());

        j->m_effective_url = j->m_url;

        download();
    }

    void download()
    {
        TransferJob *j = static_cast<TransferJob *>(job());

        bool isget = (j->m_http_method == TransferJob::HttpGet);
        QUrl url = j->effectiveUrl();

        if (url.host() != m_last_url.host() || url.port() != m_last_url.port())
            m_http->setHost(url.host(), url.port(80));

        QHttpRequestHeader req;
        QByteArray postdata;

        if (isget) {
            QString path = url.path();
            if (url.hasQuery())
                path = path + QLatin1String("?") + url.encodedQuery();

            req.setRequest("GET", path);

            if (j->m_only_if_newer.isValid())
                req.setValue(QLatin1String("If-Modified-Since"), toHttpDate(j->m_only_if_newer));
        }
        else {
            req.setRequest(QLatin1String("POST"), url.path());
            postdata = url.encodedQuery();
            req.setContentType(QLatin1String("application/x-www-form-urlencoded"));
            req.setContentLength(postdata.size());
        }
        req.setValue(QLatin1String("Host"), url.host());
        req.setValue(QLatin1String("User-Agent"), m_user_agent);

        s_cookie_mutex.lock();
        QList<QNetworkCookie> cookies = s_cookies.cookiesForUrl(j->m_effective_url);
        s_cookie_mutex.unlock();
        
        if (!cookies.isEmpty()) {
            QByteArray rawcookies;
            
            foreach (QNetworkCookie c, cookies) {
                rawcookies += c.toRawForm(QNetworkCookie::NameAndValueOnly);
                rawcookies += ';';
            }
            rawcookies.chop(1);
            
            req.setValue(QLatin1String("Cookie"), rawcookies);
        }
        
        //qDebug() << req.toString();

        m_job_http_id = m_http->request(req, postdata, j->m_file);

        m_last_url = url;
    }


protected slots:
    void responseHeaderReceived(const QHttpResponseHeader &resp)
    {
        TransferJob *j = static_cast<TransferJob *>(job());

        j->m_respcode = resp.statusCode();
        j->m_error_string = resp.reasonPhrase();

        switch (j->m_respcode) {
        case 301:
        case 302:
        case 303:
        case 307: {
            QUrl redirect = resp.value("Location");
            if (!redirect.isEmpty()) {
                j->m_effective_url = j->m_effective_url.resolved(redirect);
                download();
                //not finished yet!
            }
            else {
                j->setStatus(TransferJob::Failed);
            }
            break;
        }
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
    QHttp *m_http;
    int m_job_http_id;
    QString m_user_agent;
    QUrl m_last_url;
    
    static QNetworkCookieJar s_cookies;
    static QMutex            s_cookie_mutex;
};

QNetworkCookieJar TransferEngine::s_cookies;
QMutex TransferEngine::s_cookie_mutex;

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

TransferJob *TransferJob::create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    if (url.isEmpty())
        return 0;

    TransferJob *j = new TransferJob();

    j->m_url = url;
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

Transfer::Transfer(int threadcount)
    : ThreadPool()
{
    if (s_default_user_agent.isEmpty())
        s_default_user_agent = QString("%1/%2").arg(qApp->applicationName()).arg(qApp->applicationVersion());
    m_user_agent = s_default_user_agent;

    init(threadcount);
}

void Transfer::setProxy(const QNetworkProxy &proxy)
{
    m_proxy = proxy;
}

void Transfer::setUserAgent(const QString &ua)
{
    m_user_agent = ua;
}

ThreadPoolEngine *Transfer::createThreadPoolEngine()
{
    return new TransferEngine(this);
}

#include "transfer.moc"
