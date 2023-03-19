// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <memory>
#include <array>

#include <QBuffer>
#include <QSaveFile>
#include <QDirIterator>
#include <QCoreApplication>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QXmlStreamReader>
#include <QQmlEngine>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQueryModel>
#include <QtCore/QLoggingCategory>

#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/order.h"
#include "bricklink/order_p.h"

#include "common/currency.h"
#include "utility/exception.h"
#include "utility/stopwatch.h"
#include "utility/transfer.h"
#include "utility/utility.h"

Q_DECLARE_LOGGING_CATEGORY(LogCache)
Q_DECLARE_LOGGING_CATEGORY(LogSql)


namespace BrickLink {


/*! \qmltype Order
    \inqmlmodule BrickLink
    \ingroup qml-api
    \brief This type holds the information about a BrickLink order.
*/
/*! \qmlproperty string Order::id
    \readonly
    The order id.
*/
/*! \qmlproperty enumeration Order::type
    \readonly
    The type of order. The possible values are:
    \value BrickLink.OrderType.Received  A received order.
    \value BrickLink.OrderType.Placed    A placed order.
*/
/*! \qmlproperty date Order::date
    \readonly
    The date when the order was placed.
*/
/*! \qmlproperty date Order::lastUpdated
    \readonly
    The date when the order was last updated.
*/
/*! \qmlproperty string Order::otherParty
    \readonly
    The user-name of the other party (either buyer or seller, depending on the type).
*/
/*! \qmlproperty real Order::shipping
    \readonly
*/
/*! \qmlproperty real Order::insurance
    \readonly
*/
/*! \qmlproperty real Order::additionalCharges1
    \readonly
*/
/*! \qmlproperty real Order::additionalCharges2
    \readonly
*/
/*! \qmlproperty real Order::credit
    \readonly
*/
/*! \qmlproperty real Order::creditCoupon
    \readonly
*/
/*! \qmlproperty real Order::orderTotal
    \readonly
*/
/*! \qmlproperty real Order::usSalesTax
    \readonly
*/
/*! \qmlproperty real Order::grandTotal
    \readonly
*/
/*! \qmlproperty real Order::vatChargeSeller
    \readonly
*/
/*! \qmlproperty real Order::vatChargeBrickLink
    \readonly
*/
/*! \qmlproperty string Order::currencyCode
    \readonly
*/
/*! \qmlproperty string Order::paymentCurrencyCode
    \readonly
*/
/*! \qmlproperty int Order::itemCount
    \readonly
    The number of items in this order.
*/
/*! \qmlproperty int Order::lotCount
    \readonly
    The number of lots in this order.
*/
/*! \qmlproperty enumeration Order::status
    \readonly
    The status of the order. Can be one of:
    \value BrickLink.OrderStatus.Unknown
    \value BrickLink.OrderStatus.Pending
    \value BrickLink.OrderStatus.Updated
    \value BrickLink.OrderStatus.Processing
    \value BrickLink.OrderStatus.Ready
    \value BrickLink.OrderStatus.Paid
    \value BrickLink.OrderStatus.Packed
    \value BrickLink.OrderStatus.Shipped
    \value BrickLink.OrderStatus.Received
    \value BrickLink.OrderStatus.Completed
    \value BrickLink.OrderStatus.OCR
    \value BrickLink.OrderStatus.NPB
    \value BrickLink.OrderStatus.NPX
    \value BrickLink.OrderStatus.NRS
    \value BrickLink.OrderStatus.NSS
    \value BrickLink.OrderStatus.Cancelled
*/
/*! \qmlproperty string Order::paymentType
    \readonly
*/
/*! \qmlproperty string Order::trackingNumber
    \readonly
*/
/*! \qmlproperty string Order::address
    \readonly
    The full address of the otherParty.
*/
/*! \qmlproperty string Order::phone
    \readonly
    The phone number of the otherParty.
*/
/*! \qmlproperty string Order::countryCode
    \readonly
    The 2-letter country code of the otherParty.
*/

Order::Order(const QString &id, OrderType type)
    : QObject()
    , d(new OrderPrivate)
{
    d->m_id = id;
    d->m_type = type;
}

Order::~Order()
{ /* needed to use std::unique_ptr on d */ }

LotList Order::loadLots() const
{
    if (auto orders = qobject_cast<Orders *>(parent())) {
        try {
            return orders->loadOrderLots(this);
        } catch (const Exception &e) {
            qWarning() << "Order::loadLots() failed:" << e.errorString();
            return { };
        }
    } else {
        qWarning() << "Order::loadLots() is not possible: wrong parent";
        return { };
    }
}

Order::Order()
    : Order({ }, OrderType::Received)
{ }


QString Order::id() const
{
    return d->m_id;
}

OrderType Order::type() const
{
    return d->m_type;
}

QDate Order::date() const
{
    return d->m_date.date();
}

QDate Order::lastUpdated() const
{
    return d->m_lastUpdate.date();
}

QString Order::otherParty() const
{
    return d->m_otherParty;
}

double Order::shipping() const
{
    return d->m_shipping;
}

double Order::insurance() const
{
    return d->m_insurance;
}

double Order::additionalCharges1() const
{
    return d->m_addCharges1;
}

double Order::additionalCharges2() const
{
    return d->m_addCharges2;
}

double Order::credit() const
{
    return d->m_credit;
}

double Order::creditCoupon() const
{
    return d->m_creditCoupon;
}

double Order::orderTotal() const
{
    return d->m_orderTotal;
}

double Order::usSalesTax() const
{
    return d->m_usSalesTax;
}

double Order::grandTotal() const
{
    return d->m_grandTotal;
}

double Order::vatChargeSeller() const
{
    return d->m_vatChargeSeller;
}

double Order::vatChargeBrickLink() const
{
    return d->m_vatChargeBrickLink;
}

QString Order::currencyCode() const
{
    return d->m_currencyCode;
}

QString Order::paymentCurrencyCode() const
{
    return d->m_paymentCurrencyCode;
}

int Order::lotCount() const
{
    return d->m_lotCount;
}

int Order::itemCount() const
{
    return d->m_itemCount;
}

double Order::cost() const
{
    return d->m_cost;
}

OrderStatus Order::status() const
{
    return d->m_status;
}

QString Order::paymentType() const
{
    return d->m_paymentType;
}

QString Order::remarks() const
{
    return d->m_remarks;
}

QString Order::trackingNumber() const
{
    return d->m_trackingNumber;
}

QString Order::paymentStatus() const
{
    return d->m_paymentStatus;
}

QDate Order::paymentLastUpdated() const
{
    return d->m_paymentLastUpdate.date();
}

QString Order::address() const
{
    return d->m_address;
}

QString Order::phone() const
{
    return d->m_phone;
}

QString Order::countryCode() const
{
    return d->m_countryCode;
}

void Order::setId(const QString &id)
{
    if (d->m_id != id) {
        d->m_id = id;
        emit idChanged(id);
    }
}

void Order::setType(OrderType type)
{
    if (d->m_type != type) {
        d->m_type = type;
        emit typeChanged(type);
    }
}

void Order::setDate(const QDate &dt)
{
    if (d->m_date.date() != dt) {
        d->m_date.setDate(dt);
        emit dateChanged(dt);
    }
}

void Order::setLastUpdated(const QDate &dt)
{
    if (d->m_lastUpdate.date() != dt) {
        d->m_lastUpdate.setDate(dt);
        emit lastUpdatedChanged(dt);
    }
}

void Order::setOtherParty(const QString &str)
{
    if (d->m_otherParty != str) {
        d->m_otherParty = str;
        emit otherPartyChanged(str);
    }
}

void Order::setShipping(double m)
{
    if (!qFuzzyCompare(d->m_shipping, m)) {
        d->m_shipping = m;
        emit shippingChanged(m);
    }
}

void Order::setInsurance(double m)
{
    if (!qFuzzyCompare(d->m_insurance, m)) {
        d->m_insurance = m;
        emit insuranceChanged(m);
    }
}

void Order::setAdditionalCharges1(double m)
{
    if (!qFuzzyCompare(d->m_addCharges1, m)) {
        d->m_addCharges1 = m;
        emit additionalCharges1Changed(m);
    }
}

void Order::setAdditionalCharges2(double m)
{
    if (!qFuzzyCompare(d->m_addCharges2, m)) {
        d->m_addCharges2 = m;
        emit additionalCharges2Changed(m);
    }
}

void Order::setCredit(double m)
{
    if (!qFuzzyCompare(d->m_credit, m)) {
        d->m_credit = m;
        emit creditChanged(m);
    }
}

void Order::setCreditCoupon(double m)
{
    if (!qFuzzyCompare(d->m_creditCoupon, m)) {
        d->m_creditCoupon = m;
        emit creditCouponChanged(m);
    }
}

void Order::setOrderTotal(double m)
{
    if (!qFuzzyCompare(d->m_orderTotal, m)) {
        d->m_orderTotal = m;
        emit orderTotalChanged(m);
    }
}

void Order::setUsSalesTax(double m)
{
    if (!qFuzzyCompare(d->m_usSalesTax, m)) {
        d->m_usSalesTax = m;
        emit usSalesTaxChanged(m);
    }
}

void Order::setGrandTotal(double m)
{
    if (!qFuzzyCompare(d->m_grandTotal, m)) {
        d->m_grandTotal = m;
        emit grandTotalChanged(m);
    }
}

void Order::setVatChargeSeller(double m)
{
    if (!qFuzzyCompare(d->m_vatChargeSeller, m)) {
        d->m_vatChargeSeller = m;
        emit vatChargeSellerChanged(m);
    }
}

void Order::setVatChargeBrickLink(double m)
{
    if (!qFuzzyCompare(d->m_vatChargeBrickLink, m)) {
        d->m_vatChargeBrickLink = m;
        emit vatChargeBrickLinkChanged(m);
    }
}

void Order::setCurrencyCode(const QString &str)
{
    if (d->m_currencyCode != str) {
        d->m_currencyCode = str;
        emit currencyCodeChanged(str);
    }
}

void Order::setPaymentCurrencyCode(const QString &str)
{
    if (d->m_paymentCurrencyCode != str) {
        d->m_paymentCurrencyCode = str;
        emit paymentCurrencyCodeChanged(str);
    }
}

void Order::setItemCount(int i)
{
    if (d->m_itemCount != i) {
        d->m_itemCount = i;
        emit itemCountChanged(i);
    }
}

void Order::setCost(double c)
{
    if (!qFuzzyCompare(d->m_cost, c)) {
        d->m_cost = c;
        emit costChanged(c);
    }
}

void Order::setLotCount(int i)
{
    if (d->m_lotCount != i) {
        d->m_lotCount = i;
        emit lotCountChanged(i);
    }
}

void Order::setStatus(OrderStatus status)
{
    if (d->m_status != status) {
        d->m_status = status;
        emit statusChanged(status);
    }
}

void Order::setPaymentType(const QString &str)
{
    if (d->m_paymentType != str) {
        d->m_paymentType = str;
        emit paymentTypeChanged(str);
    }
}

void Order::setRemarks(const QString &str)
{
    if (d->m_remarks != str) {
        d->m_remarks = str;
        emit remarksChanged(str);
    }
}

void Order::setTrackingNumber(const QString &str)
{
    if (d->m_trackingNumber != str) {
        d->m_trackingNumber = str;
        emit trackingNumberChanged(str);
    }
}

void Order::setPaymentStatus(const QString &str)
{
    if (d->m_paymentStatus != str) {
        d->m_paymentStatus = str;
        emit paymentStatusChanged(str);
    }
}

void Order::setPaymentLastUpdated(const QDate &dt)
{
    if (d->m_paymentLastUpdate.date() != dt) {
        d->m_paymentLastUpdate.setDate(dt);
        emit paymentLastUpdatedChanged(dt);
    }
}

void Order::setAddress(const QString &str)
{
    if (d->m_address != str) {
        d->m_address = str;
        emit addressChanged(str);
    }
}

void Order::setPhone(const QString &str)
{
    if (d->m_phone != str) {
        d->m_phone = str;
        emit phoneChanged(str);
    }
}

void Order::setCountryCode(const QString &str)
{
    if (d->m_countryCode != str) {
        d->m_countryCode = str;
        emit countryCodeChanged(str);
    }
}




static const std::pair<OrderStatus, const char *> orderStatus[] = {
    { OrderStatus::Unknown,    QT_TRANSLATE_NOOP("Orders", "Unknown")    },
    { OrderStatus::Pending,    QT_TRANSLATE_NOOP("Orders", "Pending")    },
    { OrderStatus::Updated,    QT_TRANSLATE_NOOP("Orders", "Updated")    },
    { OrderStatus::Processing, QT_TRANSLATE_NOOP("Orders", "Processing") },
    { OrderStatus::Ready,      QT_TRANSLATE_NOOP("Orders", "Ready")      },
    { OrderStatus::Paid,       QT_TRANSLATE_NOOP("Orders", "Paid")       },
    { OrderStatus::Packed,     QT_TRANSLATE_NOOP("Orders", "Packed")     },
    { OrderStatus::Shipped,    QT_TRANSLATE_NOOP("Orders", "Shipped")    },
    { OrderStatus::Received,   QT_TRANSLATE_NOOP("Orders", "Received")   },
    { OrderStatus::Completed,  QT_TRANSLATE_NOOP("Orders", "Completed")  },
    { OrderStatus::OCR,        QT_TRANSLATE_NOOP("Orders", "OCR")        },
    { OrderStatus::NPB,        QT_TRANSLATE_NOOP("Orders", "NPB")        },
    { OrderStatus::NPX,        QT_TRANSLATE_NOOP("Orders", "NPX")        },
    { OrderStatus::NRS,        QT_TRANSLATE_NOOP("Orders", "NRS")        },
    { OrderStatus::NSS,        QT_TRANSLATE_NOOP("Orders", "NSS")        },
    { OrderStatus::Cancelled,  QT_TRANSLATE_NOOP("Orders", "Cancelled")  },
};

OrderStatus Order::statusFromString(const QString &s)
{
    for (const auto &os : orderStatus) {
        if (s == QLatin1String(os.second))
            return os.first;
    }
    return OrderStatus::Unknown;
}

QString Order::statusAsString(bool translated) const
{
    for (const auto &os : orderStatus) {
        if (d->m_status == os.first) {
            return translated ? QCoreApplication::translate("Orders", os.second)
                              : QString::fromLatin1(os.second);
        }
    }
    return { };
}


Orders::Orders(Core *core)
    : QAbstractTableModel(core)
    , d(new OrdersPrivate)
{
    d->m_core = core;

    connect(core, &Core::userIdChanged,
            this, &Orders::reloadOrdersFromDatabase);
    reloadOrdersFromDatabase(core->userId());

    connect(core, &Core::authenticatedTransferStarted,
            this, [this](TransferJob *job) {
                if ((d->m_updateStatus == UpdateStatus::Updating) && !d->m_jobs.isEmpty()
                    && (d->m_jobs.constFirst() == job)) {
            emit updateStarted();
        }
    });
    connect(core, &Core::authenticatedTransferProgress,
            this, [this](TransferJob *job, int progress, int total) {
        if ((d->m_updateStatus == UpdateStatus::Updating) && d->m_jobs.contains(job)) {
            d->m_jobProgress[job] = qMakePair(progress, total);

            int overallProgress = 0, overallTotal = 0;
            for (auto pair : std::as_const(d->m_jobProgress)) {
                overallProgress += pair.first;
                overallTotal += pair.second;
            }
            emit updateProgress(overallProgress, overallTotal);
        }
    });
    connect(core, &Core::authenticatedTransferFinished,
            this, [this](TransferJob *job) {
        bool jobCompleted = job->isCompleted() && (job->responseCode() == 200) && job->data();
        QByteArray type = job->userTag();

        if (d->m_addressJobs.contains(job) && (type == "address")) {
            d->m_addressJobs.removeOne(job);

            try {
                if (!jobCompleted)
                    throw Exception(job->errorString());

                int row = indexOfOrder(job->userData(type).toString());
                if (row >= 0) {
                    Order *order = d->m_orders.at(row);
                    auto [address, phone] = parseAddressAndPhone(order->type(), *job->data());
                    if (address.isEmpty())
                        address = tr("Address not available");

                    order->setAddress(address);
                    order->setPhone(phone);

                    d->m_saveAddressQuery.bindValue(u":id"_qs, order->id());
                    d->m_saveAddressQuery.bindValue(u":address"_qs, address);
                    d->m_saveAddressQuery.bindValue(u":phone"_qs, phone);

                    auto finishGuard = qScopeGuard([this]() { d->m_saveAddressQuery.finish(); });

                    if (!d->m_saveAddressQuery.exec())
                        throw Exception("Failed to save address to database: %1").arg(d->m_saveAddressQuery.lastError().text());
                }
            } catch (const Exception &e) {
                qWarning() << "Failed to retrieve address for order"
                           << job->userData(type).toString() << ":" << e.errorString();
            }
        } else if ((d->m_updateStatus == UpdateStatus::Updating) && d->m_jobs.contains(job)) {
            bool success = true;
            QString message;

            if (jobCompleted) { // if there are no matching orders, we get an error reply back...
                QHash<Order *, QString> orders;

                d->m_db.transaction();

                try {
                    orders = parseOrdersXML(*job->data());

                    for (auto it = orders.cbegin(); it != orders.cend(); ++it) {
                        Order *order = it.key();
                        QByteArray orderXml = it.value().toUtf8();

                        if (order->id().isEmpty() || !order->date().isValid())
                            throw Exception("Invalid order without ID and DATE");

                        d->m_saveOrderQuery.bindValue(u":id"_qs, order->id());
                        d->m_saveOrderQuery.bindValue(u":type"_qs, int(order->type()));
                        d->m_saveOrderQuery.bindValue(u":otherParty"_qs, order->otherParty());
                        d->m_saveOrderQuery.bindValue(u":date"_qs, order->date().toJulianDay());
                        d->m_saveOrderQuery.bindValue(u":lastUpdated"_qs, order->lastUpdated().toJulianDay());
                        d->m_saveOrderQuery.bindValue(u":shipping"_qs, order->shipping());
                        d->m_saveOrderQuery.bindValue(u":insurance"_qs, order->insurance());
                        d->m_saveOrderQuery.bindValue(u":additionalCharges1"_qs, order->additionalCharges1());
                        d->m_saveOrderQuery.bindValue(u":additionalCharges2"_qs, order->additionalCharges2());
                        d->m_saveOrderQuery.bindValue(u":credit"_qs, order->credit());
                        d->m_saveOrderQuery.bindValue(u":creditCoupon"_qs, order->creditCoupon());
                        d->m_saveOrderQuery.bindValue(u":orderTotal"_qs, order->orderTotal());
                        d->m_saveOrderQuery.bindValue(u":usSalesTax"_qs, order->usSalesTax());
                        d->m_saveOrderQuery.bindValue(u":vatChargeBrickLink"_qs, order->vatChargeBrickLink());
                        d->m_saveOrderQuery.bindValue(u":currencyCode"_qs, order->currencyCode());
                        d->m_saveOrderQuery.bindValue(u":grandTotal"_qs, order->grandTotal());
                        d->m_saveOrderQuery.bindValue(u":paymentCurrencyCode"_qs, order->paymentCurrencyCode());
                        d->m_saveOrderQuery.bindValue(u":lotCount"_qs, order->lotCount());
                        d->m_saveOrderQuery.bindValue(u":itemCount"_qs, order->itemCount());
                        d->m_saveOrderQuery.bindValue(u":cost"_qs, order->cost());
                        d->m_saveOrderQuery.bindValue(u":status"_qs, int(order->status()));
                        d->m_saveOrderQuery.bindValue(u":paymentType"_qs, order->paymentType());
                        d->m_saveOrderQuery.bindValue(u":remarks"_qs, order->remarks());
                        d->m_saveOrderQuery.bindValue(u":trackingNumber"_qs, order->trackingNumber());
                        d->m_saveOrderQuery.bindValue(u":paymentStatus"_qs, order->paymentStatus());
                        d->m_saveOrderQuery.bindValue(u":paymentLastUpdated"_qs, order->paymentLastUpdated().toJulianDay());
                        d->m_saveOrderQuery.bindValue(u":vatChargeSeller"_qs, order->vatChargeSeller());
                        d->m_saveOrderQuery.bindValue(u":countryCode"_qs, order->countryCode());
                        d->m_saveOrderQuery.bindValue(u":orderDataFormat"_qs, int(OrdersPrivate::Format_CompressedXML));
                        d->m_saveOrderQuery.bindValue(u":orderData"_qs, qCompress(orderXml));

                        auto finishGuard = qScopeGuard([this]() { d->m_saveOrderQuery.finish(); });

                        if (!d->m_saveOrderQuery.exec())
                            throw Exception("Failed to save order to database: %1").arg(d->m_saveOrderQuery.lastError().text());
                    }

                    while (!orders.isEmpty()) {
                        auto order = std::unique_ptr<Order> { *orders.keyBegin() };
                        orders.remove(order.get());
                        updateOrder(std::move(order));
                    }
                } catch (const Exception &e) {
                    success = false;
                    message = tr("Could not parse the received order XML data") + u": " + e.errorString();
                }
                d->m_db.commit();

                qDeleteAll(orders.keyBegin(), orders.keyEnd());
            }
            d->m_jobResult[job] = qMakePair(success, message);
            d->m_jobs.removeOne(job);

            if (d->m_jobs.isEmpty()) {
                bool overallSuccess = true;
                QString overallMessage;
                for (const auto &pair : std::as_const(d->m_jobResult)) {
                    overallSuccess = overallSuccess && pair.first;
                    if (overallMessage.isEmpty()) // only show the first error message
                        overallMessage = pair.second;
                }
                setUpdateStatus(overallSuccess ? UpdateStatus::Ok : UpdateStatus::UpdateFailed);

                if (overallSuccess) {
                    d->m_lastUpdated = QDateTime::currentDateTime();
                    d->m_saveUpdatedQuery.bindValue(u":updated"_qs, d->m_lastUpdated.toMSecsSinceEpoch());

                    if (!d->m_saveUpdatedQuery.exec()) {
                        qCWarning(LogSql) << "Failed to write 'updated' field to order database:"
                                          << d->m_saveOrderQuery.lastError().text();
                    }
                    d->m_saveUpdatedQuery.finish();
                }

                d->m_jobProgress.clear();
                d->m_jobResult.clear();

                emit updateFinished(overallSuccess, overallMessage);
            }
        }
    });
}

void Orders::reloadOrdersFromDatabase(const QString &userId)
{
    if (userId == d->m_userId)
        return;

    beginResetModel();
    qDeleteAll(d->m_orders);
    d->m_orders.clear();
    d->m_lastUpdated = { };
    endResetModel();

    emit countChanged(0);

    d->m_userId = userId;

    if (d->m_db.isOpen())
        d->m_db.close();

    if (userId.isEmpty())
        return;

    QString dbName = d->m_core->dataPath() + u"order_cache_%1.sqlite"_qs.arg(userId);
    d->m_db = QSqlDatabase::addDatabase(u"QSQLITE"_qs, u"OrderCache"_qs);
    d->m_db.setDatabaseName(dbName);

    // try to start from scratch, if the DB open fails
    if (!d->m_db.open() &&
        !(QFile::exists(dbName) && QFile::remove(dbName) && d->m_db.open())) {
        qCWarning(LogSql) << "Failed to open the orders database:" << d->m_db.lastError().text();
    }

    // MSVC2022 has a size limit on the literal operator"" _qs, so we have to use QStringLiteral

    if (d->m_db.isOpen()) {
        QSqlQuery createStatusQuery(d->m_db);
        if (!createStatusQuery.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS status ("
                "id INTEGER NOT NULL PRIMARY KEY, "  // 0
                "updated INTEGER "                   // msecsSinceEpoch
                ") WITHOUT ROWID;"))) {
            qCWarning(LogSql) << "Failed to create the 'status' table in the orders database:"
                              << createStatusQuery.lastError().text();
            d->m_db.close();
        }

        QSqlQuery createOrdersQuery(d->m_db);
        if (!createOrdersQuery.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS orders ("
                "id TEXT NOT NULL PRIMARY KEY,"
                "type INTEGER NOT NULL,"          // OrderType
                "otherParty TEXT NOT NULL,"
                "date INTEGER NOT NULL,"          // Julian day
                "lastUpdated INTEGER NOT NULL,"   // Julian day
                "shipping REAL,"
                "insurance REAL,"
                "additionalCharges1 REAL,"
                "additionalCharges2 REAL,"
                "credit REAL,"
                "creditCoupon REAL,"
                "orderTotal REAL,"
                "usSalesTax REAL,"
                "vatChargeBrickLink REAL,"
                "currencyCode TEXT,"
                "grandTotal REAL,"
                "paymentCurrencyCode TEXT,"
                "lotCount INTEGER NOT NULL,"
                "itemCount INTEGER NOT NULL,"
                "cost REAL,"
                "status INTEGER NOT NULL,"      // OrderStatus
                "paymentType TEXT,"
                "remarks TEXT,"
                "trackingNumber TEXT,"
                "paymentStatus TEXT,"
                "paymentLastUpdated INTEGER,"   // Julian day
                "vatChargeSeller REAL,"
                "countryCode TEXT,"
                "address TEXT,"
                "phone TEXT,"
                "orderDataFormat INTEGER," // enum OrderDataFormat
                "orderData BLOB NOT NULL"
                ") WITHOUT ROWID;"))) {
            qCWarning(LogSql) << "Failed to create the 'orders' table in the orders database:"
                              << createOrdersQuery.lastError().text();
            d->m_db.close();
        }

        d->m_importQuery = QSqlQuery(d->m_db);
        if (!d->m_importQuery.prepare(QStringLiteral(
                "INSERT INTO orders(id,type,otherParty,date,lastUpdated,shipping,insurance,additionalCharges1,additionalCharges2,credit,creditCoupon,orderTotal,usSalesTax,vatChargeBrickLink,currencyCode,grandTotal,paymentCurrencyCode,lotCount,itemCount,cost,status,paymentType,remarks,trackingNumber,paymentStatus,paymentLastUpdated,vatChargeSeller,countryCode,address,phone,orderDataFormat,orderData)"
                " VALUES(:id,:type,:otherParty,:date,:lastUpdated,:shipping,:insurance,:additionalCharges1,:additionalCharges2,:credit,:creditCoupon,:orderTotal,:usSalesTax,:vatChargeBrickLink,:currencyCode,:grandTotal,:paymentCurrencyCode,:lotCount,:itemCount,:cost,:status,:paymentType,:remarks,:trackingNumber,:paymentStatus,:paymentLastUpdated,:vatChargeSeller,:countryCode,:address,:phone,:orderDataFormat,:orderData)"
                " ON CONFLICT(id) DO NOTHING;"))) {
            qCWarning(LogSql) << "Failed to prepare import query for the orders database:" << d->m_importQuery.lastError().text();
        }
        d->m_loadOrdersQuery = QSqlQuery(d->m_db);
        if (!d->m_loadOrdersQuery.prepare(QStringLiteral(
                "SELECT id,type,otherParty,date,lastUpdated,shipping,insurance,additionalCharges1,additionalCharges2,credit,creditCoupon,orderTotal,usSalesTax,vatChargeBrickLink,currencyCode,grandTotal,paymentCurrencyCode,lotCount,itemCount,cost,status,paymentType,remarks,trackingNumber,paymentStatus,paymentLastUpdated,vatChargeSeller,countryCode,address,phone"
                " FROM orders;"))) {
            qCWarning(LogSql) << "Failed to prepare load orders query for the orders database:" << d->m_loadOrdersQuery.lastError().text();
        }

        d->m_loadXmlQuery = QSqlQuery(d->m_db);
        if (!d->m_loadXmlQuery.prepare(QStringLiteral(
                "SELECT orderDataFormat,orderData"
                " FROM orders WHERE id=:id;"))) {
            qCWarning(LogSql) << "Failed to prepare load xml query for the orders database:" << d->m_loadXmlQuery.lastError().text();
        }

        d->m_saveAddressQuery = QSqlQuery(d->m_db);
        if (!d->m_saveAddressQuery.prepare(QStringLiteral(
                "UPDATE orders SET address=:address,phone=:phone WHERE id=:id;"))) {
            qCWarning(LogSql) << "Failed to prepare save address query for the orders database:" << d->m_saveAddressQuery.lastError().text();
        }
        d->m_saveOrderQuery = QSqlQuery(d->m_db);
        if (!d->m_saveOrderQuery.prepare(QStringLiteral(
                "INSERT INTO orders(id,type,otherParty,date,lastUpdated,shipping,insurance,additionalCharges1,additionalCharges2,credit,creditCoupon,orderTotal,usSalesTax,vatChargeBrickLink,currencyCode,grandTotal,paymentCurrencyCode,lotCount,itemCount,cost,status,paymentType,remarks,trackingNumber,paymentStatus,paymentLastUpdated,vatChargeSeller,countryCode,orderDataFormat,orderData)"
                " VALUES(:id,:type,:otherParty,:date,:lastUpdated,:shipping,:insurance,:additionalCharges1,:additionalCharges2,:credit,:creditCoupon,:orderTotal,:usSalesTax,:vatChargeBrickLink,:currencyCode,:grandTotal,:paymentCurrencyCode,:lotCount,:itemCount,:cost,:status,:paymentType,:remarks,:trackingNumber,:paymentStatus,:paymentLastUpdated,:vatChargeSeller,:countryCode,:orderDataFormat,:orderData)"
                " ON CONFLICT(id) DO UPDATE"
                " SET type=excluded.type,otherParty=excluded.otherParty,date=excluded.date,lastUpdated=excluded.lastUpdated,shipping=excluded.shipping,insurance=excluded.insurance,additionalCharges1=excluded.additionalCharges1,additionalCharges2=excluded.additionalCharges2,credit=excluded.credit,creditCoupon=excluded.creditCoupon,orderTotal=excluded.orderTotal,usSalesTax=excluded.usSalesTax,vatChargeBrickLink=excluded.vatChargeBrickLink,currencyCode=excluded.currencyCode,grandTotal=excluded.grandTotal,paymentCurrencyCode=excluded.paymentCurrencyCode,lotCount=excluded.lotCount,itemCount=excluded.itemCount,cost=excluded.cost,status=excluded.status,paymentType=excluded.paymentType,remarks=excluded.remarks,trackingNumber=excluded.trackingNumber,paymentStatus=excluded.paymentStatus,paymentLastUpdated=excluded.paymentLastUpdated,vatChargeSeller=excluded.vatChargeSeller,countryCode=excluded.countryCode,orderDataFormat=excluded.orderDataFormat,orderData=excluded.orderData;"))) {
            qCWarning(LogSql) << "Failed to prepare save order query for the orders database:" << d->m_saveOrderQuery.lastError().text();
        }

        d->m_saveUpdatedQuery = QSqlQuery(d->m_db);
        if (!d->m_saveUpdatedQuery.prepare(QStringLiteral(
                "INSERT INTO status (id,updated) VALUES(0,:updated)"
                " ON CONFLICT(id) DO UPDATE"
                " SET updated=excluded.updated;"))) {
            qCWarning(LogSql) << "Failed to prepare save update query for the orders database:" << d->m_saveUpdatedQuery.lastError().text();
        }
    }

    QDateTime lastUpdated;

    if (d->m_db.isOpen()) {
        static constexpr int DBVersion = 1;

        {
            QSqlQuery jnlQuery(u"PRAGMA journal_mode = wal;"_qs, d->m_db);
            if (jnlQuery.lastError().isValid())
                qCWarning(LogSql) << "Failed to set journaling mode to 'wal' on the orders database:"
                                  << jnlQuery.lastError();
        }
        {
            QSqlQuery uvQuery(u"PRAGMA user_version;"_qs, d->m_db);
            uvQuery.next();
            auto userVersion = uvQuery.value(0).toInt();
            if (userVersion == 0) // brand new file, bump version
                QSqlQuery(u"PRAGMA user_version=%1;"_qs.arg(DBVersion), d->m_db);
        }
        {
            QSqlQuery updatedQuery(u"SELECT updated FROM status WHERE id=0;"_qs, d->m_db);
            if (updatedQuery.next())
                lastUpdated = QDateTime::fromMSecsSinceEpoch(updatedQuery.value(0).toLongLong());
        }

        // DB schema upgrade code goes here...
    }

    setUpdateStatus(lastUpdated.isValid() ? UpdateStatus::Ok : UpdateStatus::UpdateFailed);
    setLastUpdated(lastUpdated);

    importOldCache(userId);

    if (d->m_loadOrdersQuery.exec()) {
        stopwatch sw("Loading orders");

        auto readValue = [this](const QString &field) {
            QVariant v = d->m_loadOrdersQuery.value(field);
            if (!v.isValid()) {
                qCWarning(LogSql) << "Loading order field" << field << "failed:"
                                  << d->m_loadOrdersQuery.lastError().text();
            }
            return v;
        };

        while (d->m_loadOrdersQuery.next()) {
            auto order = std::make_unique<Order>();
            order->d->m_id = readValue(u"id"_qs).toString();
            order->d->m_type = OrderType(readValue(u"type"_qs).toInt());
            order->d->m_otherParty = readValue(u"otherParty"_qs).toString();
            order->d->m_date = QDate::fromJulianDay(readValue(u"date"_qs).toLongLong()).startOfDay();
            order->d->m_lastUpdate = QDate::fromJulianDay(readValue(u"lastUpdated"_qs).toLongLong()).startOfDay();
            order->d->m_shipping = readValue(u"shipping"_qs).toDouble();
            order->d->m_insurance = readValue(u"insurance"_qs).toDouble();
            order->d->m_addCharges1 = readValue(u"additionalCharges1"_qs).toDouble();
            order->d->m_addCharges2 = readValue(u"additionalCharges2"_qs).toDouble();
            order->d->m_credit = readValue(u"credit"_qs).toDouble();
            order->d->m_creditCoupon = readValue(u"creditCoupon"_qs).toDouble();
            order->d->m_orderTotal = readValue(u"orderTotal"_qs).toDouble();
            order->d->m_usSalesTax = readValue(u"usSalesTax"_qs).toDouble();
            order->d->m_vatChargeBrickLink = readValue(u"vatChargeBrickLink"_qs).toDouble();
            order->d->m_currencyCode = readValue(u"currencyCode"_qs).toString();
            order->d->m_grandTotal = readValue(u"grandTotal"_qs).toDouble();
            order->d->m_paymentCurrencyCode = readValue(u"paymentCurrencyCode"_qs).toString();
            order->d->m_lotCount = readValue(u"lotCount"_qs).toInt();
            order->d->m_itemCount = readValue(u"itemCount"_qs).toInt();
            order->d->m_cost = readValue(u"cost"_qs).toDouble();
            order->d->m_status = OrderStatus(readValue(u"status"_qs).toInt());
            order->d->m_paymentType = readValue(u"paymentType"_qs).toString();
            order->d->m_remarks = readValue(u"remarks"_qs).toString();
            order->d->m_trackingNumber = readValue(u"trackingNumber"_qs).toString();
            order->d->m_paymentStatus = readValue(u"paymentStatus"_qs).toString();
            order->d->m_paymentLastUpdate = QDate::fromJulianDay(readValue(u"paymentLastUpdated"_qs).toLongLong()).startOfDay();
            order->d->m_vatChargeSeller = readValue(u"vatChargeSeller"_qs).toDouble();
            order->d->m_countryCode = readValue(u"countryCode"_qs).toString();
            order->d->m_address = readValue(u"address"_qs).toString();
            order->d->m_phone = readValue(u"phone"_qs).toString();

            appendOrderToModel(std::move(order));
        }
    } else {
        qCWarning(LogSql) << "Failed to read orders from database:" << d->m_loadOrdersQuery.lastError().text();
    }
    d->m_loadOrdersQuery.finish();
}

void Orders::importOldCache(const QString &userId)
{
    // import the old file-system based cache
    if (userId.isEmpty() || !d->m_db.isOpen())
        return;

    QString path = d->m_core->dataPath() + u"orders/" + userId;
    QFileInfo imported(path + u"/.imported");
    if (imported.exists())
        return;

    stopwatch sw("Importing orders from old cache");

    d->m_db.transaction();

    QDirIterator dit(path, { u"*.order.xml"_qs },
                     QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        try {
            dit.next();
            QFile f(dit.filePath());
            if (!f.open(QIODevice::ReadOnly))
                throw Exception(&f, "Failed to open order XML");
            auto orders = Orders::parseOrdersXML(f.readAll());
            if (orders.size() != 1)
                throw Exception("Order XML does not contain exactly one order: %1").arg(f.fileName());
            std::unique_ptr<Order> order { orders.cbegin().key() };
            const QString &xml = orders.cbegin().value();
            order->moveToThread(this->thread());

            QDir dir = dit.fileInfo().absoluteDir();
            QString addressFileName = order->id() + u".brickstore.json";
            if (dir.exists(addressFileName)) {
                QFile fa(dir.absoluteFilePath(addressFileName));
                if (fa.open(QIODevice::ReadOnly) && (fa.size() < 5000)) {
                    auto json = QJsonDocument::fromJson(fa.readAll());
                    if (json.isObject()) {
                        order->setAddress(json[u"address"].toString());
                        order->setPhone(json[u"phone"].toString());
                    }
                }
            }

            d->m_importQuery.bindValue(u":id"_qs, order->id());
            d->m_importQuery.bindValue(u":type"_qs, int(order->type()));
            d->m_importQuery.bindValue(u":otherParty"_qs, order->otherParty());
            d->m_importQuery.bindValue(u":date"_qs, order->date().toJulianDay());
            d->m_importQuery.bindValue(u":lastUpdated"_qs, order->lastUpdated().toJulianDay());
            d->m_importQuery.bindValue(u":shipping"_qs, order->shipping());
            d->m_importQuery.bindValue(u":insurance"_qs, order->insurance());
            d->m_importQuery.bindValue(u":addCharges1"_qs, order->additionalCharges1());
            d->m_importQuery.bindValue(u":addCharges2"_qs, order->additionalCharges2());
            d->m_importQuery.bindValue(u":credit"_qs, order->credit());
            d->m_importQuery.bindValue(u":creditCoupon"_qs, order->creditCoupon());
            d->m_importQuery.bindValue(u":orderTotal"_qs, order->orderTotal());
            d->m_importQuery.bindValue(u":usSalesTax"_qs, order->usSalesTax());
            d->m_importQuery.bindValue(u":vatChargeBrickLink"_qs, order->vatChargeBrickLink());
            d->m_importQuery.bindValue(u":currencyCode"_qs, order->currencyCode());
            d->m_importQuery.bindValue(u":grandTotal"_qs, order->grandTotal());
            d->m_importQuery.bindValue(u":paymentCurrencyCode"_qs, order->paymentCurrencyCode());
            d->m_importQuery.bindValue(u":lotCount"_qs, order->lotCount());
            d->m_importQuery.bindValue(u":itemCount"_qs, order->itemCount());
            d->m_importQuery.bindValue(u":cost"_qs, order->cost());
            d->m_importQuery.bindValue(u":status"_qs, int(order->status()));
            d->m_importQuery.bindValue(u":paymentType"_qs, order->paymentType());
            d->m_importQuery.bindValue(u":remarks"_qs, order->remarks());
            d->m_importQuery.bindValue(u":trackingNumber"_qs, order->trackingNumber());
            d->m_importQuery.bindValue(u":paymentStatus"_qs, order->paymentStatus());
            d->m_importQuery.bindValue(u":paymentLastUpdated"_qs, order->paymentLastUpdated().toJulianDay());
            d->m_importQuery.bindValue(u":vatChargeSeller"_qs, order->vatChargeSeller());
            d->m_importQuery.bindValue(u":countryCode"_qs, order->countryCode());
            d->m_importQuery.bindValue(u":orderDataFormat"_qs, int(OrdersPrivate::Format_CompressedXML));
            d->m_importQuery.bindValue(u":orderData"_qs, qCompress(xml.toUtf8()));
            d->m_importQuery.bindValue(u":address"_qs, order->address());
            d->m_importQuery.bindValue(u":phone"_qs, order->phone());

            auto finishGuard = qScopeGuard([this]() { d->m_importQuery.finish(); });

            if (!d->m_importQuery.exec())
                throw Exception("SQL error: %1").arg(d->m_importQuery.lastError().text());

        } catch (const Exception &e) {
            // keep this UI silent for now
            qWarning() << "Failed to import order XML:" << e.errorString();
        }
    }

    d->m_db.commit();

    QFile importedFile(imported.absoluteFilePath());
    importedFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    importedFile.close();
}

QHash<Order *, QString> Orders::parseOrdersXML(const QByteArray &data_)
{
    QString data = QString::fromUtf8(data_); // otherwise characterOffset doesn't match
    QXmlStreamReader xml(data);

    QHash<Order *, QString> result;
    std::unique_ptr<Order> order;

    QHash<QStringView, std::function<void(Order *, const QString &)>> rootTagHash;

    rootTagHash.insert(u"ORDERID",           [](auto *o, auto &v) { o->setId(v); } );
    rootTagHash.insert(u"BUYER",             [](auto *o, auto &v) { o->setOtherParty(v); o->setType(OrderType::Received); } );
    rootTagHash.insert(u"SELLER",            [](auto *o, auto &v) { o->setOtherParty(v); o->setType(OrderType::Placed); } );
    rootTagHash.insert(u"ORDERDATE",         [](auto *o, auto &v) { o->setDate(QDate::fromString(v, u"M/d/yyyy")); } );
    rootTagHash.insert(u"ORDERSTATUSCHANGED",[](auto *o, auto &v) { o->setLastUpdated(QDate::fromString(v, u"M/d/yyyy")); } );
    rootTagHash.insert(u"ORDERSHIPPING",     [](auto *o, auto &v) { o->setShipping(v.toDouble()); } );
    rootTagHash.insert(u"ORDERINSURANCE",    [](auto *o, auto &v) { o->setInsurance(v.toDouble()); } );
    rootTagHash.insert(u"ORDERADDCHRG1",     [](auto *o, auto &v) { o->setAdditionalCharges1(v.toDouble()); } );
    rootTagHash.insert(u"ORDERADDCHRG2",     [](auto *o, auto &v) { o->setAdditionalCharges2(v.toDouble()); } );
    rootTagHash.insert(u"ORDERCREDIT",       [](auto *o, auto &v) { o->setCredit(v.toDouble()); } );
    rootTagHash.insert(u"ORDERCREDITCOUPON", [](auto *o, auto &v) { o->setCreditCoupon(v.toDouble()); } );
    rootTagHash.insert(u"ORDERTOTAL",        [](auto *o, auto &v) { o->setOrderTotal(v.toDouble()); } );
    rootTagHash.insert(u"ORDERSALESTAX",     [](auto *o, auto &v) { o->setUsSalesTax(v.toDouble()); } );   // US SalesTax collected by BL
    rootTagHash.insert(u"ORDERVAT",          [](auto *o, auto &v) { o->setVatChargeBrickLink(v.toDouble()); } ); // VAT collected by BL
    rootTagHash.insert(u"BASECURRENCYCODE",  [](auto *o, auto &v) { o->setCurrencyCode(v); } );
    rootTagHash.insert(u"BASEGRANDTOTAL",    [](auto *o, auto &v) { o->setGrandTotal(v.toDouble()); } );
    rootTagHash.insert(u"PAYCURRENCYCODE",   [](auto *o, auto &v) { o->setPaymentCurrencyCode(v); } );
    rootTagHash.insert(u"ORDERLOTS",         [](auto *o, auto &v) { o->setLotCount(v.toInt()); } );
    rootTagHash.insert(u"ORDERITEMS",        [](auto *o, auto &v) { o->setItemCount(v.toInt()); } );
    rootTagHash.insert(u"ORDERCOST",         [](auto *o, auto &v) { o->setCost(v.toDouble()); } );
    rootTagHash.insert(u"ORDERSTATUS",       [](auto *o, auto &v) { o->setStatus(Order::statusFromString(v)); } );
    rootTagHash.insert(u"PAYMENTTYPE",       [](auto *o, auto &v) { o->setPaymentType(v); } );
    rootTagHash.insert(u"ORDERREMARKS",      [](auto *o, auto &v) { o->setRemarks(v); } );
    rootTagHash.insert(u"ORDERTRACKNO",      [](auto *o, auto &v) { o->setTrackingNumber(v); } );
    rootTagHash.insert(u"PAYMENTSTATUS",     [](auto *o, auto &v) { o->setPaymentStatus(v); } );
    rootTagHash.insert(u"PAYMENTSTATUSCHANGED", [](auto *o, auto &v) { o->setPaymentLastUpdated(QDate::fromString(v, u"M/d/yyyy")); } );
    rootTagHash.insert(u"VATCHARGES",        [](auto *o, auto &v) { o->setVatChargeSeller(v.toDouble()); } ); // VAT charge by seller
    rootTagHash.insert(u"LOCATION",          [](auto *o, auto &v) { if (!v.isEmpty()) o->setCountryCode(BrickLink::core()->countryIdFromName(v.section(u", "_qs, 0, 0))); } );

    try {
        int startOfOrder = -1;

        while (true) {
            switch (xml.readNext()) {
            case QXmlStreamReader::StartElement: {
                auto tagName = xml.name();

                if (tagName == u"ORDER") {
                    if (order || startOfOrder >= 0)
                        throw Exception("Found a nested ORDER tag");
                    startOfOrder = int(xml.characterOffset());
                    order = std::make_unique<Order>();

                    QQmlEngine::setObjectOwnership(order.get(), QQmlEngine::CppOwnership);

                } else if (tagName != u"ORDERS") {
                    auto it = rootTagHash.find(xml.name());
                    if (it != rootTagHash.end())
                        (*it)(order.get(), xml.readElementText());
                    else
                        xml.skipCurrentElement();
                }

                break;
            }

            case QXmlStreamReader::EndElement: {
                auto tagName = xml.name();

                if (tagName == u"ORDER") {
                    if (!order || (startOfOrder < 0))
                        throw Exception("Found a ORDER end tag without a start tag");
                    int endOfOrder = int(xml.characterOffset());
                    QString header = u"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<ORDER>\n"_qs;
                    QString footer = u"\n"_qs;
                    result.insert(order.release(), header + data.mid(startOfOrder, endOfOrder - startOfOrder + 1) + footer);
                    startOfOrder = -1;
                }
                break;
            }
            case QXmlStreamReader::EndDocument:
                return result;

            case QXmlStreamReader::Invalid:
                throw Exception(xml.errorString());

            default:
                break;
            }
        }
    } catch (const Exception &e) {
        qDeleteAll(result.keyBegin(), result.keyEnd());
        throw Exception("XML parse error at line %1, column %2: %3")
                .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.errorString());
    }
}

void Orders::updateOrder(std::unique_ptr<Order> newOrder)
{
    for (Order *order : std::as_const(d->m_orders)) {
        if (order->id() != newOrder->id())
            continue;

        Q_ASSERT(order->type() == newOrder->type());
        Q_ASSERT(order->date() == newOrder->date());

        order->setLastUpdated(newOrder->lastUpdated());
        order->setOtherParty(newOrder->otherParty());
        order->setShipping(newOrder->shipping());
        order->setInsurance(newOrder->insurance());
        order->setAdditionalCharges1(newOrder->additionalCharges1());
        order->setAdditionalCharges2(newOrder->additionalCharges2());
        order->setCredit(newOrder->credit());
        order->setCreditCoupon(newOrder->creditCoupon());
        order->setOrderTotal(newOrder->orderTotal());
        order->setUsSalesTax(newOrder->usSalesTax());
        order->setVatChargeBrickLink(newOrder->vatChargeBrickLink());
        order->setCurrencyCode(newOrder->currencyCode());
        order->setGrandTotal(newOrder->grandTotal());
        order->setPaymentCurrencyCode(newOrder->paymentCurrencyCode());
        order->setLotCount(newOrder->lotCount());
        order->setItemCount(newOrder->itemCount());
        order->setCost(newOrder->cost());
        order->setStatus(newOrder->status());
        order->setPaymentType(newOrder->paymentType());
        order->setRemarks(newOrder->remarks());
        order->setTrackingNumber(newOrder->trackingNumber());
        order->setPaymentStatus(newOrder->paymentStatus());
        order->setPaymentLastUpdated(newOrder->paymentLastUpdated());
        order->setVatChargeSeller(newOrder->vatChargeSeller());
        order->setCountryCode(newOrder->countryCode());
        order->setAddress(newOrder->address());
        order->setPhone(newOrder->phone());

        newOrder.reset();

        if (order->address().isEmpty() && d->m_core->isAuthenticated())
            startUpdateAddress(order);

        return;
    }
    appendOrderToModel(std::move(newOrder));  // not found -> add it
}

void Orders::appendOrderToModel(std::unique_ptr<Order> order)
{
    Order *o = order.release();
    o->setParent(this); // needed to prevent QML from taking ownership

    int row = int(d->m_orders.count());
    beginInsertRows({ }, row, row);

    connect(o, &Order::idChanged, this, [this, row]() { emitDataChanged(row, OrderId); });
    connect(o, &Order::otherPartyChanged, this, [this, row]() { emitDataChanged(row, OtherParty); });
    connect(o, &Order::dateChanged, this, [this, row]() { emitDataChanged(row, Date); });
    connect(o, &Order::typeChanged, this, [this, row]() { emitDataChanged(row, Type); });
    connect(o, &Order::statusChanged, this, [this, row]() { emitDataChanged(row, Status); });
    connect(o, &Order::itemCountChanged, this, [this, row]() { emitDataChanged(row, ItemCount); });
    connect(o, &Order::lotCountChanged, this, [this, row]() { emitDataChanged(row, LotCount); });
    connect(o, &Order::grandTotalChanged, this, [this, row]() { emitDataChanged(row, Total); });
    connect(o, &Order::addressChanged, this, [this, row]() { emitDataChanged(row, -1); });

    if (o->address().isEmpty() && d->m_core->isAuthenticated())
        startUpdateAddress(o);
    d->m_orders.append(o);

    endInsertRows();
    emit countChanged(rowCount());
}

void Orders::setLastUpdated(const QDateTime &lastUpdated)
{
    if (lastUpdated != d->m_lastUpdated) {
        d->m_lastUpdated = lastUpdated;
        emit lastUpdatedChanged(lastUpdated);
    }
}

void Orders::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != d->m_updateStatus) {
        d->m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}

void Orders::emitDataChanged(int row, int col)
{
    QModelIndex from = index(row, col < 0 ? 0 : col);
    QModelIndex to = index(row, col < 0 ? columnCount() - 1 : col);
    emit dataChanged(from, to);
}

void Orders::startUpdateAddress(Order *order)
{
    // is there already a job scheduled for this order's address?
    for (const auto *job : std::as_const(d->m_addressJobs)) {
        if (job->userData("address").toString() == order->id())
            return;
    }

    QUrl url(u"https://www.bricklink.com/orderDetail.asp"_qs);
    QUrlQuery query;
    query.addQueryItem(u"ID"_qs, Utility::urlQueryEscape(order->id()));
    url.setQuery(query);

    auto job = TransferJob::get(url);
    job->setUserData("address", order->id());
    d->m_addressJobs << job;

    d->m_core->retrieveAuthenticated(job);
}

std::pair<QString, QString> Orders::parseAddressAndPhone(OrderType type, const QByteArray &data)
{

    if (!data.isEmpty()) {
        QString s = QString::fromUtf8(data);
        QString a;

        static const QRegularExpression regExp(uR"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Name & Address:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)"_qs);
        auto matches = regExp.globalMatch(s);
        if (type == OrderType::Placed) {
            // skip our own address
            if (matches.hasNext())
                matches.next();
        }
        if (matches.hasNext()) {
            std::pair<QString, QString> result;

            QRegularExpressionMatch match = matches.next();
            a = match.captured(1);
            static const QRegularExpression breakRegExp(uR"(<[bB][rR] ?/?>)"_qs);
            a.replace(breakRegExp, u"\n"_qs);
            result.first = a;

            static const QRegularExpression phoneRegExp(uR"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Phone:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)"_qs);

            auto phoneMatch = phoneRegExp.match(s, match.capturedEnd());
            if (phoneMatch.hasMatch())
                result.second = phoneMatch.captured(1);
            return result;
        }
    }
    return { };
}


Orders::~Orders()
{ /* needed to use std::unique_ptr on d */ }

QDateTime Orders::lastUpdated() const
{
    return d->m_lastUpdated;
}

UpdateStatus Orders::updateStatus() const
{
    return d->m_updateStatus;
}

void Orders::startUpdate(const QString &id)
{
    startUpdateInternal({ }, { }, id);
}

void Orders::startUpdate(int lastNDays)
{
    QDate today = QDate::currentDate();
    QDate fromDate = today.addDays(-lastNDays);

    startUpdateInternal(fromDate, today, { });
}

void Orders::startUpdate(const QDate &fromDate, const QDate &toDate)
{
    startUpdateInternal(fromDate, toDate, { });
}

void Orders::startUpdateInternal(const QDate &fromDate, const QDate &toDate,
                                 const QString &orderId)
{
    if (updateStatus() == UpdateStatus::Updating)
        return;
    if (d->m_core->userId().isEmpty())
        return;
    Q_ASSERT(d->m_jobs.isEmpty());
    setUpdateStatus(UpdateStatus::Updating);

    static const std::array types = { "received", "placed" };
    for (auto &type : types) {
        QUrl url(u"https://www.bricklink.com/orderExcelFinal.asp"_qs);
        QUrlQuery query;
        query.addQueryItem(u"action"_qs,        u"save"_qs);
        query.addQueryItem(u"orderType"_qs,     QLatin1String(type));
        query.addQueryItem(u"viewType"_qs,      u"X"_qs);
        if (fromDate.isValid() && toDate.isValid()) {
            query.addQueryItem(u"getOrders"_qs,     u"date"_qs);
            query.addQueryItem(u"fMM"_qs,           QString::number(fromDate.month()));
            query.addQueryItem(u"fDD"_qs,           QString::number(fromDate.day()));
            query.addQueryItem(u"fYY"_qs,           QString::number(fromDate.year()));
            query.addQueryItem(u"tMM"_qs,           QString::number(toDate.month()));
            query.addQueryItem(u"tDD"_qs,           QString::number(toDate.day()));
            query.addQueryItem(u"tYY"_qs,           QString::number(toDate.year()));
        } else if (!orderId.isEmpty()) {
            query.addQueryItem(u"orderID"_qs,       orderId);
        }
        query.addQueryItem(u"getStatusSel"_qs,  u"I"_qs);
        query.addQueryItem(u"getFiled"_qs,      u"Y"_qs);
        query.addQueryItem(u"getDetail"_qs,     u"y"_qs);
        query.addQueryItem(u"getDateFormat"_qs, u"0"_qs);    // MM/DD/YYYY
        url.setQuery(query);

        auto job = TransferJob::post(url);
        job->setUserData(type, true);
        d->m_jobs << job;

        d->m_core->retrieveAuthenticated(job);
    }
}

void Orders::cancelUpdate()
{
    if (d->m_updateStatus == UpdateStatus::Updating)
        std::for_each(d->m_jobs.cbegin(), d->m_jobs.cend(), [](auto job) { job->abort(); });
    std::for_each(d->m_addressJobs.cbegin(), d->m_addressJobs.cend(), [](auto job) { job->abort(); });
}

LotList Orders::loadOrderLots(const Order *order) const
{
    d->m_loadXmlQuery.bindValue(u":id"_qs, order->id());

    auto finishGuard = qScopeGuard([this]() { d->m_loadXmlQuery.finish(); });

    if (!d->m_loadXmlQuery.next())
        throw Exception("could not find order %1 in database").arg(order->id());

    auto format = OrdersPrivate::OrderDataFormat(d->m_loadXmlQuery.value(u"orderDataFormat"_qs).toInt());
    auto data = d->m_loadXmlQuery.value(u"orderData"_qs).toByteArray();

    switch (format) {
    case OrdersPrivate::Format_XML:
        break;
    case OrdersPrivate::Format_CompressedXML:
        data = qUncompress(data);
        break;
    default:
        throw Exception("unknown data format (%1) for order").arg(int(format));
        break;
    }
    auto pr = IO::fromBrickLinkXML(data, IO::Hint::Order);
    return pr.takeLots();
}

Order *Orders::order(int index) const
{
    return d->m_orders.value(index);
}

int Orders::indexOfOrder(const QString &orderId) const
{
    for (int i = 0; i < d->m_orders.count(); ++i) {
        if (d->m_orders.at(i)->id() == orderId)
            return i;
    }
    return -1;
}

int Orders::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(d->m_orders.count());
}

int Orders::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant Orders::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= d->m_orders.size()))
        return { };

    Order *order = d->m_orders.at(index.row());
    int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
        case Date: return QLocale::system().toString(order->date(), QLocale::ShortFormat);
        case Type: return (order->type() == OrderType::Received)
                    ? tr("Received") : tr("Placed");
        case Status: return order->statusAsString(true);
        case OrderId: return order->id();
        case OtherParty: {
            auto firstline = order->address().indexOf(u'\n');
            if (firstline > 0) {
                return u"%2 (%1)"_qs.arg(order->address().left(firstline), order->otherParty());
            }
            return order->otherParty();
        }
        case ItemCount: return QLocale::system().toString(order->itemCount());
        case LotCount: return QLocale::system().toString(order->lotCount());
        case Total: return Currency::toDisplayString(order->grandTotal(), order->currencyCode(), 2);
        }
    } else if (role == Qt::DecorationRole) {
        switch (col) {
        case OtherParty: {
            QIcon flag;
            QString cc = order->countryCode();
            flag = d->m_flags.value(cc);
            if (flag.isNull()) {
                flag.addFile(u":/assets/flags/" + cc, { }, QIcon::Normal);
                flag.addFile(u":/assets/flags/" + cc, { }, QIcon::Selected);
                d->m_flags.insert(cc, flag);
            }
            return flag;
        }
        }
    } else if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignVCenter) | int((col == Total) ? Qt::AlignRight : Qt::AlignLeft);
    } else if (role == Qt::BackgroundRole) {
        if (col == Type) {
            QColor c((order->type() == OrderType::Received) ? Qt::green : Qt::blue);
            c.setAlphaF(0.1f);
            return c;
        } else if (col == Status) {
            QColor c = QColor::fromHslF(float(order->status()) / float(OrderStatus::Count),
                                        .5f, .5f, .5f);
            return c;
        }
    } else if (role == Qt::ToolTipRole) {
        QString tt = data(index, Qt::DisplayRole).toString();

        if (!order->address().isEmpty())
            tt = tt + u"\n\n" + order->address();
        return tt;
    } else if (role == OrderPointerRole) {
        return QVariant::fromValue(order);
    } else if (role == OrderSortRole) {
        switch (col) {
        case Date:       return order->date();
        case Type:       return int(order->type());
        case Status:     return order->statusAsString(true);
        case OrderId:    return order->id();
        case OtherParty: return order->otherParty();
        case ItemCount:  return order->itemCount();
        case LotCount:   return order->lotCount();
        case Total:      return order->grandTotal();
        }
    } else if (role == DateRole) {
        return order->date();
    } else if (role == TypeRole) {
        return QVariant::fromValue(order->type());
    }

    return { };
}

QVariant Orders::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
            case Date:       return tr("Date");
            case Type:       return tr("Type");
            case Status:     return tr("Status");
            case OrderId:    return tr("Order ID");
            case OtherParty: return tr("Buyer/Seller");
            case ItemCount:  return tr("Items");
            case LotCount:   return tr("Lots");
            case Total:      return tr("Total");
            }
        } else if (role == Qt::TextAlignmentRole) {
            return (section == Total) ? Qt::AlignRight : Qt::AlignLeft;
        }
    }
    return { };
}

QHash<int, QByteArray> Orders::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "display" },
        { Qt::TextAlignmentRole, "textAlignment" },
        { Qt::DecorationRole, "decoration" },
        { Qt::BackgroundRole, "background" },
        { OrderPointerRole, "order" },
        { DateRole, "date" },
        { TypeRole, "type" },
    };
    return roles;
}

} // namespace BrickLink

#include "moc_order.cpp"
