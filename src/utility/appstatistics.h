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

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>
#include <QVariant>
#include <QDateTime>
#include <QStringList>
#include <QMutex>
#include <QTimer>
#include <QAtomicInteger>


class AppStatistics : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval NOTIFY updateIntervalChanged)

public:
    static AppStatistics *inst();

    int addSource(const QString &name, const QString &unit = { });
    void removeSource(int sourceId);

    QVector<int> sourceIds() const;
    QString sourceName(int sourceId) const;
    QString sourceUnit(int sourceId) const;
    QVariant sourceValue(int sourceId, const QVariant &defaultValue = { }) const;

    int updateInterval() const;
    void setUpdateInterval(int newUpdateInterval);

    // update() can be called from any thread
    void update(int sourceId, const QVariant &value);

    int columnCount(const QModelIndex &parent = { }) const override;
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    void updateIntervalChanged(int newUpdateInterval);
    void valuesChanged(const QVector<std::pair<int, QVariant>> &newSourceValues);
    void sourceAdded(int soureId);
    void sourceAboutToBeRemoved(int sourceId);

private:
    explicit AppStatistics(QObject *parent = nullptr);
    static AppStatistics *s_inst;

    struct Source {
        int id;
        QString name;
        QString unit;
        QVariant value;
    };
    QVector<Source> m_sources;
    int m_nextSourceId = 0;

    int m_updateInterval = 1000; // every second
    QTimer m_timer;

    QMutex m_mutex;
    QHash<int, QVariant> m_updateHash;
    QAtomicInteger<bool> m_updateNeeded = false;
};
