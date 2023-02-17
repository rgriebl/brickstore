// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QIODevice>

#include "chunkreader.h"
#include "chunkwriter.h"

#if 0

on-disk format for every chunk (bits / field):
 * 32 ID
 * 32 VERSION
 * 64 SIZE

 * SIZE BYTES DATA

 * PAD WITH 0 TO SIZE %16

 * 64 SIZE
 * 32 VERSION
 * 32 ID

 ----------------------------------------------

example:

    QFile f(...);
    if (f.open(QIODevice::WriteOnly)) {
        ChunkWriter cw(&f);
        QDataStream &ds;

        ds = cw.startChunk('BSDB'L, 1);
        ds << u"INFO TEXT"_qs;
        ds = cw.startChunk('COLO', 1);
        cw.endChunk();
        cw.endChunk();
        f.close();
    }
#endif

ChunkReader::ChunkReader(QIODevice *dev, QDataStream::ByteOrder bo)
    : m_file(nullptr)
    , m_stream(nullptr)
{
    if (dev->isSequential()) {
        qWarning("ChunkReader: device does not support direct access!");
        return;
    }

    if (!dev->isOpen() || !dev->isReadable()) {
        qWarning("ChunkReader: device is not open and readable!");
        return;
    }
    m_file = dev;
    m_stream.setVersion(QDataStream::Qt_5_11);
    m_stream.setDevice(dev);
    m_stream.setByteOrder(bo);
}

QDataStream &ChunkReader::dataStream()
{
    static QDataStream nullstream;

    if (!m_file)
        return nullstream;
    return m_stream;
}

bool ChunkReader::startChunk()
{
    if (!m_file || (m_stream.status() != QDataStream::Ok))
        return false;

    if (!m_chunks.isEmpty()) {
        const read_chunk_info &parent = std::as_const(m_chunks).top();

        if (m_file->pos() >= (parent.startpos + parent.size))
            return false;
    }

    read_chunk_info ci;

    m_stream >> ci.id >> ci.version >> ci.size;
    if (m_stream.status() == QDataStream::Ok) {
        ci.startpos = m_file->pos();
        m_chunks.push(ci);
        return true;
    }
    return false;
}

bool ChunkReader::skipChunk()
{
    if (!m_file || m_chunks.isEmpty() || (m_stream.status() != QDataStream::Ok))
        return false;

    const read_chunk_info &ci = std::as_const(m_chunks).top();

    m_stream.skipRawData(int(ci.size));
    return (m_stream.status() == QDataStream::Ok);
}

bool ChunkReader::endChunk()
{
    if (!m_file || m_chunks.isEmpty())
        return false;

    read_chunk_info ci = m_chunks.pop();

    qint64 endpos = m_file->pos();
    if (ci.startpos + ci.size != endpos) {
        qWarning("ChunkReader: called endChunk() on a position %llu bytes from the chunk end.", ci.startpos + ci.size - endpos);
        m_file->seek(ci.startpos + ci.size);
    }

    if (ci.size % 16)
        m_stream.skipRawData(16 - ci.size % 16);

    read_chunk_info ciend;
    m_stream >> ciend.size >> ciend.version >> ciend.id;
    return (m_stream.status() == QDataStream::Ok) &&
           (ci.id == ciend.id) &&
           (ci.version == ciend.version) &&
           (ci.size == ciend.size);
}

quint32 ChunkReader::chunkId() const
{
    if (m_chunks.isEmpty())
        return 0;
    return m_chunks.top().id;
}

quint32 ChunkReader::chunkVersion() const
{
    if (m_chunks.isEmpty())
        return 0;
    return m_chunks.top().version;
}

qint64 ChunkReader::chunkSize() const
{
    if (m_chunks.isEmpty())
        return 0;
    return m_chunks.top().size;
}


///////////////////////////////////////////////////////////////////////


ChunkWriter::ChunkWriter(QIODevice *dev, QDataStream::ByteOrder bo)
    : m_file(nullptr), m_stream(nullptr)
{
    if (dev->isSequential()) {
        qWarning("ChunkWriter: device does not support direct access!");
        return;
    }

    if (!dev->isOpen() || !dev->isWritable()) {
        qWarning("ChunkWriter: device is not yet open!");
        return;
    }
    m_file = dev;
    m_stream.setVersion(QDataStream::Qt_5_11);
    m_stream.setDevice(dev);
    m_stream.setByteOrder(bo);
}

ChunkWriter::~ChunkWriter()
{
    while (!m_chunks.isEmpty()) {
        if (m_file && m_file->isOpen())
            endChunk();
        else
            qWarning("ChunkWriter: file was closed before ending chunk.");
    }
}

QDataStream &ChunkWriter::dataStream()
{
    static QDataStream nullstream;

    if (!m_file)
        return nullstream;
    return m_stream;
}

bool ChunkWriter::startChunk(quint32 id, quint32 version)
{
    m_stream << id << version << quint64(0);

    write_chunk_info ci;
    ci.id = id;
    ci.version = version;
    ci.startpos = m_file->pos();
    m_chunks.push(ci);

    return (m_stream.status() == QDataStream::Ok);
}

bool ChunkWriter::endChunk()
{
    if (!m_file || m_chunks.isEmpty() || (m_stream.status() != QDataStream::Ok))
        return false;

    write_chunk_info ci = m_chunks.pop();
    qint64 endpos = m_file->pos();
    qint64 len = endpos - ci.startpos;
    m_file->seek(ci.startpos - int(sizeof(qint64)));
    m_stream << len;
    m_file->seek(endpos);

    static const char padbytes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (len % 16)
        m_stream.writeRawData(padbytes, 16 - len % 16);

    m_stream << len << ci.version << ci.id;

    return (m_stream.status() == QDataStream::Ok);
}
