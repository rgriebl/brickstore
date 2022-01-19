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

class OrderPrivate;


class Order : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id NOTIFY idChanged)
    Q_PROPERTY(BrickLink::OrderType type READ type NOTIFY typeChanged)
    Q_PROPERTY(QDate date READ date NOTIFY dateChanged)
    Q_PROPERTY(QDate lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(QString otherParty READ otherParty NOTIFY otherPartyChanged)
    Q_PROPERTY(double shipping READ shipping NOTIFY shippingChanged)
    Q_PROPERTY(double insurance READ insurance NOTIFY insuranceChanged)
    Q_PROPERTY(double additionalCharges1 READ additionalCharges1 NOTIFY additionalCharges1Changed)
    Q_PROPERTY(double additionalCharges2 READ additionalCharges2 NOTIFY additionalCharges2Changed)
    Q_PROPERTY(double credit READ credit NOTIFY creditChanged)
    Q_PROPERTY(double creditCoupon READ creditCoupon NOTIFY creditCouponChanged)
    Q_PROPERTY(double orderTotal READ orderTotal NOTIFY orderTotalChanged)
    Q_PROPERTY(double usSalesTax READ usSalesTax NOTIFY usSalesTaxChanged)
    Q_PROPERTY(double grandTotal READ grandTotal NOTIFY grandTotalChanged)
    Q_PROPERTY(double vatChargeSeller READ vatChargeSeller NOTIFY vatChargeSellerChanged)
    Q_PROPERTY(double vatChargeBrickLink READ vatChargeBrickLink NOTIFY vatChargeBrickLinkChanged)
    Q_PROPERTY(QString currencyCode READ currencyCode NOTIFY currencyCodeChanged)
    Q_PROPERTY(QString paymentCurrencyCode READ paymentCurrencyCode NOTIFY paymentCurrencyCodeChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(int lotCount READ lotCount NOTIFY lotCountChanged)
    Q_PROPERTY(BrickLink::OrderStatus status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString paymentType READ paymentType NOTIFY paymentTypeChanged)
    Q_PROPERTY(QString trackingNumber READ trackingNumber NOTIFY trackingNumberChanged)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(QString phone READ phone NOTIFY phoneChanged)
    Q_PROPERTY(QString countryCode READ countryCode NOTIFY countryCodeChanged)

public:
    Order();
    Order(const QString &id, OrderType type);
    ~Order() override;

    LotList takeLots();
    const LotList &lots() const;
    QString id() const;
    OrderType type() const;
    QDate date() const;
    QDate lastUpdated() const;
    QString otherParty() const;
    double shipping() const;
    double insurance() const;
    double additionalCharges1() const;
    double additionalCharges2() const;
    double credit() const;
    double creditCoupon() const;
    double orderTotal() const;
    double usSalesTax() const;
    double grandTotal() const;
    double vatChargeSeller() const;
    double vatChargeBrickLink() const;
    QString currencyCode() const;
    QString paymentCurrencyCode() const;
    int itemCount() const;
    int lotCount() const;
    OrderStatus status() const;
    QString paymentType() const;
    QString trackingNumber() const;
    QString address() const;
    QString phone() const;
    QString countryCode() const;

    void setLots(LotList &&lots); // we take ownership
    void setId(const QString &id);
    void setType(OrderType type);
    void setDate(const QDate &dt);
    void setLastUpdated(const QDate &dt);
    void setOtherParty(const QString &str);
    void setShipping(double m);
    void setInsurance(double m);
    void setAdditionalCharges1(double m);
    void setAdditionalCharges2(double m);
    void setCredit(double m);
    void setCreditCoupon(double m);
    void setOrderTotal(double m);
    void setUsSalesTax(double m);
    void setGrandTotal(double m);
    void setVatChargeSeller(double m);
    void setVatChargeBrickLink(double m);
    void setCurrencyCode(const QString &str);
    void setPaymentCurrencyCode(const QString &str);
    void setItemCount(int i);
    void setLotCount(int i);
    void setStatus(OrderStatus status);
    void setPaymentType(const QString &str);
    void setTrackingNumber(const QString &str);
    void setAddress(const QString &str);
    void setPhone(const QString &str);
    void setCountryCode(const QString &str);

    static OrderStatus statusFromString(const QString &s);
    static QString statusToString(OrderStatus status, bool translated = true);

signals:
    void idChanged(const QString &id);
    void typeChanged(BrickLink::OrderType type);
    void dateChanged(const QDate &dt);
    void lastUpdatedChanged(const QDate &dt);
    void otherPartyChanged(const QString &str);

    void shippingChanged(double m);
    void insuranceChanged(double m);
    void additionalCharges1Changed(double m);
    void additionalCharges2Changed(double m);
    void creditChanged(double m);
    void creditCouponChanged(double m);
    void orderTotalChanged(double m);
    void usSalesTaxChanged(double m);
    void grandTotalChanged(double m);
    void vatChargeSellerChanged(double m);
    void vatChargeBrickLinkChanged(double m);
    void currencyCodeChanged(const QString &str);
    void paymentCurrencyCodeChanged(const QString &str);

    void itemCountChanged(int i);
    void lotCountChanged(int i);
    void statusChanged(BrickLink::OrderStatus status);
    void paymentTypeChanged(const QString &str);
    void trackingNumberChanged(const QString &str);
    void addressChanged(const QString &str);
    void phoneChanged(const QString &str);
    void countryCodeChanged(const QString &str);

private:
    QScopedPointer<OrderPrivate> d;
};



class Orders : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid NOTIFY updateFinished)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateFinished)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY updateFinished)
    Q_PROPERTY(QVector<Order *> orders READ orders NOTIFY updateFinished)

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
    };

    bool isValid() const          { return m_valid; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    BrickLink::UpdateStatus updateStatus() const  { return m_updateStatus; }

    Q_INVOKABLE void startUpdate(const QString &id);
    Q_INVOKABLE void startUpdate(int lastNDays);
    Q_INVOKABLE void startUpdate(const QDate &fromDate, const QDate &toDate);
    Q_INVOKABLE void cancelUpdate();

    //Q_INVOKABLE void trimDatabase(int keepLastNDays);

    QVector<Order *> orders() const;

    int indexOfOrder(const QString &orderId) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);

private:
    Orders(QObject *parent = nullptr);
    void reloadOrdersFromCache();
    static Order *orderFromXML(const QString &fileName);
    void startUpdateInternal(const QDate &fromDate, const QDate &toDate, const QString &orderId);
    void updateOrder(std::unique_ptr<Order> order);
    void appendOrderToModel(std::unique_ptr<Order> order);
    void emitDataChanged(int row, int col);
    void startUpdateAddress(Order *order);
    std::pair<QString, QString> parseAddressAndPhone(OrderType type, const QByteArray &data);
    QSaveFile *orderSaveFile(QStringView fileName, OrderType type, const QDate &date) const;

    bool m_valid = false;
    BrickLink::UpdateStatus m_updateStatus = BrickLink::UpdateStatus::UpdateFailed;
    QString m_userId;
    QVector<TransferJob *> m_jobs;
    QVector<TransferJob *> m_addressJobs;
    QMap<TransferJob *, QPair<int, int>> m_jobProgress;
    QMap<TransferJob *, QPair<bool, QString>> m_jobResult;
    QDateTime m_lastUpdated;
    QVector<Order *> m_orders;
    mutable QHash<QString, QIcon> m_flags;

    friend class Core;
};


} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Order *)
Q_DECLARE_METATYPE(const BrickLink::Order *)
