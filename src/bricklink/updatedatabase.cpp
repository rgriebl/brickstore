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

#include <QSaveFile>
#include <QFile>
#include <QDateTime>
#include <QStringBuilder>
#include <QFileInfo>

#include "bricklink/core.h"
#include "utility/utility.h"
#include "lzma/bs_lzma.h"
#include "version.h"
#include "updatedatabase.h"


UpdateDatabase::UpdateDatabase(QObject *parent)
    : QObject(parent)
{ }

bool UpdateDatabase::start()
{
    return start(true);
}

bool UpdateDatabase::start(bool force)
{
    BrickLink::core()->cancelTransfers();

    QString dbName = BrickLink::core()->defaultDatabaseName();
    QString remotefile = BRICKSTORE_DATABASE_URL ""_l1 % dbName % u".lzma";
    QString localfile = BrickLink::core()->dataPath() % dbName;

    QDateTime dt;
    if (!force && QFile::exists(localfile))
        dt = BrickLink::core()->databaseDate().addSecs(60 * 5);

    auto file = new QSaveFile(localfile);
    auto lzma = new LZMA::DecompressFilter(file);
    auto hhc = new HashHeaderCheckFilter(lzma);
    lzma->setParent(hhc);
    file->setParent(lzma);

    TransferJob *job = nullptr;
    if (hhc->open(QIODevice::WriteOnly)) {
        job = TransferJob::getIfNewer(QUrl(remotefile), dt, hhc);
        m_trans.retrieve(job);
    }
    if (!job) {
        delete hhc;
        return false;
    }

    connect(&m_trans, &Transfer::started,
            [this, job](TransferJob *j) {
        if (j != job)
            return;
        emit started();
    });
    connect(&m_trans, &Transfer::progress,
            [this, job](TransferJob *j, int done, int total) {
        if (j != job)
            return;
        emit progress(done, total);
    });
    connect(&m_trans, &Transfer::finished,
            [this, job, hhc, file](TransferJob *j) {
        if (j != job)
            return;

        hhc->close(); // does not close/commit the QSaveFile
        hhc->deleteLater();

        if (job->isFailed()) {
            emit finished(false, tr("Failed to download and decompress the database") % u": "
                          % job->errorString());
        } else if (job->wasNotModifiedSince()) {
            emit finished(true, tr("Already up-to-date."));
        } else if (!hhc->hasValidChecksum()) {
            emit finished(false, tr("Checksum mismatch after decompression"));
        } else if (!file->commit()) {
            emit finished(false, tr("Could not save the database") % u": "
                          % file->errorString());
        } else if (!BrickLink::core()->readDatabase(file->fileName())) {
            emit finished(false, tr("Could not load the new database."));
        } else {
            emit finished(true, { });
        }
    });
    return true;
}

void UpdateDatabase::cancel()
{
    m_trans.abortAllJobs();
}

#include "moc_updatedatabase.cpp"
