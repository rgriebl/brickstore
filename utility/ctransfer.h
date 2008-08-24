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
#ifndef __CTRANSFER_H__
#define __CTRANSFER_H__

#include <QDateTime>
#include <QUrl>
#include <QNetworkProxy>

#include "cthreadpool.h"


class QIODevice;
class CTransfer;
class CTransferThread;

class CTransferJob : public CThreadPoolJob {
public:
    ~CTransferJob();

    CTransfer *transfer() const      { return qobject_cast<CTransfer *>(threadPool()); }

    static CTransferJob *get(const QUrl &url, QIODevice *file = 0);
    static CTransferJob *getIfNewer(const QUrl &url, const QDateTime &dt, QIODevice *file = 0);
    static CTransferJob *post(const QUrl &url, QIODevice *file = 0);

    QUrl url() const                 { return m_url; }
    QUrl effectiveUrl() const        { return m_effective_url; }
    QString errorString() const      { return isFailed() ? m_error_string : QString::null; }
    int responseCode() const         { return m_respcode; }
    QIODevice *file() const          { return m_file; }
    QByteArray *data() const         { return m_data; }
    QDateTime lastModified() const   { return m_last_modified; }
    bool wasNotModifiedSince() const { return m_was_not_modified; }

private:
    friend class CTransfer;
    friend class CTransferThread;

    enum HttpMethod {
        HttpGet = 0,
        HttpPost = 1
    };

    static CTransferJob *create(HttpMethod method, const QUrl &url, const QDateTime &ifnewer, QIODevice *file);
    CTransferJob();

private:
    QUrl         m_url;
    QUrl         m_effective_url;
    QByteArray * m_data;
    QIODevice *  m_file;
    QString      m_error_string;
    void *       m_user_data;
    QDateTime    m_only_if_newer;
    QDateTime    m_last_modified;

    int          m_respcode         : 16;
    HttpMethod   m_http_method      : 1;
    bool         m_was_not_modified : 1;

    friend class CTransferEngine;
};


class CTransfer : public CThreadPool {
    Q_OBJECT
public:
    CTransfer(int threadcount);

    inline bool retrieve(CTransferJob *job, bool high_priority = false)
    { return execute(job, high_priority); }

signals:
    void finished(CTransferJob *);
    void started(CTransferJob *);
    void jobProgress(CTransferJob *, int done, int total);

protected:
    virtual CThreadPoolEngine *createThreadPoolEngine();

public slots:
    void setProxy(const QNetworkProxy &proxy);

private:
    QNetworkProxy         m_proxy;
};

#endif
