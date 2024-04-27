// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <array>
#include <QIODevice>
#include <QFile>

#include "chunkreader.h"
#include "chunkwriter.h"
#include "exception.h"

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

class ChunkException : public Exception
{
public:
    ChunkException(QDataStream &ds, const char *msg)
        : Exception(u"Chunk error%1%2: %3"_qs.arg(fileName(ds.device())).arg(filePos(ds.device())).arg(QString::fromLatin1(msg)))
    { }

private:
    static QString fileName(QIODevice *dev)
    {
        if (auto file = qobject_cast<QFile *>(dev))
            return u" in file " + file->fileName();
        return { };
    }

    static QString filePos(QIODevice *dev)
    {
        if (dev)
            return u" at position " + QString::number(dev->pos());
        return { };
    }
};

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
    return m_stream;
}

void ChunkReader::checkStream()
{
    if (!m_file)
        throw ChunkException(dataStream(), "cannot end a chunk without a file");
    if (m_stream.status() != QDataStream::Ok)
        throw ChunkException(dataStream(), "data stream is not readable: %1").arg(m_stream.status());
}

void ChunkReader::checkChunkStarted()
{
    if (m_chunks.isEmpty())
        throw ChunkException(dataStream(), "cannot end or skip a chunk that was never started");
}

bool ChunkReader::startChunk()
{
    checkStream();

    if (!m_chunks.isEmpty()) {
        const read_chunk_info &parent = std::as_const(m_chunks).top();

        auto filePos = m_file->pos();
        if (filePos == (parent.startpos + parent.size))
            return false;
        else if (filePos > (parent.startpos + parent.size))
            throw ChunkException(dataStream(), "cannot read a nested chunk outside the parent chunk");
    }

    read_chunk_info ci;
    m_stream >> ci.id >> ci.version >> ci.size;

    checkStream();
    ci.startpos = m_file->pos();
    m_chunks.push(ci);
    return true;
}

void ChunkReader::skipChunk()
{
    checkStream();
    checkChunkStarted();

    const read_chunk_info &ci = std::as_const(m_chunks).top();

    m_stream.skipRawData(int(ci.size));

    checkStream();
}

void ChunkReader::endChunk()
{
    checkStream();
    checkChunkStarted();

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

    checkStream();
    if ((ci.id != ciend.id) || (ci.version != ciend.version) || (ci.size != ciend.size))
        throw ChunkException(dataStream(), "end of chunk header does not match start of chunk header");
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

quint64 ChunkReader::chunkIdAndVersion() const
{
    return chunkId() | (quint64(chunkVersion()) << 32);
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

void ChunkWriter::checkStream()
{
    if (!m_file)
        throw ChunkException(dataStream(), "cannot end a chunk without a file");
    if (m_stream.status() != QDataStream::Ok)
        throw ChunkException(dataStream(), "data stream is not writable: %1").arg(m_stream.status());
}

void ChunkWriter::startChunk(quint32 id, quint32 version)
{
    checkStream();
    m_stream << id << version << quint64(0);
    checkStream();

    write_chunk_info ci;
    ci.id = id;
    ci.version = version;
    ci.startpos = m_file->pos();
    m_chunks.push(ci);
}

void ChunkWriter::endChunk()
{
    checkStream();
    if (m_chunks.isEmpty())
        throw ChunkException(dataStream(), "cannot end a chunk that was never started");

    write_chunk_info ci = m_chunks.pop();
    qint64 endpos = m_file->pos();
    qint64 len = endpos - ci.startpos;
    m_file->seek(ci.startpos - int(sizeof(qint64)));
    m_stream << len;
    m_file->seek(endpos);

    static const std::array<char, 16> padbytes = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (len % 16)
        m_stream.writeRawData(padbytes.data(), 16 - len % 16);

    m_stream << len << ci.version << ci.id;
    checkStream();
}
