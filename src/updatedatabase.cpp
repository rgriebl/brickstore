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

#include <QFile>
#include <QDateTime>

#include "updatedatabase.h"
#include "lzmadec.h"
#include "config.h"
#include "bricklink.h"
#include "progressdialog.h"


#define DATABASE_URL   "http://brickforge.de/brickstore-data/"


UpdateDatabase::UpdateDatabase(ProgressDialog *pd)
    : m_progress(pd)
{
    connect(pd, &ProgressDialog::transferFinished,
            this, &UpdateDatabase::gotten);

    pd->setAutoClose(false);
    pd->setHeaderText(tr("Updating BrickLink Database"));
    pd->setMessageText(tr("Download: %p"));

    QString remotefile = DATABASE_URL + BrickLink::core()->defaultDatabaseName();
    QString localfile = BrickLink::core()->dataPath() + BrickLink::core()->defaultDatabaseName();

    QDateTime dt;
    if (QFile::exists(localfile))
        dt = Config::inst()->lastDatabaseUpdate();

    QFile *file = new QFile(localfile + ".lzma");

    if (file->open(QIODevice::WriteOnly)) {
        pd->get(remotefile + ".lzma", dt, file);
    }
    else {
        pd->setErrorText(tr("Could not write to file: %1").arg(file->fileName()));
        delete file;
    }
}

void UpdateDatabase::gotten()
{
    TransferJob *job = m_progress->job();
    auto *file = qobject_cast<QFile *>(job->file());

    if (job->wasNotModifiedSince()) {
        file->remove();
        m_progress->setMessageText(tr("Already up-to-date."));
        m_progress->setFinished(true);
    }
    else if (file && file->size()) {
        QString basepath = file->fileName();
        basepath.truncate(basepath.length() - 5);      // strip '.lzma'

        QString error = decompress(file->fileName(), basepath);

        if (error.isNull()) {
            if (BrickLink::core()->readDatabase()) {
                Config::inst()->setLastDatabaseUpdate(QDateTime::currentDateTime());

                m_progress->setMessageText(tr("Finished."));
                m_progress->setFinished(true);
            }
            else
                m_progress->setErrorText(tr("Could not load the new database."));
        }
        else
            m_progress->setErrorText(error);
    }
    else
        m_progress->setErrorText(tr("Downloaded file is empty."));
}

QString UpdateDatabase::decompress(const QString &src, const QString &dst)
{
    QFile sf(src);
    QFile df(dst);

    if (!sf.open(QIODevice::ReadOnly))
        return tr("Could not read downloaded file: %1").arg(src);
    if (!df.open(QIODevice::WriteOnly))
        return tr("Could not write to database file: %1").arg(dst);

    static const int CHUNKSIZE_IN = 4096;
    static const int CHUNKSIZE_OUT = 512 * 1024;

    char *buffer_in  = new char [CHUNKSIZE_IN];
    char *buffer_out = new char [CHUNKSIZE_OUT];

    lzmadec_stream strm;
    strm.lzma_alloc = nullptr;
    strm.lzma_free = nullptr;
    strm.opaque = nullptr;
    strm.avail_in = 0;
    strm.next_in = nullptr;

    if (lzmadec_init(&strm) != LZMADEC_OK)
        return tr("Could not initialize the LZMA decompressor");


    QString loop_error;

    m_progress->setMessageText(tr("Decompressing database"));
    m_progress->setProgress(0, 0);

    while (true) {
        if (strm.avail_in == 0) {
            strm.next_in  = (unsigned char *) buffer_in;
            strm.avail_in = sf.read(buffer_in, CHUNKSIZE_IN);
        }
        strm.next_out  = (unsigned char *) buffer_out;
        strm.avail_out = CHUNKSIZE_OUT;

        int ret = lzmadec_decode(&strm, strm.avail_in == 0);
        if (ret != LZMADEC_OK && ret != LZMADEC_STREAM_END) {
            loop_error = tr("Error while decompressing %1").arg(src);
            break;
        }

        qint64 write_size = CHUNKSIZE_OUT - strm.avail_out;
        if (write_size != df.write(buffer_out, write_size)) {
            loop_error = tr("Error writing to file %1: %2").arg(dst, df.errorString());
            break;
        }
        if (ret == LZMADEC_STREAM_END) {
            lzmadec_end(&strm);
            break;
        }
        m_progress->setProgress(sf.pos(), sf.size());
    }

    delete [] buffer_in;
    delete [] buffer_out;

    return loop_error;
}

#include "moc_updatedatabase.cpp"
