// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QMap>
#include <QDateTime>
#include <QVector>
#include <QIcon>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include "bricklink/global.h"

class TransferJob;

namespace BrickLink {

class OrderPrivate
{
public:
    QString   m_id;
    OrderType m_type;
    QString   m_otherParty;
    QDateTime m_date;
    QDateTime m_lastUpdate;
    double    m_shipping = 0;
    double    m_insurance = 0;
    double    m_addCharges1 = 0;
    double    m_addCharges2 = 0;
    double    m_credit = 0;
    double    m_creditCoupon = 0;
    double    m_orderTotal = 0;
    double    m_usSalesTax = 0;
    double    m_vatChargeBrickLink = 0;
    QString   m_currencyCode;
    double    m_grandTotal = 0;
    QString   m_paymentCurrencyCode;
    int       m_lotCount = 0;
    int       m_itemCount = 0;
    double    m_cost = 0;
    OrderStatus m_status = OrderStatus::Unknown;
    QString   m_paymentType;
    QString   m_remarks;
    QString   m_trackingNumber;
    QString   m_paymentStatus;
    QDateTime m_paymentLastUpdate;
    double    m_vatChargeSeller = 0;
    QString   m_countryCode;
    QString   m_address;
    QString   m_phone;
};

class OrdersPrivate
{
public:
    Core *m_core;
    BrickLink::UpdateStatus m_updateStatus = BrickLink::UpdateStatus::UpdateFailed;
    QString m_userId;
    QVector<TransferJob *> m_jobs;
    QVector<TransferJob *> m_addressJobs;
    QMap<TransferJob *, QPair<int, int>> m_jobProgress;
    QMap<TransferJob *, QPair<bool, QString>> m_jobResult;
    QDateTime m_lastUpdated;
    QVector<Order *> m_orders;
    mutable QHash<QString, QIcon> m_flags;

    QSqlDatabase m_db;
    QSqlQuery m_loadOrdersQuery;
    QSqlQuery m_loadXmlQuery;
    QSqlQuery m_importQuery;
    QSqlQuery m_saveAddressQuery;
    QSqlQuery m_saveOrderQuery;
    QSqlQuery m_saveUpdatedQuery;

    enum OrderDataFormat {
        Format_XML = 0,
        Format_CompressedXML,
    };
};

} // namespace BrickLink
