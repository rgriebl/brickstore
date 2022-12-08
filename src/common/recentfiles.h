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
