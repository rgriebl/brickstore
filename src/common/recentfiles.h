// Copyright (C) 2004-2023 Robert Griebl
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
    void clear();
    int count() const;

    enum Roles {
        FilePathRole = Qt::UserRole,
        FileNameRole,
        DirNameRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    std::pair<QString, QString> filePathAndName(int index) const;

    Q_INVOKABLE void open(int index);

signals:
    void countChanged(int count);
    void recentFilesChanged();
    void openDocument(const QString &fileName);

private:
    RecentFiles(QObject *parent = nullptr);
    void save();

    QVector<std::pair<QString, QString>> m_pathsAndNames;

    static RecentFiles *s_inst;
};
