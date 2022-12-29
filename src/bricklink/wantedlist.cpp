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

#include <QBuffer>
#include <QSaveFile>
#include <QDirIterator>
#include <QCoreApplication>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QQmlEngine>

#include "bricklink/wantedlist.h"
#include "bricklink/core.h"
#include "bricklink/io.h"

#include "utility/exception.h"
#include "utility/transfer.h"


namespace BrickLink {

class WantedListPrivate
{
public:
    ~WantedListPrivate() { qDeleteAll(m_lots); }

private:
    int       m_id;
    QString   m_name;
    QString   m_description;
    int       m_itemCount = 0;
    int       m_itemLeftCount = 0;
    int       m_lotCount = 0;
    double    m_filled = 0.;

    LotList   m_lots;

    friend class WantedList;
};


WantedList::WantedList()
    : QObject()
    , d(new WantedListPrivate)
{ }

WantedList::~WantedList()
{ }

const LotList &WantedList::lots() const
{
    return d->m_lots;
}

int WantedList::id() const
{
    return d->m_id;
}

QString WantedList::name() const
{
    return d->m_name;
}

QString WantedList::description() const
{
    return d->m_description;
}

double WantedList::filled() const
{
    return d->m_filled;
}

int WantedList::itemLeftCount() const
{
    return d->m_itemLeftCount;
}

int WantedList::itemCount() const
{
    return d->m_itemCount;
}

int WantedList::lotCount() const
{
    return d->m_lotCount;
}

void WantedList::setId(int id)
{
    if (d->m_id != id) {
        d->m_id = id;
        emit idChanged(id);
    }
}

void WantedList::setName(const QString &name)
{
    if (d->m_name != name) {
        d->m_name = name;
        emit nameChanged(name);
    }
}

void WantedList::setDescription(const QString &description)
{
    if (d->m_description != description) {
        d->m_description = description;
        emit descriptionChanged(description);
    }
}

void WantedList::setFilled(double f)
{
    if (!qFuzzyCompare(d->m_filled, f)) {
        d->m_filled = f;
        emit filledChanged(f);
    }
}

void WantedList::setItemLeftCount(int i)
{
    if (d->m_itemLeftCount != i) {
        d->m_itemLeftCount = i;
        emit itemLeftCountChanged(i);
    }
}

void WantedList::setItemCount(int i)
{
    if (d->m_itemCount != i) {
        d->m_itemCount = i;
        emit itemCountChanged(i);
    }
}

void WantedList::setLotCount(int i)
{
    if (d->m_lotCount != i) {
        d->m_lotCount = i;
        emit lotCountChanged(i);
    }
}

void WantedList::setLots(LotList &&lots)
{
    if (d->m_lots != lots) {
        qDeleteAll(d->m_lots);
        d->m_lots = lots;
        lots.clear();
        emit lotsChanged(d->m_lots);
    }
}



WantedLists::WantedLists(Core *core)
    : QAbstractTableModel(core)
    , m_core(core)
{
    connect(core, &Core::authenticatedTransferStarted,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job))
            emit updateStarted();
    });
    connect(core, &Core::authenticatedTransferProgress,
            this, [this](TransferJob *job, int progress, int total) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job))
            emit updateProgress(progress, total);
    });
    connect(core, &Core::authenticatedTransferFinished,
            this, [this](TransferJob *job) {
        bool jobCompleted = job->isCompleted() && (job->responseCode() == 200) && job->data();
        QByteArray type = job->userTag();

        if (m_wantedListJobs.contains(job) && (type == "wantedList")) {
            m_wantedListJobs.removeOne(job);

            bool success = jobCompleted;
            int id = job->userData(type).toInt();
            QString message = tr("Failed to import wanted list %1").arg(id);

            auto it = std::find_if(m_wantedLists.cbegin(), m_wantedLists.cend(), [id](const WantedList *wantedList) {
                return wantedList->id() == id;
            });
            if (it == m_wantedLists.cend()) {
                qWarning() << "Received wanted list data for an unknown wanted list:" << id;
                return;
            }

            try {
                if (!jobCompleted) {
                    throw Exception(message + u": " + job->errorString());
                } else {
                    int invalidCount = parseWantedList(*it, *job->data());
                    if (invalidCount) {
                        message = tr("%n lot(s) of your Wanted List could not be imported.",
                                     nullptr, invalidCount);
                    }
                    message.clear();
                }
            } catch (const Exception &e) {
                success = false;
                message = message + u": " + e.errorString();
            }
            emit fetchLotsFinished(*it, success, message);

        } else if ((job == m_job) && (type == "globalWantedList")) {
            bool success = jobCompleted;
            QString message = tr("Failed to import the wanted lists");
            if (success) {
                try {
                    beginResetModel();
                    qDeleteAll(m_wantedLists);
                    m_wantedLists.clear();
                    const auto wantedLists = parseGlobalWantedList(*job->data());
                    for (auto &wantedList : wantedLists) {
                        int row = int(m_wantedLists.count());
                        m_wantedLists.append(wantedList);
                        wantedList->setParent(this); // needed to prevent QML from taking ownership

                        connect(wantedList, &WantedList::nameChanged, this, [this, row]() { emitDataChanged(row, Name); });
                        connect(wantedList, &WantedList::descriptionChanged, this, [this, row]() { emitDataChanged(row, Description); });
                        connect(wantedList, &WantedList::filledChanged, this, [this, row]() { emitDataChanged(row, Filled); });
                        connect(wantedList, &WantedList::itemCountChanged, this, [this, row]() { emitDataChanged(row, ItemCount); });
                        connect(wantedList, &WantedList::lotCountChanged, this, [this, row]() { emitDataChanged(row, LotCount); });
                        connect(wantedList, &WantedList::itemLeftCountChanged, this, [this, row]() { emitDataChanged(row, ItemLeftCount); });
                    }
                    endResetModel();
                    message.clear();

                } catch (const Exception &e) {
                    success = false;
                    message = message + u": " + e.errorString();
                }
            }
            m_lastUpdated = QDateTime::currentDateTime();
            setUpdateStatus(success ? UpdateStatus::Ok : UpdateStatus::UpdateFailed);
            emit updateFinished(success, success ? QString { } : message);
            m_job = nullptr;
        }
    });

    connect(core->database(), &BrickLink::Database::databaseAboutToBeReset,
            this, [this]() {
        beginResetModel();
        qDeleteAll(m_wantedLists);
        m_wantedLists.clear();
        endResetModel();
    });
}

int WantedLists::parseWantedList(WantedList *wantedList, const QByteArray &data)
{
    IO::ParseResult pr = IO::fromBrickLinkXML(data, IO::Hint::Wanted);
    wantedList->setLots(pr.takeLots());
    return pr.invalidLotCount();
}

QVector<BrickLink::WantedList *> WantedLists::parseGlobalWantedList(const QByteArray &data)
{
    QVector<BrickLink::WantedList *> wantedLists;
    auto pos = data.indexOf("var wlJson");
    if (pos < 0)
        throw Exception("Invalid HTML - cannot parse");
    auto startPos = data.indexOf('{', pos);
    auto endPos = data.indexOf("};\r\n", pos);

    if ((startPos <= pos) || (endPos < startPos))
        throw Exception("Invalid HTML - found wlJSON, but could not parse line");
    auto globalWantedList = data.mid(startPos, endPos - startPos + 1);
    QJsonParseError err;
    auto json = QJsonDocument::fromJson(globalWantedList, &err);
    if (json.isNull())
        throw Exception("Invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

    const QJsonArray jsonWantedLists = json[u"wantedLists"].toArray();

    for (auto &&jsonWantedList : jsonWantedLists) {
        int id = jsonWantedList[u"id"].toInt(-1);
        QString name = jsonWantedList[u"name"].toString();
        QString desc = jsonWantedList[u"desc"].toString();
        double filled = jsonWantedList[u"filledPct"].toDouble();
        int lots = jsonWantedList[u"num"].toInt();
        int items = jsonWantedList[u"totalNum"].toInt();
        int itemsLeft = jsonWantedList[u"totalLeft"].toInt();

        if (id >= 0 && !name.isEmpty() && lots && items) {
            auto wantedList = new WantedList;
            wantedList->setId(id);
            wantedList->setName(name);
            wantedList->setDescription(desc);
            wantedList->setLotCount(lots);
            wantedList->setItemCount(items);
            wantedList->setItemLeftCount(itemsLeft);
            wantedList->setFilled(filled / 100.);

            wantedLists << wantedList;

            QQmlEngine::setObjectOwnership(wantedList, QQmlEngine::CppOwnership);
        }
    }
    return wantedLists;
}


void WantedLists::emitDataChanged(int row, int col)
{
    QModelIndex from = index(row, col < 0 ? 0 : col);
    QModelIndex to = index(row, col < 0 ? columnCount() - 1 : col);
    emit dataChanged(from, to);
}

void WantedLists::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}


void WantedLists::startUpdate()
{
    if (updateStatus() == UpdateStatus::Updating)
        return;
    Q_ASSERT(!m_job);
    setUpdateStatus(UpdateStatus::Updating);

    QUrl url(u"https://www.bricklink.com/v2/wanted/list.page"_qs);

    auto job = TransferJob::post(url);
    job->setUserData("globalWantedList", true);
    m_job = job;

    m_core->retrieveAuthenticated(m_job);
}

void WantedLists::cancelUpdate()
{
    if ((m_updateStatus == UpdateStatus::Updating) && m_job)
        m_job->abort();
}

WantedList *WantedLists::wantedList(int index) const
{
    return m_wantedLists.value(index);
}

void WantedLists::startFetchLots(WantedList *wantedList)
{
    if (!wantedList)
        return;

    QUrl url(u"https://www.bricklink.com/files/clone/wanted/downloadXML.file"_qs);
    QUrlQuery query;
    query.addQueryItem(u"wantedMoreID"_qs, QString::number(wantedList->id()));
    url.setQuery(query);

    auto job = TransferJob::post(url);

    job->setUserData("wantedList", QVariant::fromValue(wantedList->id()));
    m_wantedListJobs << job;

    m_core->retrieveAuthenticated(job);
}

int WantedLists::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_wantedLists.count());
}

int WantedLists::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant WantedLists::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= m_wantedLists.size()))
        return QVariant();

    WantedList *wantedList = m_wantedLists.at(index.row());
    int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
        case Name:          return wantedList->name();
        case Description:   return wantedList->description();
        case ItemLeftCount: return QLocale::system().toString(wantedList->itemLeftCount());
        case ItemCount:     return QLocale::system().toString(wantedList->itemCount());
        case LotCount:      return QLocale::system().toString(wantedList->lotCount());
        case Filled:        return QLocale::system().toString(int(wantedList->filled() * 100)).append(u'%');
        }
    } else if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignVCenter) | Qt::AlignLeft;
    } else if (role == WantedListPointerRole) {
        return QVariant::fromValue(wantedList);
    } else if (role == WantedListSortRole) {
        switch (col) {
        case Name:          return wantedList->name();
        case Description:   return wantedList->description();
        case ItemLeftCount: return wantedList->itemLeftCount();
        case ItemCount:     return wantedList->itemCount();
        case LotCount:      return wantedList->lotCount();
        case Filled:        return QString::number(int(wantedList->filled() * 100)).append(u'%');
        }
    } else if (role == NameRole) {
        return wantedList->name();
    }

    return { };
}

QVariant WantedLists::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
            case Name:          return tr("Name");
            case Description:   return tr("Description");
            case ItemCount:     return tr("Items");
            case ItemLeftCount: return tr("Items left");
            case LotCount:      return tr("Lots");
            case Filled:        return tr("Filled");
            }
        } else if (role == Qt::TextAlignmentRole) {
            return (section == Total) ? Qt::AlignRight : Qt::AlignLeft;
        }
    }
    return { };
}

QHash<int, QByteArray> WantedLists::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "display" },
        { Qt::TextAlignmentRole, "textAlignment" },
        { Qt::DecorationRole, "decoration" },
        { Qt::BackgroundRole, "background" },
        { WantedListPointerRole, "wantedList" },
        { NameRole, "name" },
    };
    return roles;
}

} // namespace BrickLink

#include "moc_wantedlist.cpp"
