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
#pragma once

#include <functional>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QIODevice>


class HashHeaderCheckFilter : public QIODevice
{
    Q_OBJECT

public:
    HashHeaderCheckFilter(QIODevice *target, QCryptographicHash::Algorithm alg = QCryptographicHash::Sha512, QObject* parent = nullptr);

    bool hasValidChecksum() const;

    bool open(OpenMode mode = WriteOnly) override;
    void close() override;
    bool isSequential() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    QIODevice *m_target;
    bool m_gotHeader = false;
    bool m_ok = false;
    QCryptographicHash m_hash;
    int m_hashSize;
    QByteArray m_header;

    Q_DISABLE_COPY(HashHeaderCheckFilter)
};

namespace LZMA {

class DecompressFilterPrivate;

class DecompressFilter : public QIODevice
{
    Q_OBJECT

public:
    DecompressFilter(QIODevice *target, QObject* parent = nullptr);
    ~DecompressFilter() override;

    bool open(OpenMode mode = WriteOnly) override;
    void close() override;
    bool isSequential() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    qint64 decompress(const char *data, qint64 maxSize);

private:
    DecompressFilterPrivate *d;

    Q_DISABLE_COPY(DecompressFilter)
};

}
