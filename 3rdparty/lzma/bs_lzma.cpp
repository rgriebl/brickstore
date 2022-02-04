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
#include <QByteArray>
#include <QFile>
#include <QSaveFile>
#include <QDebug>

#include "lzmadec.h"
#include "bs_lzma.h"

namespace LZMA {

class DecompressFilterPrivate
{
public:
    QIODevice *m_target;
    bool m_init = false;
    QByteArray m_buffer;
    lzmadec_stream m_strm;
};

}

LZMA::DecompressFilter::DecompressFilter(QIODevice *target, QObject *parent)
    : QIODevice(parent)
    , d(new DecompressFilterPrivate)
{
    d->m_target = target;
}

LZMA::DecompressFilter::~DecompressFilter()
{
    delete d;
}

bool LZMA::DecompressFilter::open(OpenMode mode)
{
    if (mode & ReadOnly)
        return false;

    if (d->m_init)
        return false;

    d->m_strm.lzma_alloc = nullptr;
    d->m_strm.lzma_free = nullptr;
    d->m_strm.opaque = nullptr;
    d->m_strm.avail_in = 0;
    d->m_strm.next_in = nullptr;

    d->m_init = (lzmadec_init(&d->m_strm) == LZMADEC_OK);
    if (!d->m_init)
        return false;

    bool targetOk = d->m_target->isOpen() ? (d->m_target->openMode() == mode)
                                          : d->m_target->open(mode);
    if (targetOk) {
        setOpenMode(mode);
        d->m_buffer.resize(512 * 1024);
    } else {
        lzmadec_end(&d->m_strm);
        d->m_init = false;
    }
    return targetOk;
}

void LZMA::DecompressFilter::close()
{
    decompress(nullptr, 0);
    if (!qobject_cast<QSaveFile *>(d->m_target))
        d->m_target->close();
    setOpenMode(NotOpen);
    if (d->m_init)
        lzmadec_end(&d->m_strm);
    d->m_init = false;
    d->m_buffer.clear();
    d->m_buffer.squeeze();
}

bool LZMA::DecompressFilter::isSequential() const
{
    return true;
}

qint64 LZMA::DecompressFilter::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    Q_ASSERT(false);
    setErrorString(QLatin1String("Reading not supported"));
    return -1;
}

qint64 LZMA::DecompressFilter::writeData(const char *data, qint64 maxSize)
{
    return decompress(data, maxSize);
}

qint64 LZMA::DecompressFilter::decompress(const char *data, qint64 maxSize)
{
    d->m_strm.next_in  = reinterpret_cast<unsigned char *>(const_cast<char *>(data));
    d->m_strm.avail_in = static_cast<size_t>(maxSize);

    while (d->m_strm.avail_in || !data) {
        d->m_strm.next_out  = reinterpret_cast<unsigned char *>(d->m_buffer.data());
        d->m_strm.avail_out = d->m_buffer.size();

        int ret = lzmadec_decode(&d->m_strm, d->m_strm.avail_in == 0);
        if (ret != LZMADEC_OK && ret != LZMADEC_STREAM_END) {
            setErrorString(tr("Error while decompressing stream"));
            return -1;
        }
        qint64 write_size = qint64(d->m_buffer.size() - d->m_strm.avail_out);
        if (write_size != d->m_target->write(d->m_buffer.constData(), write_size)) {
            setErrorString(d->m_target->errorString());
            return -1;
        }
        if (ret == LZMADEC_STREAM_END) {
            break;
        }
    }
    return maxSize;
}









HashHeaderCheckFilter::HashHeaderCheckFilter(QIODevice *target, QCryptographicHash::Algorithm alg, QObject *parent)
    : QIODevice(parent)
    , m_target(target)
    , m_hash(alg)
    , m_hashSize(QCryptographicHash::hashLength(alg))
{ }

bool HashHeaderCheckFilter::hasValidChecksum() const
{
    return m_ok;
}

bool HashHeaderCheckFilter::open(OpenMode mode)
{
    if (mode & ReadOnly)
        return false;

    bool targetOk = m_target->isOpen() ? (m_target->openMode() == mode)
                                       : m_target->open(mode);
    if (targetOk) {
        setOpenMode(mode);
        m_gotHeader = false;
        m_ok = false;
        m_hash.reset();
    }
    return targetOk;
}

void HashHeaderCheckFilter::close()
{
    QByteArray calculated = m_hash.result();
    m_ok = (calculated != m_header);

    if (!qobject_cast<QSaveFile *>(m_target))
        m_target->close();
    setOpenMode(NotOpen);
    m_gotHeader = false;
    m_hash.reset();
}

bool HashHeaderCheckFilter::isSequential() const
{
    return true;
}

qint64 HashHeaderCheckFilter::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    Q_ASSERT(false);
    setErrorString(QLatin1String("Reading not supported"));
    return -1;
}

qint64 HashHeaderCheckFilter::writeData(const char *data, qint64 maxSize)
{
    if (maxSize <= 0)
        return maxSize;

    int dataSize = maxSize;

    if (!m_gotHeader) {
        qint64 need = m_hashSize - m_header.size();
        qint64 got = qMin(need, maxSize);
        m_header.append(data, got);
        dataSize -= got;
        data += got;

        m_gotHeader = (need == got);
    }
    if (dataSize) {
        m_hash.addData(data, dataSize);
        qint64 written = m_target->write(data, dataSize);
        if ((written < 0) || (written != dataSize)) {
            setErrorString(m_target->errorString());
            return -1;
        }
    }
    return maxSize;
}


#include "moc_bs_lzma.cpp"
