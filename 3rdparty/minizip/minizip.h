// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QCoreApplication>
#include <QHash>
#include <QDateTime>

QT_FORWARD_DECLARE_CLASS(QIODevice)


class MiniZip
{
    Q_DECLARE_TR_FUNCTIONS(MiniZip)

public:
    MiniZip(const QString &zipFileName);
    ~MiniZip();

    bool open();
    bool create();
    void close();
    bool isOpen() const;
    QString fileName() const;
    QStringList fileList() const;
    bool contains(const QString &fileName) const;
    QByteArray readFile(const QString &fileName);
    std::tuple<QByteArray, QDateTime> readFileAndLastModified(const QString &fileName);
    void writeFile(const QString &fileName, const QByteArray &data, const QDateTime &dateTime = { });

    static void unzip(const QString &zipFileName, QIODevice *destination,
                      const char *extractFileName, const char *extractPassword = nullptr);

private:
    bool openInternal(bool parseTOC);

    QString m_zipFileName;
    QHash<QByteArray, std::tuple<QByteArray, quint64, quint64>> m_contents;
    void *m_zip = nullptr;
    bool m_writing = false;
};
