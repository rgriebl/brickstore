// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractListModel>
#include <QFileInfo>
#include <QVector>


class RecentFiles : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    static RecentFiles *inst();

    static constexpr int MaxRecentFiles = 18;
    void add(const QString &filePath, const QString &fileName);
    void clearRecent();
    void clearPinned();
    int count() const;

    enum Roles {
        FilePathRole = Qt::UserRole,
        FileNameRole,
        DirNameRole,
        PinnedRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void open(int index);

signals:
    void countChanged(int count);
    void recentFilesChanged();
    void openDocument(const QString &fileName);

private:
    RecentFiles(QObject *parent = nullptr);
    void save();
    void pin(int row, bool down);

    struct Entry
    {
        QString path;
        QString name;
        bool pinned = false;
    };

    QVector<Entry> m_entries;
    qsizetype m_pinnedCount = 0;

    static RecentFiles *s_inst;
};
