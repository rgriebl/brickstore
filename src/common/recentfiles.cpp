// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
    int size = config->beginReadArray(u"RecentFiles"_qs);
    m_entries.reserve(size);
    for (int i = 0; i < size; ++i) {
        config->setArrayIndex(i);
        auto path = config->value(u"Path"_qs).toString();
        auto name = config->value(u"Name"_qs).toString();
        auto pinned = config->value(u"Pinned"_qs).toBool();
        m_entries.insert(pinned ? m_pinnedCount++ : m_entries.count(), { path, name, pinned });

        if ((m_entries.count() - m_pinnedCount) >= MaxRecentFiles)
            break;
    }
    config->endArray();
}

void RecentFiles::save()
{
    emit recentFilesChanged();

    auto config = Config::inst();
    config->beginWriteArray(u"RecentFiles"_qs);
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &pan = m_entries.at(i);
        config->setArrayIndex(i);
        config->setValue(u"Path"_qs, pan.path);
        config->setValue(u"Name"_qs, pan.name);
        config->setValue(u"Pinned"_qs, pan.pinned);
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
    auto it = std::find_if(m_entries.cbegin(), m_entries.cend(), [=](const auto &entry) {
        return (entry.path == absFilePath) && (entry.name == fileName);
    });

    if (it != m_entries.cend()) {
        int idx = int(std::distance(m_entries.cbegin(), it));
        if (idx < m_pinnedCount) {
            if (beginMoveRows({ }, idx, idx, { }, 0)) {
                m_entries.move(idx, 0);
                endMoveRows();
                save();
            }
        } else {
            if (beginMoveRows({ }, idx, idx, { }, int(m_pinnedCount))) {
                m_entries.move(idx, m_pinnedCount);
                endMoveRows();
                save();
            }
        }
    } else {
        if ((m_entries.size() - m_pinnedCount) >= MaxRecentFiles) {
            beginRemoveRows({ }, int(m_pinnedCount) + MaxRecentFiles - 1, count() - 1);
            m_entries.resize(m_pinnedCount + MaxRecentFiles - 1);
            endRemoveRows();
        }
        beginInsertRows({ }, int(m_pinnedCount), int(m_pinnedCount));
        m_entries.insert(m_pinnedCount, { absFilePath, fileName, false });
        endInsertRows();
        save();
    }
}

void RecentFiles::pin(int row, bool down)
{
    if (m_entries.at(row).pinned == down)
        return;

    int finalRow = row;

    m_entries[row].pinned = down;

    if (down) { // move to 0
        ++m_pinnedCount;
        if (beginMoveRows({ }, row, row, { }, 0)) {
            m_entries.move(row, 0);
            endMoveRows();
            save();
            finalRow = 0;
        }
    } else { // move to --m_pinnedCount (note: overlap possible)
        --m_pinnedCount;
        if (beginMoveRows({ }, row, row, { }, int(m_pinnedCount) + 1)) {
            m_entries.move(row, m_pinnedCount);
            endMoveRows();
            save();

            finalRow = m_pinnedCount;
        }
        // We might end up with more than MaxRecentFiles entries, but we allow it to give
        // the user a chance to non-destructively undo this operation.
        // The list will be shortened on restart or when a non-recent file is loaded.
    }
    emit dataChanged(index(finalRow, 0), index(finalRow, 0), { PinnedRole });
    emit recentFilesChanged();
}

void RecentFiles::clearRecent()
{
    beginRemoveRows({ }, int(m_pinnedCount), count() - 1);
    m_entries.remove(m_pinnedCount, count() - m_pinnedCount);
    endRemoveRows();
    save();
}

void RecentFiles::clearPinned()
{
    beginRemoveRows({ }, 0, int(m_pinnedCount) - 1);
    m_entries.remove(0, m_pinnedCount);
    m_pinnedCount = 0;
    endRemoveRows();
    save();
}

int RecentFiles::count() const
{
    return int(m_entries.count());
}

int RecentFiles::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant RecentFiles::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return { };

    const auto &[filePath, fileName, isPinned] = m_entries.at(index.row());
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
    case PinnedRole:
        return isPinned;
    }
    return { };
}

bool RecentFiles::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && (role == PinnedRole)) {
        pin(index.row(), value.toBool());
        return true;
    }
    return false;
}

QHash<int, QByteArray> RecentFiles::roleNames() const
{
    return {
        { Qt::DisplayRole, "display" },
        { FilePathRole, "filePath" },
        { FileNameRole, "fileName" },
        { DirNameRole, "dirName" },
        { PinnedRole, "pinned" },
    };
}

void RecentFiles::open(int index)
{
    if ((index >= 0) && (index < m_entries.size()))
        emit openDocument(m_entries.at(index).path);
}

#include "moc_recentfiles.cpp"
