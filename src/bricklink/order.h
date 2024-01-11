// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QMetaType>
#include <QtCore/QDateTime>
#include <QtCore/QAbstractTableModel>
#include <QtQml/qqmlregistration.h>

#include "bricklink/lot.h"
#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QSaveFile)

class TransferJob;

namespace BrickLink {

class OrderPrivate;
class OrdersPrivate;


class Order : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(QString id READ id NOTIFY idChanged FINAL)
    Q_PROPERTY(BrickLink::OrderType type READ type NOTIFY typeChanged FINAL)
    Q_PROPERTY(QString otherParty READ otherParty NOTIFY otherPartyChanged FINAL)
    Q_PROPERTY(QDate date READ date NOTIFY dateChanged FINAL)
    Q_PROPERTY(QDate lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged FINAL)
    Q_PROPERTY(double shipping READ shipping NOTIFY shippingChanged FINAL)
    Q_PROPERTY(double insurance READ insurance NOTIFY insuranceChanged FINAL)
    Q_PROPERTY(double additionalCharges1 READ additionalCharges1 NOTIFY additionalCharges1Changed FINAL)
    Q_PROPERTY(double additionalCharges2 READ additionalCharges2 NOTIFY additionalCharges2Changed FINAL)
    Q_PROPERTY(double credit READ credit NOTIFY creditChanged FINAL)
    Q_PROPERTY(double creditCoupon READ creditCoupon NOTIFY creditCouponChanged FINAL)
    Q_PROPERTY(double orderTotal READ orderTotal NOTIFY orderTotalChanged FINAL)
    Q_PROPERTY(double usSalesTax READ usSalesTax NOTIFY usSalesTaxChanged FINAL)
    Q_PROPERTY(double vatChargeBrickLink READ vatChargeBrickLink NOTIFY vatChargeBrickLinkChanged FINAL)
    Q_PROPERTY(QString currencyCode READ currencyCode NOTIFY currencyCodeChanged FINAL)
    Q_PROPERTY(double grandTotal READ grandTotal NOTIFY grandTotalChanged FINAL)
    Q_PROPERTY(QString paymentCurrencyCode READ paymentCurrencyCode NOTIFY paymentCurrencyCodeChanged FINAL)
    Q_PROPERTY(int lotCount READ lotCount NOTIFY lotCountChanged FINAL)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged FINAL)
    Q_PROPERTY(double cost READ cost NOTIFY costChanged FINAL)
    Q_PROPERTY(BrickLink::OrderStatus status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(QString paymentType READ paymentType NOTIFY paymentTypeChanged FINAL)
    Q_PROPERTY(QString remarks READ remarks NOTIFY remarksChanged FINAL)
    Q_PROPERTY(QString trackingNumber READ trackingNumber NOTIFY trackingNumberChanged FINAL)
    Q_PROPERTY(QString paymentStatus READ paymentStatus NOTIFY paymentStatusChanged FINAL)
    Q_PROPERTY(QDate paymentLastUpdated READ paymentLastUpdated NOTIFY paymentLastUpdatedChanged FINAL)
    Q_PROPERTY(double vatChargeSeller READ vatChargeSeller NOTIFY vatChargeSellerChanged FINAL)
    Q_PROPERTY(QString countryCode READ countryCode NOTIFY countryCodeChanged FINAL)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged FINAL)
    Q_PROPERTY(QString phone READ phone NOTIFY phoneChanged FINAL)

public:
    Order();
    Order(const QString &id, OrderType type);
    ~Order() override;

    LotList loadLots() const; // ownership is transferred to the caller

    QString id() const;
    OrderType type() const;
    QString otherParty() const;
    QDate date() const;
    QDate lastUpdated() const;
    double shipping() const;
    double insurance() const;
    double additionalCharges1() const;
    double additionalCharges2() const;
    double credit() const;
    double creditCoupon() const;
    double orderTotal() const;
    double usSalesTax() const;
    double vatChargeBrickLink() const;
    QString currencyCode() const;
    double grandTotal() const;
    QString paymentCurrencyCode() const;
    int lotCount() const;
    int itemCount() const;
    double cost() const;
    OrderStatus status() const;
    QString paymentType() const;
    QString remarks() const;
    QString trackingNumber() const;
    QString paymentStatus() const;
    QDate paymentLastUpdated() const;
    double vatChargeSeller() const;
    QString countryCode() const;
    QString address() const;
    QString phone() const;

    void setId(const QString &id);
    void setType(OrderType type);
    void setOtherParty(const QString &str);
    void setDate(const QDate &dt);
    void setLastUpdated(const QDate &dt);
    void setShipping(double m);
    void setInsurance(double m);
    void setAdditionalCharges1(double m);
    void setAdditionalCharges2(double m);
    void setCredit(double m);
    void setCreditCoupon(double m);
    void setOrderTotal(double m);
    void setUsSalesTax(double m);
    void setVatChargeBrickLink(double m);
    void setCurrencyCode(const QString &str);
    void setGrandTotal(double m);
    void setPaymentCurrencyCode(const QString &str);
    void setLotCount(int i);
    void setItemCount(int i);
    void setCost(double c);
    void setStatus(OrderStatus status);
    void setPaymentType(const QString &str);
    void setRemarks(const QString &str);
    void setTrackingNumber(const QString &str);
    void setPaymentStatus(const QString &str);
    void setPaymentLastUpdated(const QDate &dt);
    void setVatChargeSeller(double m);
    void setCountryCode(const QString &str);
    void setAddress(const QString &str);
    void setPhone(const QString &str);

    static OrderStatus statusFromString(const QString &s);
    Q_INVOKABLE QString statusAsString(bool translated = true) const;

signals:
    void idChanged(const QString &id);
    void typeChanged(BrickLink::OrderType type);
    void otherPartyChanged(const QString &str);
    void dateChanged(const QDate &dt);
    void lastUpdatedChanged(const QDate &dt);

    void shippingChanged(double m);
    void insuranceChanged(double m);
    void additionalCharges1Changed(double m);
    void additionalCharges2Changed(double m);
    void creditChanged(double m);
    void creditCouponChanged(double m);
    void orderTotalChanged(double m);
    void usSalesTaxChanged(double m);
    void vatChargeBrickLinkChanged(double m);
    void currencyCodeChanged(const QString &str);
    void grandTotalChanged(double m);
    void paymentCurrencyCodeChanged(const QString &str);
    void lotCountChanged(int i);
    void itemCountChanged(int i);
    void costChanged(double d);
    void statusChanged(BrickLink::OrderStatus status);
    void paymentTypeChanged(const QString &str);
    void remarksChanged(const QString &str);
    void trackingNumberChanged(const QString &str);
    void paymentStatusChanged(const QString &str);
    void paymentLastUpdatedChanged(const QDate &dt);
    void vatChargeSellerChanged(double m);
    void countryCodeChanged(const QString &str);
    void addressChanged(const QString &str);
    void phoneChanged(const QString &str);

private:
    std::unique_ptr<OrderPrivate> d;

    friend class Orders;
};



class Orders : public QAbstractTableModel
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
        Status,
        OrderId,
        OtherParty,
        ItemCount,
        LotCount,
        Total,

        ColumnCount,
    };

    enum Role {
        OrderPointerRole = Qt::UserRole + 1,
        OrderSortRole,
        DateRole,
        TypeRole,
    };

    ~Orders() override;

    QDateTime lastUpdated() const;
    BrickLink::UpdateStatus updateStatus() const;

    Q_INVOKABLE void startUpdate(const QString &id);
    Q_INVOKABLE void startUpdate(int lastNDays);
    Q_INVOKABLE void startUpdate(const QDate &fromDate, const QDate &toDate);
    Q_INVOKABLE void cancelUpdate();

    Q_INVOKABLE BrickLink::Order *order(int index) const;

    //Q_INVOKABLE void trimDatabase(int keepLastNDays);

    LotList loadOrderLots(const Order *order) const;

    int indexOfOrder(const QString &orderId) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void updateStatusChanged(BrickLink::UpdateStatus updateStatus);
    void lastUpdatedChanged(const QDateTime &lastUpdated);
    void countChanged(int count);

private:
    Orders(Core *core);
    void reloadOrdersFromDatabase(const QString &userId);
    void importOldCache(const QString &userId);
    static QHash<Order *, QString> parseOrdersXML(const QByteArray &data_);
    void startUpdateInternal(const QDate &fromDate, const QDate &toDate, const QString &orderId);
    void updateOrder(std::unique_ptr<Order> order);
    void appendOrderToModel(std::unique_ptr<Order> order);
    void setLastUpdated(const QDateTime &lastUpdated);
    void setUpdateStatus(UpdateStatus updateStatus);
    void emitDataChanged(int row, int col);
    void startUpdateAddress(Order *order);
    std::pair<QString, QString> parseAddressAndPhone(OrderType type, const QByteArray &data);

    std::unique_ptr<OrdersPrivate> d;

    friend class Core;
};


} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Order *)
Q_DECLARE_METATYPE(const BrickLink::Order *)
