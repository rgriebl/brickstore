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
#include <QDir>
#include <QCoreApplication>

#include "config.h"
#include "recentfiles.h"


RecentFiles *RecentFiles::s_inst = nullptr;

RecentFiles::RecentFiles(QObject *parent)
    : QAbstractListModel(parent)
{
    const auto saved = Config::inst()->recentFiles();
    for (const auto &file : saved)
        m_fileInfos << QFileInfo(file);
}

RecentFiles *RecentFiles::inst()
{
    if (!s_inst)
        s_inst = new RecentFiles(QCoreApplication::instance());
    return s_inst;
}

void RecentFiles::add(const QString &file)
{
    QFileInfo fi(file);
    bool save = true;

    int idx = m_fileInfos.indexOf(fi);
    if (idx > 0) {
        beginMoveRows({ }, idx, idx, { }, 0);
        m_fileInfos.move(idx, 0);
        endMoveRows();
    } else if (idx < 0) {
        if (m_fileInfos.size() >= MaxRecentFiles) {
            beginRemoveRows({ }, MaxRecentFiles - 1, count() - 1);
            m_fileInfos.resize(MaxRecentFiles - 1);
            endRemoveRows();
        }
        beginInsertRows({ }, 0, 0);
        m_fileInfos.prepend(fi);
        endInsertRows();
    } else {
        save = false;
    }

    if (save) {
        QStringList recent;
        for (const auto &fi : qAsConst(m_fileInfos))
            recent << QDir::toNativeSeparators(fi.absoluteFilePath());
        Config::inst()->setRecentFiles(recent);
    }
}

void RecentFiles::clear()
{
    beginRemoveRows({ }, 0, count() - 1);
    m_fileInfos.clear();
    endRemoveRows();

    Config::inst()->setRecentFiles({ });
}

int RecentFiles::count() const
{
    return m_fileInfos.count();
}

int RecentFiles::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant RecentFiles::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return { };

    const QFileInfo &fi = m_fileInfos.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return QDir::toNativeSeparators(fi.absoluteFilePath());
    case FilePathRole:
        return fi.absoluteFilePath();
    case FileNameRole:
        return fi.fileName();
    case DirNameRole:
        return QDir::toNativeSeparators(fi.absolutePath());
    case LastModifiedRole:
        return fi.lastModified();
    }
    return { };
}

QHash<int, QByteArray> RecentFiles::roleNames() const
{
    return {
        { Qt::DisplayRole, "display" },
        { FilePathRole, "filePath" },
        { FileNameRole, "fileName" },
        { DirNameRole, "dirName" },
        { LastModifiedRole, "lastModified" },
    };
}

void RecentFiles::open(int index)
{
    emit openDocument(data(createIndex(index, 0), FilePathRole).toString());
}

#include "moc_recentfiles.cpp"
