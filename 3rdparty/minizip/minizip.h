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

#include <QCoreApplication>
#include <QHash>

QT_FORWARD_DECLARE_CLASS(QIODevice)


class MiniZip
{
    Q_DECLARE_TR_FUNCTIONS(MiniZip)

public:
    MiniZip(const QString &zipFileName);
    ~MiniZip();

    bool open();
    void close();
    QStringList fileList() const;
    bool contains(const QString &fileName) const;
    QByteArray readFile(const QString &fileName);

    static void unzip(const QString &zipFileName, QIODevice *destination,
                      const char *extractFileName, const char *extractPassword = nullptr);

private:
    bool openInternal(bool parseTOC);

    QString m_zipFileName;
    QHash<QByteArray, QPair<quint64, quint64>> m_contents;
    void *m_zip = nullptr;

};
