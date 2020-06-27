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
#ifndef CHUNKREADER_H
#define CHUNKREADER_H

#include <QDataStream>
#include <QStack>

class QIODevice;

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
    struct chunk_info {
        quint32 id;
        quint32 version;
        qint64 startpos;
        qint64 size;
    };

    QStack<chunk_info> m_chunks;
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
    struct chunk_info {
        quint32 id;
        quint32 version;
        qint64 startpos;
    };

    QStack<chunk_info> m_chunks;
    QIODevice * m_file;
    QDataStream m_stream;
};

#endif
