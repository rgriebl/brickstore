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

#include "bricklink/cart.h"
#include "bricklink/core.h"
#include "bricklink/io.h"

#include "utility/currency.h"
#include "utility/exception.h"
#include "utility/transfer.h"
#include "utility/utility.h"
#include "utility/xmlhelpers.h"

namespace BrickLink {

class CartPrivate
{
public:
    ~CartPrivate() { qDeleteAll(m_lots); }

private:
    bool      m_domestic = false;
    int       m_sellerId;
    QString   m_sellerName;
    QString   m_storeName;
    QDateTime m_lastUpdated;
    double    m_total = 0;
    QString   m_currencyCode;
    int       m_itemCount = 0;
    int       m_lotCount = 0;
    QString   m_countryCode;

    LotList   m_lots;

    friend class Cart;
};


Cart::Cart()
    : QObject()
    , d(new CartPrivate)
{ }

const LotList &Cart::lots() const
{
    return d->m_lots;
}

bool Cart::domestic() const
{
    return d->m_domestic;
}

int Cart::sellerId() const
{
    return d->m_sellerId;
}

QString Cart::sellerName() const
{
    return d->m_sellerName;
}

QString Cart::storeName() const
{
    return d->m_storeName;
}

QDate Cart::lastUpdated() const
{
    return d->m_lastUpdated.date();
}

double Cart::total() const
{
    return d->m_total;
}

QString Cart::currencyCode() const
{
    return d->m_currencyCode;
}

int Cart::itemCount() const
{
    return d->m_itemCount;
}

int Cart::lotCount() const
{
    return d->m_lotCount;
}

QString Cart::countryCode() const
{
    return d->m_countryCode;
}

void Cart::setLots(LotList &&lots)
{
    if (d->m_lots != lots) {
        qDeleteAll(d->m_lots);
        d->m_lots = lots;
        lots.clear();
        emit lotsChanged(d->m_lots);
    }
}

void Cart::setDomestic(bool domestic)
{
    if (d->m_domestic != domestic) {
        d->m_domestic = domestic;
        emit domesticChanged(domestic);
    }
}

void Cart::setSellerId(int id)
{
    if (d->m_sellerId != id) {
        d->m_sellerId = id;
        emit sellerIdChanged(id);
    }
}

void Cart::setSellerName(const QString &name)
{
    if (d->m_sellerName != name) {
        d->m_sellerName = name;
        emit sellerNameChanged(name);
    }
}

void Cart::setStoreName(const QString &name)
{
    if (d->m_storeName != name) {
        d->m_storeName = name;
        emit storeNameChanged(name);
    }
}

void Cart::setLastUpdated(const QDate &dt)
{
    if (d->m_lastUpdated.date() != dt) {
        d->m_lastUpdated.setDate(dt);
        emit lastUpdatedChanged(dt);
    }
}

void Cart::setTotal(double m)
{
    if (!qFuzzyCompare(d->m_total, m)) {
        d->m_total = m;
        emit totalChanged(m);
    }
}

void Cart::setCurrencyCode(const QString &str)
{
    if (d->m_currencyCode != str) {
        d->m_currencyCode = str;
        emit currencyCodeChanged(str);
    }
}

void Cart::setItemCount(int i)
{
    if (d->m_itemCount != i) {
        d->m_itemCount = i;
        emit itemCountChanged(i);
    }
}

void Cart::setLotCount(int i)
{
    if (d->m_lotCount != i) {
        d->m_lotCount = i;
        emit lotCountChanged(i);
    }
}

void Cart::setCountryCode(const QString &str)
{
    if (d->m_countryCode != str) {
        d->m_countryCode = str;
        emit countryCodeChanged(str);
    }
}



Carts::Carts(Core *core)
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

        if (m_cartJobs.contains(job) && (type == "cart")) {
            m_cartJobs.removeOne(job);

            bool success = jobCompleted;
            int sid = job->userData(type).toInt();
            QString message = tr("Failed to import cart %1").arg(sid);

            auto it = std::find_if(m_carts.cbegin(), m_carts.cend(), [sid](const Cart *cart) {
                return cart->sellerId() == sid;
            });
            if (it == m_carts.cend()) {
                qWarning() << "Received cart data for an unknown cart:" << sid;
                return;
            }

            try {
                if (!jobCompleted) {
                    throw Exception(message % u": " % job->errorString());
                } else {
                    int invalidCount = parseSellerCart(*it, *job->data());
                    if (invalidCount) {
                        message = tr("%n lot(s) of your Shopping Cart could not be imported.",
                                     nullptr, invalidCount);
                    }
                    message.clear();
                }
            } catch (const Exception &e) {
                success = false;
                message = message % u": " % e.error();
            }
            emit fetchLotsFinished(*it, success, message);

        } else if ((job == m_job) && (type == "globalCart")) {
            bool success = jobCompleted;
            QString message = tr("Failed to import the carts");
            if (success) {
                try {
                    beginResetModel();
                    qDeleteAll(m_carts);
                    m_carts.clear();
                    const auto carts = parseGlobalCart(*job->data());
                    for (auto &cart : carts) {
                        int row = int(m_carts.count());
                        m_carts.append(cart);
                        cart->setParent(this); // needed to prevent QML from taking ownership

                        connect(cart, &Cart::storeNameChanged, this, [this, row]() { emitDataChanged(row, Store); });
                        connect(cart, &Cart::sellerNameChanged, this, [this, row]() { emitDataChanged(row, Store); });
                        connect(cart, &Cart::lastUpdatedChanged, this, [this, row]() { emitDataChanged(row, Date); });
                        connect(cart, &Cart::domesticChanged, this, [this, row]() { emitDataChanged(row, Type); });
                        connect(cart, &Cart::itemCountChanged, this, [this, row]() { emitDataChanged(row, ItemCount); });
                        connect(cart, &Cart::lotCountChanged, this, [this, row]() { emitDataChanged(row, LotCount); });
                        connect(cart, &Cart::totalChanged, this, [this, row]() { emitDataChanged(row, Total); });
                        connect(cart, &Cart::currencyCodeChanged, this, [this, row]() { emitDataChanged(row, Total); });
                    }
                    endResetModel();
                    message.clear();

                } catch (const Exception &e) {
                    success = false;
                    message = message % u": " % e.error();
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
        qDeleteAll(m_carts);
        m_carts.clear();
        endResetModel();
    });
}

int Carts::parseSellerCart(Cart *cart, const QByteArray &data)
{
    QLocale en_US("en_US"_l1);
    LotList lots;

    int invalidCount = 0;
    QJsonParseError err;
    auto json = QJsonDocument::fromJson(data, &err);
    if (json.isNull())
        throw Exception("Invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

    const QJsonArray cartItems = json["cart"_l1].toObject()["items"_l1].toArray();
    for (auto &&v : cartItems) {
        const QJsonObject cartItem = v.toObject();

        QByteArray itemId = cartItem["itemNo"_l1].toString().toLatin1();
        int itemSeq = cartItem["itemSeq"_l1].toInt();
        char itemTypeId = XmlHelpers::firstCharInString(cartItem["itemType"_l1].toString());
        uint colorId = cartItem["colorID"_l1].toVariant().toUInt();
        auto cond = (cartItem["invNew"_l1].toString() == "New"_l1)
                ? BrickLink::Condition::New : BrickLink::Condition::Used;
        int qty = cartItem["cartQty"_l1].toInt();
        QString priceStr = cartItem["nativePrice"_l1].toString();
        double price = en_US.toDouble(priceStr.mid(4));

        if (itemSeq)
            itemId = itemId % '-' % QByteArray::number(itemSeq);

        auto item = m_core->item(itemTypeId, itemId);
        auto color = m_core->color(colorId);
        if (!item || !color) {
            ++invalidCount;
        } else {
            auto *lot = new Lot(color, item);
            lot->setCondition(cond);

            if (lot->itemType()->hasSubConditions()) {
                QString scond = cartItem["invComplete"_l1].toString();
                if (scond == "Complete"_l1)
                    lot->setSubCondition(BrickLink::SubCondition::Complete);
                if (scond == "Incomplete"_l1)
                    lot->setSubCondition(BrickLink::SubCondition::Incomplete);
                if (scond == "Sealed"_l1)
                    lot->setSubCondition(BrickLink::SubCondition::Sealed);
            }

            lot->setQuantity(qty);
            lot->setPrice(price);

            lots << lot;
        }
    }
    cart->setLots(std::move(lots));
    return invalidCount;
}

QVector<BrickLink::Cart *> Carts::parseGlobalCart(const QByteArray &data)
{
    QVector<BrickLink::Cart *> carts;
    auto pos = data.indexOf("var GlobalCart");
    if (pos < 0)
        throw Exception("Invalid HTML - cannot parse");
    auto startPos = data.indexOf('{', pos);
    auto endPos = data.indexOf("};\r\n", pos);

    if ((startPos <= pos) || (endPos < startPos))
        throw Exception("Invalid HTML - found GlobalCart, but could not parse line");
    auto globalCart = data.mid(startPos, endPos - startPos + 1);
    QJsonParseError err;
    auto json = QJsonDocument::fromJson(globalCart, &err);
    if (json.isNull())
        throw Exception("Invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

    const QJsonArray domesticCarts = json["domestic"_l1].toObject()["stores"_l1].toArray();
    const QJsonArray internationalCarts = json["international"_l1].toObject()["stores"_l1].toArray();
    QVector<QJsonObject> jsonCarts;
    for (auto &&v : domesticCarts)
        jsonCarts << v.toObject();
    for (auto &&v : internationalCarts)
        jsonCarts << v.toObject();

    for (auto &&jsonCart : jsonCarts) {
        int sellerId = jsonCart["sellerID"_l1].toInt();
        int lots = jsonCart["totalLots"_l1].toInt();
        int items = jsonCart["totalItems"_l1].toInt();
        QString totalPrice = jsonCart["totalPriceNative"_l1].toString();

        if (sellerId && !totalPrice.isEmpty() && lots && items) {
            auto cart = new BrickLink::Cart;
            cart->setSellerId(sellerId);
            cart->setLotCount(lots);
            cart->setItemCount(items);
            if (totalPrice.mid(2, 2) == " $"_l1) // why does if have to be different?
                cart->setCurrencyCode(totalPrice.left(2) + 'D'_l1);
            else
                cart->setCurrencyCode(totalPrice.left(3));
            cart->setTotal(totalPrice.mid(4).toDouble());
            cart->setDomestic(jsonCart["type"_l1].toString() == "domestic"_l1);
            cart->setLastUpdated(QDate::fromString(jsonCart["lastUpdated"_l1].toString(),
                                 "yyyy-MM-dd"_l1));
            cart->setSellerName(jsonCart["sellerName"_l1].toString());
            cart->setStoreName(jsonCart["storeName"_l1].toString());
            cart->setCountryCode(jsonCart["countryID"_l1].toString());

            carts << cart;
        }
    }
    return carts;
}


void Carts::emitDataChanged(int row, int col)
{
    QModelIndex from = index(row, col < 0 ? 0 : col);
    QModelIndex to = index(row, col < 0 ? columnCount() - 1 : col);
    emit dataChanged(from, to);
}

void Carts::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}


void Carts::startUpdate()
{
    if (updateStatus() == UpdateStatus::Updating)
        return;
    Q_ASSERT(!m_job);
    setUpdateStatus(UpdateStatus::Updating);

    QUrl url("https://www.bricklink.com/v2/globalcart.page"_l1);

    auto job = TransferJob::post(url);
    job->setUserData("globalCart", true);
    m_job = job;

    m_core->retrieveAuthenticated(m_job);
}

void Carts::cancelUpdate()
{
    if ((m_updateStatus == UpdateStatus::Updating) && m_job)
        m_job->abort();
}

void Carts::startFetchLots(Cart *cart)
{
    if (!cart)
        return;

    QUrl url("https://www.bricklink.com/ajax/renovate/cart/getStoreCart.ajax"_l1);
    QUrlQuery query;
    query.addQueryItem("sid"_l1, Utility::urlQueryEscape(QString::number(cart->sellerId())));
    url.setQuery(query);

    auto job = TransferJob::post(url);

    job->setUserData("cart", QVariant::fromValue(cart->sellerId()));
    m_cartJobs << job;

    m_core->retrieveAuthenticated(job);
}

QVector<Cart *> Carts::carts() const
{
    return m_carts;
}

int Carts::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_carts.count());
}

int Carts::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant Carts::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= m_carts.size()))
        return QVariant();

    Cart *cart = m_carts.at(index.row());
    int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
        case Date: return QLocale::system().toString(cart->lastUpdated(), QLocale::ShortFormat);
        case Type: return cart->domestic() ? tr("Domestic") : tr("International");
        case Store: return QString(cart->storeName() % u" (" % cart->sellerName() % u")");
        case ItemCount: return QLocale::system().toString(cart->itemCount());
        case LotCount: return QLocale::system().toString(cart->lotCount());
        case Total: return Currency::toDisplayString(cart->total(), cart->currencyCode(), 2);
        }
    } else if (role == Qt::DecorationRole) {
        switch (col) {
        case Store: {
            QIcon flag;
            QString cc = cart->countryCode();
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
            QColor c(cart->domestic() ? Qt::green : Qt::blue);
            c.setAlphaF(0.1f);
            return c;
        }
    } else if (role == CartPointerRole) {
        return QVariant::fromValue(cart);
    } else if (role == CartSortRole) {
        switch (col) {
        case Date:      return cart->lastUpdated();
        case Type:      return cart->domestic() ? 1 : 0;
        case Store:     return cart->storeName();
        case ItemCount: return cart->itemCount();
        case LotCount:  return cart->lotCount();
        case Total:     return cart->total();
        }
    } else if (role == LastUpdatedRole) {
        return cart->lastUpdated();
    } else if (role == DomesticRole) {
        return cart->domestic();
    }

    return { };
}

QVariant Carts::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
            case Date:      return tr("Last Update");
            case Type:      return tr("Type");
            case Store:     return tr("Seller");
            case ItemCount: return tr("Items");
            case LotCount:  return tr("Lots");
            case Total:     return tr("Total");
            }
        } else if (role == Qt::TextAlignmentRole) {
            return (section == Total) ? Qt::AlignRight : Qt::AlignLeft;
        }
    }
    return { };
}

QHash<int, QByteArray> Carts::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "display" },
        { Qt::TextAlignmentRole, "textAlignment" },
        { Qt::DecorationRole, "decoration" },
        { Qt::BackgroundRole, "background" },
        { CartPointerRole, "cart" },
        { LastUpdatedRole, "lastUpdated" },
        { DomesticRole, "domestic" },
    };
    return roles;
}

} // namespace BrickLink

#include "moc_cart.cpp"
