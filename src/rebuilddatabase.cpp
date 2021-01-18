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
#include <cstdlib>

#include <QFile>
#include <QApplication>
#include <qlogging.h>
#include <QUrlQuery>
//// #include <QtNetworkAuth/QOAuth1>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#endif

#include "bricklink.h"
#include "utility.h"
#include "rebuilddatabase.h"

#if !defined(BRICKSTORE_BACKEND_ONLY)
#  include "config.h"
#endif


RebuildDatabase::RebuildDatabase(bool skipDownload)
    : QObject(nullptr)
    , m_skip_download(skipDownload)
{
    m_trans = nullptr;

#if defined(Q_OS_WINDOWS)
    AllocConsole();
    SetConsoleTitleW(L"BrickStore - Rebuilding Database");
    (void) freopen("CONIN$", "r", stdin);
    (void) freopen("CONOUT$", "w", stdout);
#endif
}

RebuildDatabase::~RebuildDatabase()
{
#if defined(Q_OS_WINDOWS)
    printf("\n\nPress RETURN to quit...\n\n");
    getchar();
#endif
    delete m_trans;
}

int RebuildDatabase::error(const QString &error)
{
    if (error.isEmpty())
        printf(" FAILED.\n");
    else
        printf(" FAILED: %s\n", qPrintable(error));

    return 2;
}


int RebuildDatabase::exec()
{
    m_trans = new Transfer;
    connect(m_trans, &Transfer::finished,
            this, &RebuildDatabase::downloadJobFinished);

    BrickLink::Core *bl = BrickLink::core();
    bl->setOnlineStatus(false);

    BrickLink::TextImport blti;

    printf("\n Rebuilding database ");
    printf("\n=====================\n");

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 1: Logging into BrickLink...\n");

    // login hack
    {
#if defined(BRICKSTORE_BACKEND_ONLY)
        QString username = qgetenv("BRICKSTORE_USERNAME");
        QString password = qgetenv("BRICKSTORE_PASSWORD");
#else
        QString username = Config::inst()->loginForBrickLink().first;
        QString password = Config::inst()->loginForBrickLink().second;
#endif

        if (username.isEmpty() || password.isEmpty())
            printf("  > Missing BrickLink login credentials: please set $BRICKLINK_USERNAME and $BRICKLINK_PASSWORD.\n");

        QNetworkAccessManager *nam = m_trans->networkAccessManager();
        QNetworkRequest req(QUrl("https://www.bricklink.com/ajax/renovate/loginandout.ajax"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QUrlQuery q;
        q.addQueryItem("userid", username);
        q.addQueryItem("password", password);
        q.addQueryItem("keepme_loggedin", "1");
        QByteArray form = q.query(QUrl::FullyEncoded).toLatin1();

        QNetworkReply *reply = nam->post(req, form);
        while (reply && !reply->isFinished())
            qApp->processEvents();

        if ((reply->error() != QNetworkReply::NoError)
                || (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200)) {
            return error(QByteArray("Failed to log into BrickLink:\n") + reply->readAll());
        }
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (!m_skip_download) {
        printf("\nSTEP 2: Downloading (text) database files...\n");

        if (!download())
            return error(m_error);
    }

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 3: Parsing downloaded files...\n");

    if (!blti.import(bl->dataPath()))
        return error("failed to parse database files.");

    blti.exportTo(bl);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 4: Parsing inventories (part I)...\n");

    QVector<const BrickLink::Item *> invs = blti.items();
    blti.importInventories(invs);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 5: Downloading missing/updated inventories...\n");

    if (!downloadInventories(invs))
        return error(m_error);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 6: Parsing inventories (part II)...\n");

    blti.importInventories(invs);

    if ((invs.size() - invs.count(nullptr)) > (blti.items().count() / 50))             // more than 2% have failed
        return error("more than 2% of all inventories had errors.");

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 7: Computing the database...\n");

    blti.exportInventoriesTo(bl);

    extern int _dwords_for_appears, _qwords_for_consists;

    printf("  > appears-in : %11d bytes\n", _dwords_for_appears * 4);
    printf("  > consists-of: %11d bytes\n", _qwords_for_consists * 8);

    /////////////////////////////////////////////////////////////////////////////////
    BrickLink::Core::DatabaseVersion dbVersion = BrickLink::Core::DatabaseVersion::Latest;
    printf("\nSTEP 8: Writing the database to disk...\n");

    while (dbVersion != BrickLink::Core::DatabaseVersion::Invalid) {
        printf("  > version %d... ", int(dbVersion));
        if (bl->writeDatabase(bl->dataPath() + bl->defaultDatabaseName(dbVersion), dbVersion))
            printf("done\n");
        else
            printf("failed\n");

        dbVersion = static_cast<BrickLink::Core::DatabaseVersion>(int(dbVersion) - 1);
    }

    printf("\nFINISHED.\n\n");


    qInstallMessageHandler(nullptr);
    return 0;
}

static QList<QPair<QString, QString> > itemQuery(char item_type)
{
    QList<QPair<QString, QString> > query;   //?a=a&viewType=0&itemType=X
    query << QPair<QString, QString>("a", "a")
            << QPair<QString, QString>("viewType", "0")
            << QPair<QString, QString>("itemType", QChar(item_type))
            << QPair<QString, QString>("selItemColor", "Y")  // special BrickStore flag to get default color - thanks Dan
            << QPair<QString, QString>("selWeight", "Y")
            << QPair<QString, QString>("selYear", "Y")
            << QPair<QString, QString>("downloadType", "X");

    return query;
}

static QList<QPair<QString, QString> > dbQuery(int which)
{
    QList<QPair<QString, QString> > query; //?a=a&viewType=X
    query << QPair<QString, QString>("a", "a")
            << QPair<QString, QString>("viewType", QString::number(which))
            << QPair<QString, QString>("downloadType", "X");

    return query;
}

bool RebuildDatabase::download()
{
    QString path = BrickLink::core()->dataPath();

#if 0
    QString baseUrl = "https://api.bricklink.com/api/store/v1";

    QUrl tu = baseUrl + "/colors";

    // User
    const char *consumerKey = "0C2AEE92FF4244CBAD7F3F2825548C35";
    const char *consumerSecret = "725833030F554AF9B9BCC9117E88ED70";
    // Access Token
    const char *tokenValue = "5AEF00FD7E664A8C86D8A8B61D180FCC";
    const char *tokenSecret = "9033ECE8CDCC48A484F8B3D85224EFCA";

    QNetworkAccessManager *nam = new QNetworkAccessManager();
    QOAuth1 oa(nam, this);
    oa.setSignatureMethod(QOAuth1::SignatureMethod::Hmac_Sha1);
    oa.setClientCredentials(consumerKey, consumerSecret);
    oa.setTokenCredentials(tokenValue, tokenSecret);

    QNetworkReply *reply = oa.get(tu);

    while (reply && !reply->isFinished())
        qApp->processEvents();

    if (reply->error() == QNetworkReply::NoError) {


        QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
        QJsonObject meta = json.object().value("meta").toObject();
        QJsonArray data = json.object().value("data").toArray();

        if (meta.value("code").toInt() == 200) {
            for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
                QJsonObject color = it->toObject();


            }
        }
    }


    exit(2);
#endif



//  {
//      replyHandler = new QOAuthHttpServerReplyHandler(1337, this);
//      replyHandler->setCallbackPath("callback");
//      setReplyHandler(replyHandler);
//      setTemporaryCredentialsUrl(QUrl("https://api.twitter.com/oauth/request_token"));
//      setAuthorizationUrl(QUrl("https://api.twitter.com/oauth/authenticate"));
//      setTokenCredentialsUrl(QUrl("https://api.twitter.com/oauth/access_token"));

//      connect(this, &QAbstractOAuth::authorizeWithBrowser, [=](QUrl url) {
//          QUrlQuery query(url);

//          // Forces the user to enter their credentials to authorize the correct
//          // user account
//          query.addQueryItem("force_login", "true");

//          if (!screenName.isEmpty())
//              query.addQueryItem("screen_name", screenName);
//          url.setQuery(query);
//          QDesktopServices::openUrl(url);
//      });

//      connect(this, &QOAuth1::granted, this, &Twitter::authenticated);

//      if (!clientCredentials.first.isEmpty() && !clientCredentials.second.isEmpty())
//          grant();




    struct {
        const char *m_url;
        const QList<QPair<QString, QString> > m_query;
        const char *m_file;
    } * tptr, table [] = {
        { "https://www.bricklink.com/catalogDownload.asp", dbQuery(1),     "itemtypes.xml"   },
        { "https://www.bricklink.com/catalogDownload.asp", dbQuery(2),     "categories.xml"  },
        { "https://www.bricklink.com/catalogDownload.asp", dbQuery(3),     "colors.xml"      },
        { "https://www.bricklink.com/catalogDownload.asp", dbQuery(5),     "part_color_codes.xml" },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('S'), "items_S.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('P'), "items_P.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('M'), "items_M.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('B'), "items_B.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('G'), "items_G.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('C'), "items_C.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('I'), "items_I.xml"     },
        { "https://www.bricklink.com/catalogDownload.asp", itemQuery('O'), "items_O.xml"     },
        { "https://www.bricklink.com/btinvlist.asp",       { },            "btinvlist.csv"   },
        { "https://www.bricklink.com/btchglog.asp",        { },            "btchglog.csv" },
        { "https://www.ldraw.org/library/official/ldconfig.ldr", { },      "ldconfig.ldr" },
        { nullptr, { }, nullptr }
    };

    bool failed = false;
    m_downloads_in_progress = 0;
    m_downloads_failed = 0;

    { // workaround for U type
        QFile uif(path + "items_U.txt");
        uif.open(QIODevice::WriteOnly);
    }

    for (tptr = table; tptr->m_url; tptr++) {
        QFile *f = new QFile(path + tptr->m_file + ".new");

        if (!f->open(QIODevice::WriteOnly)) {
            m_error = QString("failed to write %1: %2").arg(tptr->m_file, f->errorString());
            delete f;
            failed = true;
            break;
        }
        QUrl url(tptr->m_url);
        QUrlQuery query;
        query.setQueryItems(tptr->m_query);
        url.setQuery(query);

        TransferJob *job = TransferJob::get(url, f);
        m_trans->retrieve(job);
        m_downloads_in_progress++;
    }

    if (failed) {
        m_trans->abortAllJobs();
        return false;
    }

    while (m_downloads_in_progress)
        qApp->processEvents();

    if (m_downloads_failed)
        return false;

    return true;
}

void RebuildDatabase::downloadJobFinished(TransferJob *job)
{
    if (job) {
        auto *f = qobject_cast<QFile *>(job->file());

        m_downloads_in_progress--;
        bool ok = false;

        if (job->isCompleted() && f) {
            f->close();

            QString basepath = f->fileName();
            basepath.truncate(basepath.length() - 4);

            QString err = Utility::safeRename(basepath);

            if (err.isNull())
                ok = true;
            else
                m_error = err;
        }
        else
            m_error = QString("failed to download file: ") + job->errorString();

        if (!ok)
            m_downloads_failed++;

        QString fname = f ? f->fileName() : job->effectiveUrl().toString();
        fname.remove(0, BrickLink::core()->dataPath().length());
        printf("%c > %s", ok ? ' ' : '*', qPrintable(fname));
        if (ok)
            printf("\n");
        else
            printf(" (%s)\n", qPrintable(m_error));
    }
}


bool RebuildDatabase::downloadInventories(QVector<const BrickLink::Item *> &invs)
{
    bool failed = false;
    m_downloads_in_progress = 0;
    m_downloads_failed = 0;

    QUrl url("https://www.bricklink.com/catalogDownload.asp");

    const BrickLink::Item **itemp = invs.data();
    for (int i = 0; i < invs.count(); i++) {
        const BrickLink::Item *&item = itemp [i];

        if (item) {
            QFile *f = BrickLink::core()->dataFile(u"inventory.xml.new", QIODevice::WriteOnly, item);

            if (!f || !f->isOpen()) {
                m_error = QString("failed to write %1: %2").arg(f->fileName(), f->errorString());
                delete f;
                failed = true;
                break;
            }

            QList<QPair<QString, QString> > items {
                { QPair<QString, QString>("a",            "a") },
                { QPair<QString, QString>("viewType",     "4") },
                { QPair<QString, QString>("itemTypeInv",  QChar(item->itemType()->id())) },
                { QPair<QString, QString>("itemNo",       item->id()) },
                { QPair<QString, QString>("downloadType", "X") }
            };
            QUrlQuery query;
            query.setQueryItems(items);
            url.setQuery(query);
            TransferJob *job = TransferJob::get(url, f);
            m_trans->retrieve(job);
            m_downloads_in_progress++;
        }

        // avoid "too many open files" errors
        if (m_downloads_in_progress > 100) {
            while (m_downloads_in_progress > 50)
                qApp->processEvents();
        }
    }

    if (failed) {
        QString err = m_error;

        m_trans->abortAllJobs();

        m_error = err;
        return false;
    }

    while (m_downloads_in_progress)
        qApp->processEvents();

    return true;
}

#include "moc_rebuilddatabase.cpp"
