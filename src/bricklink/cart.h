/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include "lot.h"
#include "global.h"

QT_FORWARD_DECLARE_CLASS(QSaveFile)

class TransferJob;

namespace BrickLink {

class CartPrivate;


class Cart : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool domestic READ domestic NOTIFY domesticChanged)
    Q_PROPERTY(int sellerId READ sellerId NOTIFY sellerIdChanged)
    Q_PROPERTY(QString sellerName READ sellerName NOTIFY sellerNameChanged)
    Q_PROPERTY(QString storeName READ storeName NOTIFY storeNameChanged)
    Q_PROPERTY(QDate lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(double total READ total NOTIFY totalChanged)
    Q_PROPERTY(QString currencyCode READ currencyCode NOTIFY currencyCodeChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(int lotCount READ lotCount NOTIFY lotCountChanged)
    Q_PROPERTY(QString countryCode READ countryCode NOTIFY countryCodeChanged)
    Q_PROPERTY(LotList lots READ lots NOTIFY lotsChanged)

public:
    Cart();

    bool domestic() const;
    int sellerId() const;
    QString sellerName() const;
    QString storeName() const;
    QDate lastUpdated() const;
    double total() const;
    QString currencyCode() const;
    int itemCount() const;
    int lotCount() const;
    QString countryCode() const;
    LotList lots() const;

    void setDomestic(bool domestic);
    void setSellerId(int id);
    void setSellerName(const QString &name);
    void setStoreName(const QString &name);
    void setLastUpdated(const QDate &dt);
    void setTotal(double m);
    void setCurrencyCode(const QString &str);
    void setItemCount(int i);
    void setLotCount(int i);
    void setCountryCode(const QString &str);
    void setLots(const LotList &lots);

signals:
    void domesticChanged(bool domestic);
    void sellerIdChanged(int id);
    void sellerNameChanged(const QString &name);
    void storeNameChanged(const QString &name);
    void lastUpdatedChanged(const QDate &dt);
    void totalChanged(double m);
    void currencyCodeChanged(const QString &str);
    void itemCountChanged(int i);
    void lotCountChanged(int i);
    void countryCodeChanged(const QString &str);
    void lotsChanged(const BrickLink::LotList &lots);

private:
    QScopedPointer<CartPrivate> d;
};


class Carts : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid NOTIFY updateFinished)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateFinished)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY updateFinished)
    Q_PROPERTY(QVector<Cart *> carts READ carts NOTIFY updateFinished)

public:
    enum Column {
        Date = 0,
        Type,
        Store,
        ItemCount,
        LotCount,
        Total,

        ColumnCount,
    };

    enum Role {
        CartPointerRole = Qt::UserRole + 1,
        CartSortRole,
    };

    bool isValid() const          { return m_valid; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    BrickLink::UpdateStatus updateStatus() const  { return m_updateStatus; }

    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancelUpdate();

    QVector<Cart *> carts() const;

    void startFetchLots(Cart *cart);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void fetchLotsFinished(Cart *cart, bool success, const QString &message);

private:
    Carts(QObject *parent = nullptr);
    QVector<Cart *> parseGlobalCart(const QByteArray &data);
    int parseSellerCart(Cart *cart, const QByteArray &data);
    void emitDataChanged(int row, int col);

    bool m_valid = false;
    BrickLink::UpdateStatus m_updateStatus = BrickLink::UpdateStatus::UpdateFailed;
    TransferJob *m_job = nullptr;
    QVector<TransferJob *> m_cartJobs;
    QDateTime m_lastUpdated;
    QVector<Cart *> m_carts;
    mutable QHash<QString, QIcon> m_flags;

    friend class Core;
};


} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Cart *)
Q_DECLARE_METATYPE(const BrickLink::Cart *)
