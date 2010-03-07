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
#include <cstdlib>

#include <QFile>
#include <QApplication>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

#include "bricklink.h"
#include "config.h"
#include "utility.h"

#include "rebuilddatabase.h"


RebuildDatabase::RebuildDatabase()
    : QObject(0)
{
    m_trans = 0;

#if defined( Q_OS_WIN32 )
    AllocConsole();
    SetConsoleTitleW(L"BrickStore - Rebuilding Database");
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
#endif
}

RebuildDatabase::~RebuildDatabase()
{
#if defined( Q_OS_WIN32 )
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


namespace {

static void nirvanaMsgHandler(QtMsgType type, const char * /*x*/)
{
    if (type == QtFatalMsg)
        abort();
}

} // namespace


int RebuildDatabase::exec()
{
    m_trans = new Transfer(5);
    m_trans->setProxy(Config::inst()->proxy());
    connect(m_trans, SIGNAL(finished(ThreadPoolJob *)), this, SLOT(downloadJobFinished(ThreadPoolJob *)));

    BrickLink::Core *bl = BrickLink::core();
    bl->setOnlineStatus(false);

    BrickLink::TextImport blti;

    qInstallMsgHandler(nirvanaMsgHandler);

    printf("\n Rebuilding database ");
    printf("\n=====================\n");

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 1: Downloading (text) database files...\n");

    if (!download())
        return error(m_error);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 2: Parsing downloaded files...\n");

    if (!blti.import(bl->dataPath()))
        return error("failed to parse database files.");

    blti.exportTo(bl);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 3: Parsing inventories (part I)...\n");

    QVector<const BrickLink::Item *> invs = blti.items();
    blti.importInventories(bl->dataPath(), invs);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 4: Downloading missing/updated inventories...\n");

    if (!downloadInventories(invs))
        return error(m_error);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 5: Parsing inventories (part II)...\n");

    blti.importInventories(bl->dataPath(), invs);

    if ((invs.size() - invs.count(0)) > (blti.items().count() / 50))             // more than 2% have failed
        return error("more than 2% of all inventories had errors.");

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 6: Computing the database...\n");

    blti.exportInventoriesTo(bl);

    extern uint _dwords_for_appears, _qwords_for_consists;

    printf("  > appears-in : %11u bytes\n", _dwords_for_appears * 4);
    printf("  > consists-of: %11u bytes\n", _qwords_for_consists * 8);

    /////////////////////////////////////////////////////////////////////////////////
    printf("\nSTEP 7: Writing the new v0 (BS 1.1) database to disk...\n");
    if (!bl->writeDatabase(bl->dataPath() + bl->defaultDatabaseName(BrickLink::Core::BrickStore_1_1), BrickLink::Core::BrickStore_1_1))
        return error("failed to write the v0 (BS 1.1) database file.");

    printf("\nSTEP 8: Writing the new v1 (BS 2.0) database to disk...\n");
    if (!bl->writeDatabase(bl->dataPath() + bl->defaultDatabaseName(BrickLink::Core::BrickStore_2_0), BrickLink::Core::BrickStore_2_0))
        return error("failed to write the v1 (BS 2.0) database file.");

    printf("\nFINISHED.\n\n");


    qInstallMsgHandler(0);
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
    << QPair<QString, QString>("selYear", "Y");

    return query;
}

static QList<QPair<QString, QString> > dbQuery(int which)
{
    QList<QPair<QString, QString> > query; //?a=a&viewType=X
    query << QPair<QString, QString>("a", "a")
          << QPair<QString, QString>("viewType", QString::number(which));

    return query;
}

bool RebuildDatabase::download()
{
    QString path = BrickLink::core()->dataPath();

    struct {
        const char *m_url;
        const QList<QPair<QString, QString> > m_query;
        const char *m_file;
    } * tptr, table [] = {
        { "http://www.bricklink.com/catalogDownload.asp", dbQuery(1),     "itemtypes.txt"   },
        { "http://www.bricklink.com/catalogDownload.asp", dbQuery(2),     "categories.txt"  },
        { "http://www.bricklink.com/catalogDownload.asp", dbQuery(3),     "colors.txt"      },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('S'), "items_S.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('P'), "items_P.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('M'), "items_M.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('B'), "items_B.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('G'), "items_G.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('C'), "items_C.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('I'), "items_I.txt"     },
        { "http://www.bricklink.com/catalogDownload.asp", itemQuery('O'), "items_O.txt"     },
        // { "http://www.bricklink.com/catalogDownload.asp", itemQuery('U'), "items_U.txt"     }, // generates a 500 server error
        { "http://www.bricklink.com/btinvlist.asp",       QList<QPair<QString, QString> >(), "btinvlist.txt"   },
        { "http://www.bricklink.com/btchglog.asp",        QList<QPair<QString, QString> >(), "btchglog.txt" },

        { "http://www.peeron.com/inv/colors",             QList<QPair<QString, QString> >(), "peeron_colors.html" },

        { 0, QList<QPair<QString, QString> > (), 0 }
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
            m_error = QString("failed to write %1: %2").arg(tptr->m_file).arg(f->errorString());
            delete f;
            failed = true;
            break;
        }
        QUrl url(tptr->m_url);
        url.setQueryItems(tptr->m_query);

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

void RebuildDatabase::downloadJobFinished(ThreadPoolJob *pj)
{
    if (pj) {
        TransferJob *job = static_cast<TransferJob *>(pj);

        QFile *f = qobject_cast<QFile *>(job->file());

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

        QString fname = f->fileName();
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
    QString path = BrickLink::core()->dataPath();

    bool failed = false;
    m_downloads_in_progress = 0;
    m_downloads_failed = 0;

    QUrl url("http://www.bricklink.com/catalogDownload.asp");

    const BrickLink::Item **itemp = invs.data();
    for (int i = 0; i < invs.count(); i++) {
        const BrickLink::Item *&item = itemp [i];

        if (item) {
            QFile *f = new QFile(BrickLink::core()->dataPath(item) + "inventory.xml.new");

            if (!f->open(QIODevice::WriteOnly)) {
                m_error = QString("failed to write %1: %2").arg(f->fileName()).arg(f->errorString());
                delete f;
                failed = true;
                break;
            }

            QList<QPair<QString, QString> > query;
            query << QPair<QString, QString>("a",            "a")
            << QPair<QString, QString>("viewType",     "4")
            << QPair<QString, QString>("itemTypeInv",  QChar(item->itemType()->id()))
            << QPair<QString, QString>("itemNo",       item->id())
            << QPair<QString, QString>("downloadType", "X");

            url.setQueryItems(query);
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
