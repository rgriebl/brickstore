// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHUNKREADER_H
#define CHUNKREADER_H

#include <QDataStream>
#include <QStack>

QT_FORWARD_DECLARE_CLASS(QIODevice)


template<std::size_t N>
constexpr quint64 ChunkIdAndVersion(char const(&str)[N], uint v = 0)
{
    static_assert(N-1 == 4, "Chunk ID must be 4 characters long");
    return (quint64(v) << 32)
           | (quint32(str[3] & 0x7f) << 24)
           | (quint32(str[2] & 0x7f) << 16)
           | (quint32(str[1] & 0x7f) << 8)
           | (quint32(str[0] & 0x7f));
}

template<std::size_t N>
constexpr quint32 ChunkId(char const(&str)[N])
{
    return quint32(ChunkIdAndVersion(str));
}

class ChunkReader {
public:
    ChunkReader(QIODevice *dev, QDataStream::ByteOrder bo);

    QDataStream &dataStream();

    // all 3 throw Exceptions on error
    bool startChunk();
    void endChunk();
    void skipChunk();

    quint32 chunkId() const;
    quint32 chunkVersion() const;
    quint64 chunkIdAndVersion() const;
    qint64 chunkSize() const;

private:
    void checkStream();
    void checkChunkStarted();

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

    // all 3 throw Exceptions on error
    void startChunk(quint32 id, quint32 version = 0);
    template<std::size_t N>
    constexpr void startChunk(char const(&str)[N], uint version = 0)
    {
        startChunk(ChunkId(str), version);
    }
    void endChunk();

private:
    void checkStream();

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
