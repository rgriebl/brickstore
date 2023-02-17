// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHUNKREADER_H
#define CHUNKREADER_H

#include <QDataStream>
#include <QStack>

QT_FORWARD_DECLARE_CLASS(QIODevice)

#define ChunkId(a,b,c,d)    quint32((quint32(d & 0x7f) << 24) | (quint32(c & 0x7f) << 16) | (quint32(b & 0x7f) << 8) | quint32(a & 0x7f))

class ChunkReader {
public:
    ChunkReader(QIODevice *dev, QDataStream::ByteOrder bo);

    QDataStream &dataStream();

    bool startChunk();
    bool endChunk();
    bool skipChunk();

    quint32 chunkId() const;
    quint32 chunkVersion() const;
    qint64 chunkSize() const;

private:
    struct read_chunk_info {
        quint32 id;
        quint32 version;
        qint64 startpos;
        qint64 size;
    };

    QStack<read_chunk_info> m_chunks;
    QIODevice * m_file;
    QDataStream m_stream;
};

class ChunkWriter {
public:
    ChunkWriter(QIODevice *dev, QDataStream::ByteOrder bo);
    ~ChunkWriter();

    QDataStream &dataStream();

    bool startChunk(quint32 id, quint32 version = 0);
    bool endChunk();

private:
    struct write_chunk_info {
        quint32 id;
        quint32 version;
        qint64 startpos;
    };

    QStack<write_chunk_info> m_chunks;
    QIODevice * m_file;
    QDataStream m_stream;
};

#endif
