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
#include <QDomDocument>
#include <QDomElement>
#include <QSaveFile>
#include <QDirIterator>
#include <QCoreApplication>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QXmlStreamReader>

#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/order.h"

#include "utility/currency.h"
#include "utility/exception.h"
#include "utility/stopwatch.h"
#include "utility/transfer.h"
#include "utility/utility.h"
#include "utility/xmlhelpers.h"


namespace BrickLink {

class OrderPrivate
{
private:
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

    friend class Order;
};

/*! \qmltype Order
    \inqmlmodule BrickStore
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
    \value BrickLink.Received  A received order.
    \value BrickLink.Placed    A placed order.
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
{ }

LotList Order::loadLots() const
{
    if (auto orders = qobject_cast<Orders *>(parent())) {
        try {
            return orders->loadOrderLots(this);
        } catch (const Exception &e) {
            qWarning() << "Order::loadLots() failed:" << e.error();
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

QString Order::statusToString(OrderStatus status, bool translated)
{
    for (const auto &os : orderStatus) {
        if (status == os.first) {
            return translated ? QCoreApplication::translate("Orders", os.second)
                              : QString::fromLatin1(os.second);
        }
    }
    return { };
}


Orders::Orders(Core *core)
    : QAbstractTableModel(core)
    , m_core(core)
{
    //TODO: Remove in 2022.6.x
    QDir legacyPath(core->dataPath() % u"orders/");
    if (legacyPath.cd("received"_l1)) {
        legacyPath.removeRecursively();
        legacyPath.cdUp();
    }
    if (legacyPath.cd("placed"_l1)) {
        legacyPath.removeRecursively();
        legacyPath.cdUp();
    }

    connect(core, &Core::userIdChanged,
            this, &Orders::reloadOrdersFromCache);
    reloadOrdersFromCache();

    connect(core, &Core::authenticatedTransferStarted,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && !m_jobs.isEmpty()
                && (m_jobs.constFirst() == job)) {
            emit updateStarted();
        }
    });
    connect(core, &Core::authenticatedTransferProgress,
            this, [this](TransferJob *job, int progress, int total) {
        if ((m_updateStatus == UpdateStatus::Updating) && m_jobs.contains(job)) {
            m_jobProgress[job] = qMakePair(progress, total);

            int overallProgress = 0, overallTotal = 0;
            for (auto pair : qAsConst(m_jobProgress)) {
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

        if (m_addressJobs.contains(job) && (type == "address")) {
            m_addressJobs.removeOne(job);

            try {
                if (!jobCompleted)
                    throw Exception(job->errorString());

                int row = indexOfOrder(job->userData(type).toString());
                if (row >= 0) {
                    Order *order = m_orders.at(row);
                    auto [address, phone] = parseAddressAndPhone(order->type(), *job->data());
                    if (address.isEmpty())
                        address = tr("Address not available");

                    QVariantMap vm = {
                        { "id"_l1, order->id() },
                        { "address"_l1, address },
                        { "phone"_l1, phone },
                    };
                    QJsonDocument json = QJsonDocument::fromVariant(vm);
                    QByteArray utf8 = json.toJson();

                    std::unique_ptr<QSaveFile> saveFile { orderSaveFile(QString(order->id() % u".brickstore.json"),
                                                                        order->type(), order->date()) };

                    if (!saveFile
                            || (saveFile->write(utf8) != utf8.size())
                            || !saveFile->commit()) {
                        throw Exception(saveFile.get(), tr("Cannot write order address to cache"));
                    }
                    order->setAddress(address);
                }
            } catch (const Exception &e) {
                qWarning() << "Failed to retrieve address for order"
                           << job->userData(type).toString() << ":" << e.error();
            }
        } else if ((m_updateStatus == UpdateStatus::Updating) && m_jobs.contains(job)) {
            bool success = true;
            QString message;

            if (jobCompleted) { // if there are no matching orders, we get an error reply back...
                // step 1: split up the individual orders into <cache>/<year>/<month>/<id>.xml
                OrderType orderType = (type == "received") ? OrderType::Received : OrderType::Placed;
                QHash<Order *, QString> orders;

                try {
                    orders = parseOrdersXML(*job->data());

                    for (auto it = orders.cbegin(); it != orders.cend(); ++it) {
                        Order *order = it.key();
                        QByteArray orderXml = it.value().toUtf8();

                        if (order->id().isEmpty() || !order->date().isValid())
                            throw Exception("Invalid order without ID and DATE");

                        std::unique_ptr<QSaveFile> saveFile { orderSaveFile(QString(order->id() % u".order.xml"),
                                                                            orderType, order->date()) };
                        if (!saveFile
                                || (saveFile->write(orderXml) != orderXml.size())
                                || !saveFile->commit()) {
                            throw Exception(saveFile.get(), tr("Cannot write order XML to cache"));
                        }
                    }

                    while (!orders.isEmpty()) {
                        auto order = std::unique_ptr<Order> { *orders.keyBegin() };
                        orders.remove(order.get());
                        updateOrder(std::move(order));
                    }
                } catch (const Exception &e) {
                    success = false;
                    message = tr("Could not parse the received order XML data") % u": " % e.error();
                }
                qDeleteAll(orders.keyBegin(), orders.keyEnd());
            }
            m_jobResult[job] = qMakePair(success, message);
            m_jobs.removeOne(job);

            if (m_jobs.isEmpty()) {
                bool overallSuccess = true;
                QString overallMessage;
                for (const auto &pair : qAsConst(m_jobResult)) {
                    overallSuccess = overallSuccess && pair.first;
                    if (overallMessage.isEmpty()) // only show the first error message
                        overallMessage = pair.second;
                }
                setUpdateStatus(overallSuccess ? UpdateStatus::Ok : UpdateStatus::UpdateFailed);

                QFile stampFile(m_core->dataPath() % u"orders/" % m_core->userId() % u"/.stamp");
                if (overallSuccess) {
                    m_lastUpdated = QDateTime::currentDateTime();
                    stampFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
                    stampFile.close();
                } else {
                    stampFile.remove();
                }
                m_jobProgress.clear();
                m_jobResult.clear();

                emit updateFinished(overallSuccess, overallMessage);
            }
        }
    });
}

void Orders::reloadOrdersFromCache()
{
    beginResetModel();
    qDeleteAll(m_orders);
    m_orders.clear();
    endResetModel();

    if (m_core->userId().isEmpty())
        return;

    QString path = m_core->dataPath() % u"orders/" % m_core->userId();

    QFileInfo stamp(path % u"/.stamp");
    m_lastUpdated = stamp.lastModified();
    setUpdateStatus(stamp.exists() ? UpdateStatus::Ok : UpdateStatus::UpdateFailed);

    QThreadPool::globalInstance()->start([this, path]() {
        stopwatch sw("Loading orders from cache");

        QDirIterator dit(path, { "*.order.xml"_l1 },
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
                Order *order = *orders.keyBegin();
                order->moveToThread(this->thread());

                QDir d = dit.fileInfo().absoluteDir();
                QString addressFileName = order->id() % u".brickstore.json";
                if (d.exists(addressFileName)) {
                    QFile f(d.absoluteFilePath(addressFileName));
                    if (f.open(QIODevice::ReadOnly) && (f.size() < 5000)) {
                        auto json = QJsonDocument::fromJson(f.readAll());
                        if (json.isObject()) {
                            order->setAddress(json["address"_l1].toString());
                            order->setPhone(json["phone"_l1].toString());
                        }
                    }
                }
                QMetaObject::invokeMethod(this, [this, order]() {
                    std::unique_ptr<Order> uniqueOrder { order };
                    appendOrderToModel(std::move(uniqueOrder));
                });
            } catch (const Exception &e) {
                // keep this UI silent for now
                qWarning() << "Failed to load order XML:" << e.error();
            }
        }
    });
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
    rootTagHash.insert(u"ORDERDATE",         [](auto *o, auto &v) { o->setDate(QDate::fromString(v, "M/d/yyyy"_l1)); } );
    rootTagHash.insert(u"ORDERSTATUSCHANGED",[](auto *o, auto &v) { o->setLastUpdated(QDate::fromString(v, "M/d/yyyy"_l1)); } );
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
    rootTagHash.insert(u"PAYMENTSTATUSCHANGED", [](auto *o, auto &v) { o->setPaymentLastUpdated(QDate::fromString(v, "M/d/yyyy"_l1)); } );
    rootTagHash.insert(u"VATCHARGES",        [](auto *o, auto &v) { o->setVatChargeSeller(v.toDouble()); } ); // VAT charge by seller
    rootTagHash.insert(u"LOCATION",          [](auto *o, auto &v) { if (!v.isEmpty()) o->setCountryCode(BrickLink::core()->countryIdFromName(v.section(", "_l1, 0, 0))); } );

    try {
        int startOfOrder = -1;

        while (true) {
            switch (xml.readNext()) {
            case QXmlStreamReader::StartElement: {
                auto tagName = xml.name();

                if (tagName == "ORDER"_l1) {
                    if (order || startOfOrder >= 0)
                        throw Exception("Found a nested ORDER tag");
                    startOfOrder = int(xml.characterOffset());
                    order.reset(new Order());

                } else if (tagName != "ORDERS"_l1) {
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

                if (tagName == "ORDER"_l1) {
                    if (!order || (startOfOrder < 0))
                        throw Exception("Found a ORDER end tag without a start tag");
                    int endOfOrder = int(xml.characterOffset());
                    QString header = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<ORDER>\n"_l1;
                    QString footer = "\n"_l1;
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
                .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.error());
    }
}

void Orders::updateOrder(std::unique_ptr<Order> newOrder)
{
    for (Order *order : qAsConst(m_orders)) {
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

        if (order->address().isEmpty() && m_core->isAuthenticated())
            startUpdateAddress(order);

        return;
    }
    appendOrderToModel(std::move(newOrder));  // not found -> add it
}

void Orders::appendOrderToModel(std::unique_ptr<Order> order)
{
    Order *o = order.release();
    o->setParent(this); // needed to prevent QML from taking ownership

    int row = m_orders.count();
    beginInsertRows({ }, m_orders.count(), m_orders.count());

    connect(o, &Order::idChanged, this, [this, row]() { emitDataChanged(row, OrderId); });
    connect(o, &Order::otherPartyChanged, this, [this, row]() { emitDataChanged(row, OtherParty); });
    connect(o, &Order::dateChanged, this, [this, row]() { emitDataChanged(row, Date); });
    connect(o, &Order::typeChanged, this, [this, row]() { emitDataChanged(row, Type); });
    connect(o, &Order::statusChanged, this, [this, row]() { emitDataChanged(row, Status); });
    connect(o, &Order::itemCountChanged, this, [this, row]() { emitDataChanged(row, ItemCount); });
    connect(o, &Order::lotCountChanged, this, [this, row]() { emitDataChanged(row, LotCount); });
    connect(o, &Order::grandTotalChanged, this, [this, row]() { emitDataChanged(row, Total); });
    connect(o, &Order::addressChanged, this, [this, row]() { emitDataChanged(row, -1); });

    if (o->address().isEmpty() && m_core->isAuthenticated())
        startUpdateAddress(o);
    m_orders.append(o);

    endInsertRows();
}

void Orders::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
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
    for (const auto *job : qAsConst(m_addressJobs)) {
        if (job->userData("address").toString() == order->id())
            return;
    }

    QUrl url("https://www.bricklink.com/orderDetail.asp"_l1);
    QUrlQuery query;
    query.addQueryItem("ID"_l1, Utility::urlQueryEscape(order->id()));
    url.setQuery(query);

    auto job = TransferJob::get(url);
    job->setUserData("address", order->id());
    m_addressJobs << job;

    m_core->retrieveAuthenticated(job);
}

std::pair<QString, QString> Orders::parseAddressAndPhone(OrderType type, const QByteArray &data)
{

    if (!data.isEmpty()) {
        QString s = QString::fromUtf8(data);
        QString a;

        static const QRegularExpression regExp(R"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Name & Address:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)"_l1);
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
            static const QRegularExpression breakRegExp(R"(<[bB][rR] ?/?>)"_l1);
            a.replace(breakRegExp, "\n"_l1);
            result.first = a;

            static const QRegularExpression phoneRegExp(R"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Phone:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)"_l1);

            auto phoneMatch = phoneRegExp.match(s, match.capturedEnd());
            if (phoneMatch.hasMatch())
                result.second = phoneMatch.captured(1);
            return result;
        }
    }
    return { };
};

QSaveFile *Orders::orderSaveFile(QStringView fileName, OrderType type, const QDate &date) const
{
    // Avoid huge directories with 1000s of entries.
    QString p = orderFilePath(fileName, type, date);

    if (!QDir(fileName.isEmpty() ? p : p.left(p.size() - int(fileName.size()))).mkpath("."_l1))
        return nullptr;

    auto f = new QSaveFile(p);
    if (!f->open(QIODevice::WriteOnly)) {
        qWarning() << "Orders::orderSaveFile failed to open" << f->fileName()
                   << "for writing:" << f->errorString();
    }
    return f;
}

QString Orders::orderFilePath(QStringView fileName, OrderType type, const QDate &date) const
{
    return m_core->dataPath() % "orders/"_l1 % m_core->userId()
            % ((type == OrderType::Received) ? "/received/"_l1: "/placed/"_l1 )
            % QString::number(date.year()) % u'/'
            % QString("%1"_l1).arg(date.month(), 2, 10, '0'_l1) % u'/'
            % fileName;
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
    if (m_core->userId().isEmpty())
        return;
    Q_ASSERT(m_jobs.isEmpty());
    setUpdateStatus(UpdateStatus::Updating);

    static const char *types[] = { "received", "placed" };
    for (auto &type : types) {
        QUrl url("https://www.bricklink.com/orderExcelFinal.asp"_l1);
        QUrlQuery query;
        query.addQueryItem("action"_l1,        "save"_l1);
        query.addQueryItem("orderType"_l1,     QLatin1String(type));
        query.addQueryItem("viewType"_l1,      "X"_l1);
        if (fromDate.isValid() && toDate.isValid()) {
            query.addQueryItem("getOrders"_l1,     "date"_l1);
            query.addQueryItem("fMM"_l1,           QString::number(fromDate.month()));
            query.addQueryItem("fDD"_l1,           QString::number(fromDate.day()));
            query.addQueryItem("fYY"_l1,           QString::number(fromDate.year()));
            query.addQueryItem("tMM"_l1,           QString::number(toDate.month()));
            query.addQueryItem("tDD"_l1,           QString::number(toDate.day()));
            query.addQueryItem("tYY"_l1,           QString::number(toDate.year()));
        } else if (!orderId.isEmpty()) {
            query.addQueryItem("orderID"_l1,       orderId);
        }
        query.addQueryItem("getStatusSel"_l1,  "I"_l1);
        query.addQueryItem("getFiled"_l1,      "Y"_l1);
        query.addQueryItem("getDetail"_l1,     "y"_l1);
        query.addQueryItem("getDateFormat"_l1, "0"_l1);    // MM/DD/YYYY
        url.setQuery(query);

        auto job = TransferJob::post(url);
        job->setUserData(type, true);
        m_jobs << job;

        m_core->retrieveAuthenticated(job);
    }
}

void Orders::cancelUpdate()
{
    if (m_updateStatus == UpdateStatus::Updating)
        std::for_each(m_jobs.cbegin(), m_jobs.cend(), [](auto job) { job->abort(); });
    std::for_each(m_addressJobs.cbegin(), m_addressJobs.cend(), [](auto job) { job->abort(); });
}

LotList Orders::loadOrderLots(const Order *order) const
{
    QString fileName = order->id() % ".order.xml"_l1;
    QFile f(Orders::orderFilePath(fileName, order->type(), order->date()));
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(&f, tr("Cannot open order XML"));
    auto xml = f.readAll();
    auto pr = IO::fromBrickLinkXML(xml, IO::Hint::Order);
    return pr.takeLots();
}

const Order *Orders::order(int row) const
{
    return (row < 0 || row >= m_orders.size()) ? nullptr : m_orders.at(row);
}

QVector<Order *> Orders::orders() const
{
    return m_orders;
}

int Orders::indexOfOrder(const QString &orderId) const
{
    for (int i = 0; i < m_orders.count(); ++i) {
        if (m_orders.at(i)->id() == orderId)
            return i;
    }
    return -1;
}

int Orders::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_orders.count();
}

int Orders::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant Orders::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= m_orders.size()))
        return QVariant();

    Order *order = m_orders.at(index.row());
    int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
        case Date: return QLocale::system().toString(order->date(), QLocale::ShortFormat);
        case Type: return (order->type() == OrderType::Received)
                    ? tr("Received") : tr("Placed");
        case Status: return Order::statusToString(order->status(), true);
        case OrderId: return order->id();
        case OtherParty: {
            int firstline = order->address().indexOf('\n'_l1);
            if (firstline > 0) {
                return QString::fromLatin1("%2 (%1)")
                        .arg(order->address().left(firstline), order->otherParty());
            }
            return order->otherParty();
        }
        case ItemCount: return QLocale::system().toString(order->itemCount());
        case LotCount: return QLocale::system().toString(order->lotCount());
        case Total: return Currency::toString(order->grandTotal(), order->currencyCode(), 2);
        }
    } else if (role == Qt::DecorationRole) {
        switch (col) {
        case OtherParty: {
            QIcon flag;
            QString cc = order->countryCode();
            flag = m_flags.value(cc);
            if (flag.isNull()) {
                flag.addFile(u":/assets/flags/" % cc, { }, QIcon::Normal);
                flag.addFile(u":/assets/flags/" % cc, { }, QIcon::Selected);
                m_flags.insert(cc, flag);
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
            QColor c = QColor::fromHslF(qreal(order->status()) / qreal(OrderStatus::Count),
                                        .5, .5, .5);
            return c;
        }
    } else if (role == Qt::ToolTipRole) {
        QString tt = data(index, Qt::DisplayRole).toString();

        if (!order->address().isEmpty())
            tt = tt + "\n\n"_l1 + order->address();
        return tt;
    } else if (role == OrderPointerRole) {
        return QVariant::fromValue(order);
    } else if (role == OrderSortRole) {
        switch (col) {
        case Date:       return order->date();
        case Type:       return int(order->type());
        case Status:     return Order::statusToString(order->status(), true);
        case OrderId:    return order->id();
        case OtherParty: return order->otherParty();
        case ItemCount:  return order->itemCount();
        case LotCount:   return order->lotCount();
        case Total:      return order->grandTotal();
        }
    } else if (role >= OrderFirstColumnRole && role < OrderLastColumnRole) {
        return data(index.sibling(index.row(), role - OrderFirstColumnRole), Qt::DisplayRole);
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
        { int(OrderFirstColumnRole) + Date, "date" },
        { int(OrderFirstColumnRole) + Type, "type" },
        { int(OrderFirstColumnRole) + Status, "status" },
        { int(OrderFirstColumnRole) + OrderId, "id" },
        { int(OrderFirstColumnRole) + OtherParty, "otherParty" },
        { int(OrderFirstColumnRole) + ItemCount, "itemCount" },
        { int(OrderFirstColumnRole) + LotCount, "lotCount" },
        { int(OrderFirstColumnRole) + Total, "grandTotal" },
    };
    return roles;
}

} // namespace BrickLink

#include "moc_order.cpp"
