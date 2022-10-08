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

#include <QtCore/QObject>
#include <QtCore/QMetaType>
#include <QtCore/QDateTime>
#include <QtCore/QAbstractTableModel>
#include <QtQml/qqmlregistration.h>

#include "lot.h"
#include "global.h"

QT_FORWARD_DECLARE_CLASS(QSaveFile)

class TransferJob;

namespace BrickLink {

class WantedListPrivate;


class WantedList : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(int id READ id NOTIFY idChanged FINAL)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged FINAL)
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged FINAL)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged FINAL)
    Q_PROPERTY(int itemLeftCount READ itemLeftCount NOTIFY itemLeftCountChanged FINAL)
    Q_PROPERTY(int lotCount READ lotCount NOTIFY lotCountChanged FINAL)
    Q_PROPERTY(double filled READ filled NOTIFY filledChanged FINAL)
    Q_PROPERTY(LotList lots READ lots NOTIFY lotsChanged FINAL)

public:
    WantedList();
    ~WantedList() override;

    int id() const;
    QString name() const;
    QString description() const;
    int itemCount() const;
    int itemLeftCount() const;
    int lotCount() const;
    double filled() const;
    const LotList &lots() const;

    void setId(int id);
    void setName(const QString &name);
    void setDescription(const QString &description);
    void setItemCount(int i);
    void setItemLeftCount(int i);
    void setLotCount(int i);
    void setFilled(double f);
    void setLots(LotList &&lots);

signals:
    void idChanged(int id);
    void nameChanged(const QString &name);
    void descriptionChanged(const QString &description);
    void itemCountChanged(int i);
    void itemLeftCountChanged(int i);
    void lotCountChanged(int i);
    void filledChanged(double f);
    void lotsChanged(const BrickLink::LotList &lots);

private:
    std::unique_ptr<WantedListPrivate> d;
};


class WantedLists : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(bool valid READ isValid NOTIFY updateFinished FINAL)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY updateFinished FINAL)
    Q_PROPERTY(QVector<WantedList *> wantedLists READ wantedLists NOTIFY updateFinished FINAL)

public:
    enum Column {
        Name = 0,
        Description,
        ItemCount,
        ItemLeftCount,
        LotCount,
        Filled,
        Total,

        ColumnCount,
    };

    enum Role {
        WantedListPointerRole = Qt::UserRole + 1,
        WantedListSortRole,
    };

    bool isValid() const          { return m_valid; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    BrickLink::UpdateStatus updateStatus() const  { return m_updateStatus; }

    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancelUpdate();

    QVector<WantedList *> wantedLists() const;

    Q_INVOKABLE void startFetchLots(BrickLink::WantedList *wantedList);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void fetchLotsFinished(BrickLink::WantedList *wantedList, bool success, const QString &message);
    void updateStatusChanged(BrickLink::UpdateStatus updateStatus);

private:
    WantedLists(Core *core);
    QVector<WantedList *> parseGlobalWantedList(const QByteArray &data);
    int parseWantedList(WantedList *wantedList, const QByteArray &data);
    void emitDataChanged(int row, int col);
    void setUpdateStatus(UpdateStatus updateStatus);

    Core *m_core;
    bool m_valid = false;
    UpdateStatus m_updateStatus = UpdateStatus::UpdateFailed;
    TransferJob *m_job = nullptr;
    QVector<TransferJob *> m_wantedListJobs;
    QDateTime m_lastUpdated;
    QVector<WantedList *> m_wantedLists;
    mutable QHash<QString, QIcon> m_flags;

    friend class Core;
};


} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::WantedList *)
Q_DECLARE_METATYPE(const BrickLink::WantedList *)
