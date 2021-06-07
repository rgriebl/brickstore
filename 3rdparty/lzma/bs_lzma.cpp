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
#include <QByteArray>
#include <QFile>

#include "lzmadec.h"
#include "bs_lzma.h"


QString LZMA::decompress(const QString &src, const QString &dst,
                         std::function<void (int, int)> progress,
                         QCryptographicHash::Algorithm alg)
{
    QFile sf(src);
    QFile df(dst);

    if (!sf.open(QIODevice::ReadOnly))
        return tr("Could not read downloaded file: %1").arg(src);
    if (!df.open(QIODevice::WriteOnly))
        return tr("Could not write to database file: %1").arg(dst);

    static const int CHUNKSIZE_IN = 4096;
    static const int CHUNKSIZE_OUT = 512 * 1024;

    lzmadec_stream strm;

    strm.lzma_alloc = nullptr;
    strm.lzma_free = nullptr;
    strm.opaque = nullptr;
    strm.avail_in = 0;
    strm.next_in = nullptr;

    if (lzmadec_init(&strm) != LZMADEC_OK)
        return tr("Could not initialize the LZMA decompressor");

    char *buffer_in  = new char [CHUNKSIZE_IN];
    char *buffer_out = new char [CHUNKSIZE_OUT];

    QString loop_error;

    QCryptographicHash sha(alg);
    QByteArray shaRead = sf.read(QCryptographicHash::hashLength(alg));

    if (progress)
        progress(0, 0);

    while (true) {
        if (strm.avail_in == 0) {
            strm.next_in  = reinterpret_cast<unsigned char *>(buffer_in);
            strm.avail_in = static_cast<size_t>(sf.read(buffer_in, CHUNKSIZE_IN));
        }
        strm.next_out  = reinterpret_cast<unsigned char *>(buffer_out);
        strm.avail_out = CHUNKSIZE_OUT;

        int ret = lzmadec_decode(&strm, strm.avail_in == 0);
        if (ret != LZMADEC_OK && ret != LZMADEC_STREAM_END) {
            loop_error = tr("Error while decompressing %1").arg(src);
            break;
        }

        qint64 write_size = qint64(CHUNKSIZE_OUT - strm.avail_out);
        sha.addData(buffer_out, write_size);
        if (write_size != df.write(buffer_out, write_size)) {
            loop_error = tr("Error writing to file %1: %2").arg(dst, df.errorString());
            break;
        }
        if (ret == LZMADEC_STREAM_END) {
            lzmadec_end(&strm);

            QByteArray shaCalculated = sha.result();
            if (shaCalculated != shaRead)
                loop_error = tr("Checksum mismatch after decompression");

            break;
        }
        if (progress)
            progress(int(sf.pos()), int(sf.size()));
    }

    delete [] buffer_in;
    delete [] buffer_out;

    if (!loop_error.isEmpty()) {
        df.close();
        df.remove();
    }

    return loop_error;
}
