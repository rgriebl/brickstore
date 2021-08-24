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

#include <QFile>
#include <QDateTime>
#include <QStringBuilder>

#include "bs_lzma.h"
#include "updatedatabase.h"
#include "config.h"
#include "bricklink.h"
#include "utility.h"
#include "version.h"
#include "progressdialog.h"


UpdateDatabase::UpdateDatabase(QWidget *parent)
    : QObject(parent)
{
    BrickLink::core()->cancelTransfers();

    QString remotefile = QLatin1String(BRICKSTORE_DATABASE_URL) % BrickLink::core()->defaultDatabaseName();
    QString localfile = BrickLink::core()->dataPath() % BrickLink::core()->defaultDatabaseName();

    QDateTime dt;
    if (QFile::exists(localfile))
        dt = BrickLink::core()->databaseDate();

    QFile *file = new QFile(localfile % u".lzma");

    TransferJob *job = nullptr;
    if (file->open(QIODevice::WriteOnly)) {
        job = TransferJob::getIfNewer(QUrl(remotefile % u".lzma"), dt, file);
        m_trans.retrieve(job);
    } else {
        delete file;
    }

    m_progress = new ProgressDialog(tr("Update Database"), job, parent);
    connect(m_progress, &ProgressDialog::transferFinished,
            this, &UpdateDatabase::gotten);
    connect(&m_trans, &Transfer::jobProgress, m_progress, &ProgressDialog::transferProgress);
    connect(&m_trans, &Transfer::finished, m_progress, &ProgressDialog::transferDone);

    if (!job)
        m_progress->setErrorText(tr("Could not write to file: %1").arg(file->fileName()));

    m_progress->setHeaderText(tr("Updating the BrickLink database"));
    m_progress->setMessageText(tr("Download: %p"));
}

int UpdateDatabase::exec()
{
    return m_progress->exec();
}

void UpdateDatabase::gotten()
{
    TransferJob *job = m_progress->job();
    auto *file = qobject_cast<QFile *>(job->file());

    if (job->isFailed() || job->responseCode() != 200 || !file)
        return;

    if (job->wasNotModifiedSince()) {
        file->remove();
        m_progress->setMessageText(tr("Already up-to-date."));
        m_progress->setFinished(true);

    } else if (file && file->size()) {
        QString basepath = file->fileName();
        basepath.truncate(basepath.length() - 5);      // strip '.lzma'

        m_progress->setMessageText(tr("Decompressing database"));
        QString error = LZMA::decompress(file->fileName(), basepath, [=](int p, int t) {
            m_progress->setProgress(p, t);
        });

        if (error.isNull()) {
            if (BrickLink::core()->readDatabase()) {
                m_progress->setMessageText(tr("Finished."));
                m_progress->setFinished(true);
            } else {
                m_progress->setErrorText(tr("Could not load the new database."));
            }
        } else  {
            m_progress->setErrorText(error);
        }
    } else {
        m_progress->setErrorText(tr("Downloaded file is empty."));
    }
}


#include "moc_updatedatabase.cpp"
