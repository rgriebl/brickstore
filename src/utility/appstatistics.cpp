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
#include <QCoreApplication>
#include <QThread>

#include "appstatistics.h"


AppStatistics *AppStatistics::s_inst = nullptr;

AppStatistics *AppStatistics::inst()
{
    if (!s_inst)
        s_inst = new AppStatistics(qApp);
    return s_inst;
}

int AppStatistics::addSource(const QString &name, const QString &unit)
{
    Q_ASSERT(QThread::currentThread() == thread());

    beginInsertRows({ }, rowCount(), rowCount());

    int sid = ++m_nextSourceId;
    //m_sources.emplace_back(sid, name, unit, QVariant { }); // VS2022 internal compiler error
    m_sources.append({ sid, name, unit, QVariant { } });

    endInsertRows();
    emit sourceAdded(sid);

    if (!m_timer.isActive())
        m_timer.start();

    return sid;
}

void AppStatistics::removeSource(int sourceId)
{
    for (auto i = 0; i < m_sources.size(); ++i) {
        if (m_sources.at(i).id == sourceId) {
            emit sourceAboutToBeRemoved(sourceId);

            beginRemoveRows({ }, i, i);
            m_sources.removeAt(i);
            endRemoveRows();
            break;
        }
    }
}

QVector<int> AppStatistics::sourceIds() const
{
    QVector<int> sids;
    for (const auto &src : m_sources)
        sids << src.id;
    return sids;
}

QString AppStatistics::sourceName(int sourceId) const
{
    for (const auto &src : m_sources) {
        if (src.id == sourceId)
            return src.name;
    }
    return { };
}

QString AppStatistics::sourceUnit(int sourceId) const
{
    for (const auto &src : m_sources) {
        if (src.id == sourceId)
            return src.unit;
    }
    return { };
}

QVariant AppStatistics::sourceValue(int sourceId, const QVariant &defaultValue) const
{
    for (const auto &src : m_sources) {
        if (src.id == sourceId)
            return src.value;
    }
    return defaultValue;
}

int AppStatistics::updateInterval() const
{
    return m_updateInterval;
}

void AppStatistics::setUpdateInterval(int newUpdateInterval)
{
    if (newUpdateInterval != m_updateInterval) {
        m_updateInterval = newUpdateInterval;
        m_timer.setInterval(newUpdateInterval);
        emit updateIntervalChanged(newUpdateInterval);
    }
}

AppStatistics::AppStatistics(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_timer.setInterval(m_updateInterval);

    connect(&m_timer, &QTimer::timeout,
            this, [this]() {
        if (m_updateNeeded.testAndSetOrdered(true, false)) {
            // only take an expensive lock if we really have to
            m_mutex.lock();
            const auto updateHash = std::exchange(m_updateHash, { });
            m_mutex.unlock();

            QVector<std::pair<int, QVariant>> newData;
            int firstRow = std::numeric_limits<int>::max();
            int lastRow = std::numeric_limits<int>::min();

            for (auto uit = updateHash.cbegin(); uit != updateHash.cend(); ++uit) {
                int sid = uit.key();

                for (int i = 0; i < int(m_sources.size()); ++i) {
                    if (m_sources.at(i).id == sid) {
                        const auto &value = uit.value();
                        m_sources[i].value = value;
                        newData.append({ sid, value });
                        firstRow = std::min(firstRow, i);
                        lastRow = std::max(lastRow, i);
                    }
                }
            }
            if (!newData.isEmpty()) {
                emit valuesChanged(newData);
                emit dataChanged(index(firstRow, 1), index(lastRow, 1), { Qt::DisplayRole });
            }
        }
    });
}

void AppStatistics::update(int sourceId, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);
    m_updateHash.insert(sourceId, value);
    m_updateNeeded.testAndSetOrdered(false, true);
}

int AppStatistics::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

int AppStatistics::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_sources.size());
}

QVariant AppStatistics::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return m_sources.at(index.row()).name;
        case 1: return m_sources.at(index.row()).value.toString();
        }
    } else if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignVCenter) | ((index.column() == 1) ? Qt::AlignRight : Qt::AlignLeft);
    }
    return { };
}

QVariant AppStatistics::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return { };
    if (role != Qt::DisplayRole)
        return { };
    switch (section) {
    case 0: return u"Statistics"_qs;
    case 1: return u"Value"_qs;
    }
    return { };
}

#include "moc_appstatistics.cpp"
