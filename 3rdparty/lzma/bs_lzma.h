// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
