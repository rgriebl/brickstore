// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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

class CartPrivate;


class Cart : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(bool domestic READ domestic NOTIFY domesticChanged FINAL)
    Q_PROPERTY(int sellerId READ sellerId NOTIFY sellerIdChanged FINAL)
    Q_PROPERTY(QString sellerName READ sellerName NOTIFY sellerNameChanged FINAL)
    Q_PROPERTY(QString storeName READ storeName NOTIFY storeNameChanged FINAL)
    Q_PROPERTY(QDate lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged FINAL)
    Q_PROPERTY(double total READ total NOTIFY totalChanged FINAL)
    Q_PROPERTY(QString currencyCode READ currencyCode NOTIFY currencyCodeChanged FINAL)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged FINAL)
    Q_PROPERTY(int lotCount READ lotCount NOTIFY lotCountChanged FINAL)
    Q_PROPERTY(QString countryCode READ countryCode NOTIFY countryCodeChanged FINAL)
    Q_PROPERTY(LotList lots READ lots NOTIFY lotsChanged FINAL)

public:
    Cart();
    ~Cart() override;

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
    const LotList &lots() const;

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
    void setLots(LotList &&lots);

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
    std::unique_ptr<CartPrivate> d;
};


class Carts : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

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
        LastUpdatedRole,
        DomesticRole,
    };

    QDateTime lastUpdated() const { return m_lastUpdated; }
    BrickLink::UpdateStatus updateStatus() const  { return m_updateStatus; }

    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancelUpdate();

    Q_INVOKABLE void startFetchLots(BrickLink::Cart *cart);

    Q_INVOKABLE BrickLink::Cart *cart(int index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void fetchLotsFinished(BrickLink::Cart *cart, bool success, const QString &message);
    void updateStatusChanged(BrickLink::UpdateStatus updateStatus);
    void lastUpdatedChanged(const QDateTime &lastUpdated);
    void countChanged(int count);

private:
    Carts(Core *core);
    QVector<Cart *> parseGlobalCart(const QByteArray &data);
    int parseSellerCart(Cart *cart, const QByteArray &data);
    void emitDataChanged(int row, int col);
    void setLastUpdated(const QDateTime &lastUpdated);
    void setUpdateStatus(UpdateStatus updateStatus);

    Core *m_core;
    UpdateStatus m_updateStatus = UpdateStatus::UpdateFailed;
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
