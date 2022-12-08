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
    connect(this, &QAbstractListModel::rowsInserted, this, [this]() {
        emit countChanged(count());
    });
    connect(this, &QAbstractListModel::rowsRemoved, this, [this]() {
        emit countChanged(count());
    });

    auto config = Config::inst();
    auto size = qBound(0, config->beginReadArray(u"RecentFiles"_qs), MaxRecentFiles);
    for (int i = 0; i < size; ++i) {
        config->setArrayIndex(i);
        auto path = config->value(u"Path"_qs).toString();
        auto name = config->value(u"Name"_qs).toString();
        m_pathsAndNames.append({ path, name });
    }
    config->endArray();
}

void RecentFiles::save()
{
    emit recentFilesChanged();

    auto config = Config::inst();
    config->beginWriteArray(u"RecentFiles"_qs);
    for (int i = 0; i < m_pathsAndNames.size(); ++i) {
        const auto &pan = m_pathsAndNames.at(i);
        config->setArrayIndex(i);
        config->setValue(u"Path"_qs, pan.first);
        config->setValue(u"Name"_qs, pan.second);
    }
    config->endArray();
}

RecentFiles *RecentFiles::inst()
{
    if (!s_inst)
        s_inst = new RecentFiles(QCoreApplication::instance());
    return s_inst;
}

void RecentFiles::add(const QString &filePath, const QString &fileName)
{
    QString absFilePath = filePath;
#if !defined(BS_MOBILE)
    absFilePath = QFileInfo(filePath).absoluteFilePath();
#endif

    auto pan = std::make_pair(absFilePath, fileName);

    int idx = int(m_pathsAndNames.indexOf(pan));
    if (idx > 0) {
        beginMoveRows({ }, idx, idx, { }, 0);
        m_pathsAndNames.move(idx, 0);
        endMoveRows();
        save();
    } else if (idx < 0) {
        if (m_pathsAndNames.size() >= MaxRecentFiles) {
            beginRemoveRows({ }, MaxRecentFiles - 1, count() - 1);
            m_pathsAndNames.resize(MaxRecentFiles - 1);
            endRemoveRows();
        }
        beginInsertRows({ }, 0, 0);
        m_pathsAndNames.prepend(pan);
        endInsertRows();
        save();
    }
}

void RecentFiles::clear()
{
    beginRemoveRows({ }, 0, count() - 1);
    m_pathsAndNames.clear();
    endRemoveRows();
    save();
}

int RecentFiles::count() const
{
    return int(m_pathsAndNames.count());
}

int RecentFiles::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant RecentFiles::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return { };

    const auto [filePath, fileName] = m_pathsAndNames.at(index.row());
#if !defined(BS_MOBILE)
    QFileInfo fi(filePath);
#endif

    switch (role) {
    case Qt::DisplayRole:
#if defined(BS_MOBILE)
        return fileName;
#else
        return QDir::toNativeSeparators(fi.absoluteFilePath());
#endif
    case FilePathRole:
        return filePath;
    case FileNameRole:
        return fileName;
    case DirNameRole:
#if defined(BS_MOBILE)
        return QString { };
#else
        return QDir::toNativeSeparators(fi.absolutePath());
#endif
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
    };
}

std::pair<QString, QString> RecentFiles::filePathAndName(int index) const
{
    if ((index >= 0) && (index < m_pathsAndNames.size()))
        return m_pathsAndNames.at(index);
    else
        return { };
}

void RecentFiles::open(int index)
{
    if ((index >= 0) && (index < m_pathsAndNames.size()))
        emit openDocument(m_pathsAndNames.at(index).first);
}

#include "moc_recentfiles.cpp"
