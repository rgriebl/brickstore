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
#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "QtCore/private/qabstractfileengine_p.h"

class QTemporaryResourceFileEnginePrivate;
class QTemporaryResourceFileEngine : public QAbstractFileEngine
{
private:
//    Q_DECLARE_PRIVATE(QTemporaryResourceFileEngine)
    QTemporaryResourceFileEnginePrivate *d;
public:
    explicit QTemporaryResourceFileEngine(const QString &path);

    void setFileName(const QString &file) override;

    bool open(QIODevice::OpenMode flags) override;
    bool close() override;
    bool flush() override;
    qint64 size() const override;
    qint64 pos() const override;
    bool atEnd() const;
    bool seek(qint64) override;
    qint64 read(char *data, qint64 maxlen) override;
    qint64 write(const char *data, qint64 len) override;

    bool remove() override;
    bool copy(const QString &newName) override;
    bool rename(const QString &newName) override;
    bool link(const QString &newName) override;

    bool isSequential() const override;

    bool isRelativePath() const override;

    bool mkdir(const QString &dirName, bool createParentDirectories) const override;
    bool rmdir(const QString &dirName, bool recurseParentDirectories) const override;

    bool setSize(qint64 size) override;

    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const override;

    bool caseSensitive() const override;

    FileFlags fileFlags(FileFlags type) const override;

    bool setPermissions(uint perms) override;

    QString fileName(QAbstractFileEngine::FileName file) const override;

    uint ownerId(FileOwner) const override;
    QString owner(FileOwner) const override;

    QDateTime fileTime(FileTime time) const override;

    bool extension(Extension extension, const ExtensionOption *option = nullptr, ExtensionReturn *output = nullptr) override;
    bool supportsExtension(Extension extension) const override;
};
