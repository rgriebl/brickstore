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

#include <QBuffer>
#include <QDomDocument>
#include <QDomElement>
#include <QSaveFile>
#include <QDirIterator>
#include <QCoreApplication>
#include <QUrlQuery>
#include <QRegularExpression>

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
public:
    ~OrderPrivate() { qDeleteAll(m_lots); }

private:
    QString   m_id;
    OrderType m_type;
    QDateTime m_date;
    QDateTime m_lastUpdate;
    QString   m_otherParty;
    double    m_shipping = 0;
    double    m_insurance = 0;
    double    m_addCharges1 = 0;
    double    m_addCharges2 = 0;
    double    m_credit = 0;
    double    m_creditCoupon = 0;
    double    m_orderTotal = 0;
    double    m_salesTax = 0;
    double    m_grandTotal = 0;
    double    m_vatCharges = 0;
    QString   m_currencyCode;
    QString   m_paymentCurrencyCode;
    int       m_itemCount = 0;
    int       m_lotCount = 0;
    OrderStatus m_status;
    QString   m_paymentType;
    QString   m_trackingNumber;
    QString   m_address;
    QString   m_countryCode;
    LotList   m_lots;

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
/*! \qmlproperty real Order::salesTax
    \readonly
*/
/*! \qmlproperty real Order::grandTotal
    \readonly
*/
/*! \qmlproperty real Order::vatCharges
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

LotList Order::takeLots()
{
    LotList lots;
    std::swap(lots, d->m_lots);
    return lots;
}

Order::Order()
    : Order({ }, OrderType::Received)
{ }


const LotList &Order::lots() const
{
    return d->m_lots;
}

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

double Order::salesTax() const
{
    return d->m_salesTax;
}

double Order::grandTotal() const
{
    return d->m_grandTotal;
}

double Order::vatCharges() const
{
    return d->m_vatCharges;
}

QString Order::currencyCode() const
{
    return d->m_currencyCode;
}

QString Order::paymentCurrencyCode() const
{
    return d->m_paymentCurrencyCode;
}

int Order::itemCount() const
{
    return d->m_itemCount;
}

int Order::lotCount() const
{
    return d->m_lotCount;
}

OrderStatus Order::status() const
{
    return d->m_status;
}

QString Order::paymentType() const
{
    return d->m_paymentType;
}

QString Order::trackingNumber() const
{
    return d->m_trackingNumber;
}

QString Order::address() const
{
    return d->m_address;
}

QString Order::countryCode() const
{
    return d->m_countryCode;
}

void Order::setLots(LotList &&lots)
{
    d->m_lots = lots;
    lots.clear();
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
    if (d->m_shipping != m) {
        d->m_shipping = m;
        emit shippingChanged(m);
    }
}

void Order::setInsurance(double m)
{
    if (d->m_insurance != m) {
        d->m_insurance = m;
        emit insuranceChanged(m);
    }
}

void Order::setAdditionalCharges1(double m)
{
    if (d->m_addCharges1 != m) {
        d->m_addCharges1 = m;
        emit additionalCharges1Changed(m);
    }
}

void Order::setAdditionalCharges2(double m)
{
    if (d->m_addCharges2 != m) {
        d->m_addCharges2 = m;
        emit additionalCharges2Changed(m);
    }
}

void Order::setCredit(double m)
{
    if (d->m_credit != m) {
        d->m_credit = m;
        emit creditChanged(m);
    }
}

void Order::setCreditCoupon(double m)
{
    if (d->m_creditCoupon != m) {
        d->m_creditCoupon = m;
        emit creditCouponChanged(m);
    }
}

void Order::setOrderTotal(double m)
{
    if (d->m_orderTotal != m) {
        d->m_orderTotal = m;
        emit orderTotalChanged(m);
    }
}

void Order::setSalesTax(double m)
{
    if (d->m_salesTax != m) {
        d->m_salesTax = m;
        emit salesTaxChanged(m);
    }
}

void Order::setGrandTotal(double m)
{
    if (d->m_grandTotal != m) {
        d->m_grandTotal = m;
        emit grandTotalChanged(m);
    }
}

void Order::setVatCharges(double m)
{
    if (d->m_vatCharges != m) {
        d->m_vatCharges = m;
        emit vatChargesChanged(m);
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

void Order::setTrackingNumber(const QString &str)
{
    if (d->m_trackingNumber != str) {
        d->m_trackingNumber = str;
        emit trackingNumberChanged(str);
    }
}

void Order::setAddress(const QString &str)
{
    if (d->m_address != str) {
        d->m_address = str;
        emit addressChanged(str);
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


Orders::Orders(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(core(), &Core::databaseDateChanged,
            this, [this]() {
        static bool once = false;
        if (once)
            return;
        once = true;

        stopwatch sw("Loading orders from cache");

        QDirIterator dit(core()->dataPath() % u"orders", { "*.order.xml"_l1 },
                         QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
        while (dit.hasNext()) {
            try {
                dit.next();
                std::unique_ptr<Order> order(Orders::orderFromXML(dit.filePath()));
                QDir d = dit.fileInfo().absoluteDir();
                QString addressFileName = order->id() % u".address.txt";
                if (d.exists(addressFileName)) {
                    QFile f(d.absoluteFilePath(addressFileName));
                    if (f.open(QIODevice::ReadOnly) && (f.size() < 2000)) {
                        QString address = QString::fromUtf8(f.readAll());
                        order->setAddress(address);
                    }
                }
                appendOrderToModel(std::move(order));
            } catch (const Exception &e) {
                // keep this UI silent for now
                qWarning() << "Failed to load order XML:" << e.error();
            }
        }
    }, Qt::QueuedConnection);

    connect(core(), &Core::authenticatedTransferStarted,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && !m_jobs.isEmpty()
                && (m_jobs.constFirst() == job)) {
            emit updateStarted();
        }
    });
    connect(core(), &Core::authenticatedTransferProgress,
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
    connect(core(), &Core::authenticatedTransferFinished,
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
                    QString address = parseAddress(order->type(), *job->data());
                    if (address.isEmpty())
                        address = tr("Address not available");
                    QByteArray utf8 = address.toUtf8();

                    auto saveFile = orderSaveFile(QString(order->id() % u".address.txt"),
                                                  order->type(), order->date());

                    if (!saveFile
                            || (saveFile->write(utf8) != utf8.size())
                            || !saveFile->commit()) {
                        throw Exception(saveFile, tr("Cannot write order address to cache"));
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
                QStringList updatedXmlFiles;
                OrderType orderType = (type == "received") ? OrderType::Received : OrderType::Placed;

                auto buf = new QBuffer(job->data());
                buf->open(QIODevice::ReadOnly);
                try {
                    //TODO: this in inefficient. We should rather use QXmlStreamReader

                    XmlHelpers::ParseXML p(buf, "ORDERS", "ORDER");
                    p.parse([&p, &updatedXmlFiles, orderType](QDomElement e) {
                        auto id = p.elementText(e, "ORDERID");
                        if (id.isEmpty())
                            throw Exception("Order without ORDERID");

                        auto date = QDate::fromString(p.elementText(e, "ORDERDATE"), "M/d/yyyy"_l1);
                        if (!date.isValid())
                            throw Exception("Order %1 has an invalid ORDERDATE").arg(id);

                        QDomDocument orderDoc;
                        auto orderNode = orderDoc.importNode(e, true /*deep*/);
                        orderDoc.appendChild(orderNode);
                        auto orderXml = orderDoc.toByteArray();

                        auto saveFile = orderSaveFile(QString(id % u".order.xml"), orderType, date);
                        if (!saveFile)
                            throw Exception(tr("Cannot save order to file"));

                        if (!saveFile
                                || (saveFile->write(orderXml) != orderXml.size())
                                || !saveFile->commit()) {
                            throw Exception(saveFile, tr("Cannot write order XML to cache"));
                        }
                        updatedXmlFiles << saveFile->fileName();
                    });

                } catch (const Exception &e) {
                    success = false;
                    message = tr("Could not parse the received order XML data") % u": " % e.error();
                }
                if (success) {
                    // step 2: re-read xml files
                    for (const auto &fileName : qAsConst(updatedXmlFiles)) {
                        try {
                            std::unique_ptr<Order> order(Orders::orderFromXML(fileName));
                            updateOrder(std::move(order));
                        } catch (const Exception &e) {
                            success = false;
                            message = tr("Could not parse the received order XML data") % u": " % e.error();
                        }
                    }
                }
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
                m_updateStatus = overallSuccess ? UpdateStatus::Ok : UpdateStatus::UpdateFailed;
                if (overallSuccess)
                    m_lastUpdated = QDateTime::currentDateTime();
                m_jobProgress.clear();
                m_jobResult.clear();

                emit updateFinished(overallSuccess, overallMessage);
            }
        }
    });
}

Order *Orders::orderFromXML(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(&f, tr("Cannot open order XML"));
    auto xml = f.readAll();
    auto pr = IO::fromBrickLinkXML(xml, IO::Hint::Order);
    return pr.takeOrder();
}

void Orders::updateOrder(std::unique_ptr<Order> newOrder)
{
    for (Order *order : qAsConst(m_orders)) {
        if (order->id() != newOrder->id())
            continue;

        Q_ASSERT(order->type() == newOrder->type());
        Q_ASSERT(order->date() == newOrder->date());

        if (order->lastUpdated() != newOrder->lastUpdated())
            order->setLastUpdated(newOrder->lastUpdated());
        if (order->otherParty() != newOrder->otherParty())
            order->setOtherParty(newOrder->otherParty());
        if (order->shipping() != newOrder->shipping())
            order->setShipping(newOrder->shipping());
        if (order->insurance() != newOrder->insurance())
            order->setInsurance(newOrder->insurance());
        if (order->additionalCharges1() != newOrder->additionalCharges1())
            order->setAdditionalCharges1(newOrder->additionalCharges1());
        if (order->additionalCharges2() != newOrder->additionalCharges2())
            order->setAdditionalCharges2(newOrder->additionalCharges2());
        if (order->credit() != newOrder->credit())
            order->setCredit(newOrder->credit());
        if (order->creditCoupon() != newOrder->creditCoupon())
            order->setCreditCoupon(newOrder->creditCoupon());
        if (order->orderTotal() != newOrder->orderTotal())
            order->setOrderTotal(newOrder->orderTotal());
        if (order->salesTax() != newOrder->salesTax())
            order->setSalesTax(newOrder->salesTax());
        if (order->grandTotal() != newOrder->grandTotal())
            order->setGrandTotal(newOrder->grandTotal());
        if (order->vatCharges() != newOrder->vatCharges())
            order->setVatCharges(newOrder->vatCharges());
        if (order->currencyCode() != newOrder->currencyCode())
            order->setCurrencyCode(newOrder->currencyCode());
        if (order->paymentCurrencyCode() != newOrder->paymentCurrencyCode())
            order->setPaymentCurrencyCode(newOrder->paymentCurrencyCode());
        if (order->itemCount() != newOrder->itemCount())
            order->setItemCount(newOrder->itemCount());
        if (order->lotCount() != newOrder->lotCount())
            order->setLotCount(newOrder->lotCount());
        if (order->status() != newOrder->status())
            order->setStatus(newOrder->status());
        if (order->paymentType() != newOrder->paymentType())
            order->setPaymentType(newOrder->paymentType());
        if (order->address() != newOrder->address())
            order->setAddress(newOrder->address());
        if (order->countryCode() != newOrder->countryCode())
            order->setCountryCode(newOrder->countryCode());

        order->setLots(newOrder->takeLots());

        newOrder.reset();

        if (order->address().isEmpty() && core()->isAuthenticated())
            startUpdateAddress(order);

        return;
    }
    appendOrderToModel(std::move(newOrder));  // not found -> add it
}

void Orders::appendOrderToModel(std::unique_ptr<Order> order)
{
    Order *o = order.release();
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

    if (o->address().isEmpty() && core()->isAuthenticated())
        startUpdateAddress(o);
    m_orders.append(o);

    endInsertRows();
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

    core()->retrieveAuthenticated(job);
}

QString Orders::parseAddress(OrderType type, const QByteArray &data)
{
    if (!data.isEmpty()) {
        QString s = QString::fromUtf8(data);
        QString a;

        QRegularExpression regExp(R"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Name & Address:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)"_l1);
        auto matches = regExp.globalMatch(s);
        if (type == OrderType::Placed) {
            // skip our own address
            if (matches.hasNext())
                matches.next();
        }
        if (matches.hasNext()) {
            QRegularExpressionMatch match = matches.next();
            a = match.captured(1);
            a.replace(QRegularExpression(R"(<[bB][rR] ?/?>)"_l1), "\n"_l1);
            return a;
        }
    }
    return { };
};

QSaveFile *Orders::orderSaveFile(QStringView fileName, OrderType type, const QDate &date)
{
    // Avoid huge directories with 1000s of entries.

    QString p = core()->dataPath() % "orders/"_l1
            % ((type == OrderType::Received) ? "received/"_l1: "placed/"_l1 )
            % QString::number(date.year()) % u'/'
            % QString("%1"_l1).arg(date.month(), 2, 10, '0'_l1) % u'/'
            % fileName;

    if (!QDir(fileName.isEmpty() ? p : p.left(p.size() - int(fileName.size()))).mkpath("."_l1))
        return nullptr;

    auto f = new QSaveFile(p);
    if (!f->open(QIODevice::WriteOnly)) {
        qWarning() << "Orders::orderSaveFile failed to open" << f->fileName()
                   << "for writing:" << f->errorString();
    }
    return f;
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
    Q_ASSERT(m_jobs.isEmpty());
    m_updateStatus = UpdateStatus::Updating;

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
        query.addQueryItem("getDetail"_l1,      "y"_l1);
        query.addQueryItem("getDateFormat"_l1, "0"_l1);    // MM/DD/YYYY
        url.setQuery(query);

        auto job = TransferJob::post(url, nullptr, true /* no redirects */);
        job->setUserData(type, true);
        m_jobs << job;

        core()->retrieveAuthenticated(job);
    }
}

void Orders::cancelUpdate()
{
    if (m_updateStatus == UpdateStatus::Updating)
        std::for_each(m_jobs.cbegin(), m_jobs.cend(), [](auto job) { job->abort(); });
    std::for_each(m_addressJobs.cbegin(), m_addressJobs.cend(), [](auto job) { job->abort(); });
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
        }
        else if (role == Qt::TextAlignmentRole) {
            return (section == Total) ? Qt::AlignRight : Qt::AlignLeft;
        }
    }
    return { };
}

} // namespace BrickLink

#include "moc_order.cpp"
