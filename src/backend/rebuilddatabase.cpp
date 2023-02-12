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
#include <cstdlib>

#include <QFile>
#include <QSaveFile>
#include <QCoreApplication>
#include <qlogging.h>
#include <QUrlQuery>
//// #include <QtNetworkAuth/QOAuth1>
//// #include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#endif

#include "utility/exception.h"
#include "utility/utility.h"
#include "bricklink/core.h"
#include "bricklink/textimport.h"
#include "rebuilddatabase.h"


RebuildDatabase::RebuildDatabase(bool skipDownload, QObject *parent)
    : QObject(parent)
    , m_skip_download(skipDownload)
{
    m_trans = nullptr;

#if defined(Q_OS_WINDOWS)
    AllocConsole();
    SetConsoleTitleW(L"BrickStore - Rebuilding Database");
    FILE *dummy;
    freopen_s(&dummy, "CONIN$", "r", stdin);
    freopen_s(&dummy, "CONOUT$", "w", stdout);
#endif

    // disable buffering on stdout
    setvbuf(stdout, nullptr, _IONBF, 0);
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

    BrickLink::Core *bl = BrickLink::core();

    BrickLink::TextImport blti;

    printf("\n Rebuilding database ");
    printf("\n=====================\n");

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 1: Logging into BrickLink...\n");

    // login hack
    {
        QString username = QString::fromLocal8Bit(qgetenv("BRICKLINK_USERNAME"));
        QString password = QString::fromLocal8Bit(qgetenv("BRICKLINK_PASSWORD"));

        if (username.isEmpty() || password.isEmpty())
            printf("  > Missing BrickLink login credentials: please set $BRICKLINK_USERNAME and $BRICKLINK_PASSWORD.\n");

        m_rebrickableApiKey = QString::fromLocal8Bit(qgetenv("REBRICKABLE_APIKEY"));

        if (m_rebrickableApiKey.isEmpty())
            printf("  > Missing Rebrickable API key: please set $REBRICKABLE_APIKEY.\n");
        QUrl url(u"https://www.bricklink.com/ajax/renovate/loginandout.ajax"_qs);
        QUrlQuery q;
        q.addQueryItem(u"userid"_qs, Utility::urlQueryEscape(username));
        q.addQueryItem(u"password"_qs, Utility::urlQueryEscape(password));
        q.addQueryItem(u"keepme_loggedin"_qs, u"1"_qs);
        url.setQuery(q);

        auto job = TransferJob::post(url);
        bool loggedIn = false;
        QByteArray httpReply;
        auto loginConn = connect(m_trans, &Transfer::finished, this, [job, &loggedIn, &httpReply](TransferJob *j) {
            if (job == j) {
                loggedIn = true;
                if (j->isFailed() || (j->responseCode() != 200))
                    httpReply = *j->data();
            }
        });

        m_trans->retrieve(job, true);

        while (!loggedIn)
            QCoreApplication::processEvents();

        disconnect(loginConn);
        connect(m_trans, &Transfer::finished,
                this, &RebuildDatabase::downloadJobFinished);

        if (!httpReply.isEmpty())
            return error(u"Failed to log into BrickLink:\n"_qs + QLatin1String(httpReply));
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
        return error(u"failed to parse database files."_qs);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 4: Parsing inventories (part I)...\n");

    std::vector<bool> processedInvs(blti.items().size(), false);
    blti.importInventories(processedInvs, BrickLink::TextImport::ImportFromDiskCache);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 5: Downloading missing/updated inventories...\n");

    if (!downloadInventories(blti.items(), processedInvs))
        return error(m_error);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 6: Parsing inventories (part II)...\n");

    blti.importInventories(processedInvs, BrickLink::TextImport::ImportAfterDownload);

    if (std::count(processedInvs.cbegin(), processedInvs.cend(), false)
            > (int(processedInvs.size()) / 50)) {            // more than 2% have failed
        return error(u"more than 2% of all inventories had errors."_qs);
    }

    blti.calculateItemTypeCategories();
    blti.calculatePartsYearUsed();
    blti.calculateCategoryRecency();

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 7: Computing the database...\n");

    blti.exportTo(bl->database());

    extern int _dwords_for_appears, _qwords_for_consists;

    printf("  > appears-in : %11d bytes\n", _dwords_for_appears * 4);
    printf("  > consists-of: %11d bytes\n", _qwords_for_consists * 8);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 8: Writing the database to disk...\n");

    int dbVersionLowest = int(BrickLink::Database::Version::OldestStillSupported);
    int dbVersionHighest = int(BrickLink::Database::Version::Latest);

    Q_ASSERT(dbVersionHighest >= dbVersionLowest);

    for (int v = dbVersionHighest; v >= dbVersionLowest; --v) {
        printf("  > version %d... ", v);
        auto dbVersion = static_cast<BrickLink::Database::Version>(v);
        try {
            bl->database()->write(bl->dataPath() + BrickLink::Database::defaultDatabaseName(dbVersion), dbVersion);
            printf("done\n");
        } catch (const Exception &e) {
            printf("failed: %s\n", e.what());
        }
    }

    printf("\nFINISHED.\n\n");


    qInstallMessageHandler(nullptr);
    return 0;
}

static QUrlQuery partCategoriesQuery(char item_type)
{
    return {
        { u"a"_qs,            u"a"_qs },
        { u"viewType"_qs,     u"0"_qs },
        { u"itemType"_qs,     QString(QLatin1Char(item_type)) },
        { u"downloadType"_qs, u"T"_qs },
    };
}

static QUrlQuery itemQuery(char item_type)
{
    return { //?a=a&viewType=0&itemType=X
        { u"a"_qs,            u"a"_qs },
        { u"viewType"_qs,     u"0"_qs },
        { u"itemType"_qs,     QString(QLatin1Char(item_type)) },
        { u"selItemColor"_qs, u"Y"_qs },  // special BrickStore flag to get default color - thanks Dan
        { u"selWeight"_qs,    u"Y"_qs },
        { u"selYear"_qs,      u"Y"_qs },
        { u"downloadType"_qs, u"X"_qs },
    };
}

static QUrlQuery dbQuery(int which)
{
    return { //?a=a&viewType=X
        { u"a"_qs,            u"a"_qs },
        { u"viewType"_qs,     QString::number(which) },
        { u"downloadType"_qs, u"X"_qs },
    };
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
        QCoreApplication::processEvents();

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


    auto rebrickableQuery = [this]() -> QUrlQuery {
        return {
            { u"page_size"_qs,    u"1000"_qs },
            { u"key"_qs,          m_rebrickableApiKey },
        };
    };


    struct {
        const char *m_url;
        const QUrlQuery m_query;
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
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('S'), "items_S.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('P'), "items_P.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('M'), "items_M.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('B'), "items_B.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('G'), "items_G.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('C'), "items_C.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('I'), "items_I.csv" },
        { "https://www.bricklink.com/catalogDownload.asp", partCategoriesQuery('O'), "items_O.csv" },
        { "https://www.bricklink.com/btinvlist.asp",       { },                "btinvlist.csv"   },
        { "https://www.bricklink.com/btchglog.asp",        { },                "btchglog.csv" },
        { "https://www.ldraw.org/library/official/LDConfig.ldr", { },          "ldconfig.ldr" },
        { "https://rebrickable.com/api/v3/lego/colors/",   rebrickableQuery(), "rebrickable_colors.json" },

        { nullptr, { }, nullptr }
    };

    bool failed = false;
    m_downloads_in_progress = 0;
    m_downloads_failed = 0;

    { // workaround for U type
        QFile uif(path + u"items_U.txt"_qs);
        uif.open(QIODevice::WriteOnly);
    }

    for (tptr = table; tptr->m_url; tptr++) {
        auto *f = new QSaveFile(path + QLatin1String(tptr->m_file));

        if (!f->open(QIODevice::WriteOnly)) {
            m_error = QString::fromLatin1("failed to write %1: %2")
                    .arg(QLatin1String(tptr->m_file), f->errorString());
            delete f;
            failed = true;
            break;
        }
        QUrl url(QLatin1String(tptr->m_url));
        url.setQuery(tptr->m_query);

        TransferJob *job = TransferJob::get(url, f, 2);
        m_trans->retrieve(job);
        m_downloads_in_progress++;
    }

    if (failed) {
        m_trans->abortAllJobs();
        return false;
    }

    while (m_downloads_in_progress)
        QCoreApplication::processEvents();

    if (m_downloads_failed)
        return false;

    return true;
}

void RebuildDatabase::downloadJobFinished(TransferJob *job)
{
    if (job) {
        auto *f = qobject_cast<QSaveFile *>(job->file());

        m_downloads_in_progress--;
        bool ok = false;

        if (job->isCompleted() && f) {
            if (f->commit())
                ok = true;
            else
                m_error = f->errorString();
        }
        else
            m_error = u"Failed to download file: "_qs + job->errorString();

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


bool RebuildDatabase::downloadInventories(const std::vector<BrickLink::Item> &invs,
                                          const std::vector<bool> &processedInvs)
{
    bool failed = false;
    m_downloads_in_progress = 0;
    m_downloads_failed = 0;

    QUrl url(u"https://www.bricklink.com/catalogDownload.asp"_qs);

    for (uint i = 0; i < invs.size(); ++i) {
        const BrickLink::Item *item = &invs[i];
        if (!processedInvs[i]) {
            QSaveFile *f = BrickLink::core()->dataSaveFile(u"inventory.xml", item);

            if (!f || !f->isOpen()) {
                if (f)
                    m_error = u"failed to write "_qs + f->fileName() + u": " + f->errorString();
                else
                    m_error = u"could not get a file handle to write inventory for "_qs + QLatin1String(item->id());
                delete f;
                failed = true;
                break;
            }

            QList<QPair<QString, QString> > items {
                { QPair<QString, QString>(u"a"_qs,            u"a"_qs) },
                { QPair<QString, QString>(u"viewType"_qs,     u"4"_qs) },
                { QPair<QString, QString>(u"itemTypeInv"_qs,  QString(QLatin1Char(item->itemTypeId()))) },
                { QPair<QString, QString>(u"itemNo"_qs,       QLatin1String(item->id())) },
                { QPair<QString, QString>(u"downloadType"_qs, u"X"_qs) }
            };
            QUrlQuery query;
            query.setQueryItems(items);
            url.setQuery(query);
            TransferJob *job = TransferJob::get(url, f, 2);
            m_trans->retrieve(job);
            m_downloads_in_progress++;
        }

        // avoid "too many open files" errors
        if (m_downloads_in_progress > 100) {
            while (m_downloads_in_progress > 50)
                QCoreApplication::processEvents();
        }
    }

    if (failed) {
        QString err = m_error;

        m_trans->abortAllJobs();

        m_error = err;
        return false;
    }

    while (m_downloads_in_progress)
        QCoreApplication::processEvents();

    return true;
}

#include "moc_rebuilddatabase.cpp"
