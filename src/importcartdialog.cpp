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
#include <QPushButton>
#include <QHeaderView>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QPalette>
#include <QVariant>
#include <QCache>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "bricklink.h"
#include "transfer.h"
#include "xmlhelpers.h"
#include "messagebox.h"
#include "exception.h"
#include "config.h"
#include "framework.h"
#include "documentio.h"
#include "humanreadabletimedelta.h"

#include "importcartdialog.h"


using namespace std::chrono_literals;


enum {
    CartPointerRole = Qt::UserRole + 1,
};


class CartModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit CartModel(QObject *parent)
        : QAbstractTableModel(parent)
    {
        MODELTEST_ATTACH(this)

        m_trans = new Transfer(this);
        connect(m_trans, &Transfer::finished,
                this, &CartModel::flagReceived);
    }

    ~CartModel() override
    {
        qDeleteAll(m_carts);
    }

    void setCarts(const QVector<BrickLink::Cart *> &carts)
    {
        beginResetModel();
        qDeleteAll(m_carts);
        m_carts = carts;
        sort(m_sortSection, m_sortOrder);
        endResetModel();
    }

    void updateCart(BrickLink::Cart *cart)
    {
        int r = m_carts.indexOf(cart);
        if (r >= 0)
            emit dataChanged(index(r, 0), index(r, columnCount() - 1));
    }

    int rowCount(const QModelIndex &parent = { }) const override
    {
        return parent.isValid() ? 0 : m_carts.size();
    }

    int columnCount(const QModelIndex &parent = { }) const override
    {
        return parent.isValid() ? 0 : 6;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || (index.row() < 0) || (index.row() >= m_carts.size()))
            return QVariant();

        const BrickLink::Cart *cart = m_carts.at(index.row());
        int col = index.column();

        if (role == Qt::DisplayRole) {
            switch (col) {
            case 0: return (cart->domestic())
                        ? ImportCartDialog::tr("Domestic") : ImportCartDialog::tr("International");
            case 1: return QLocale::system().toString(cart->lastUpdated(), QLocale::ShortFormat);
            case 2: return QLocale::system().toString(cart->itemCount());
            case 3: return QLocale::system().toString(cart->lotCount());
            case 4: return QString(cart->storeName() % u" (" % cart->sellerName() % u")");
            case 5: return Currency::toString(cart->cartTotal(), cart->currencyCode(), 2);
            }
        } else if (role == Qt::DecorationRole) {
            switch (col) {
            case 4:
                QImage *flag = nullptr;
                QString cc = cart->countryCode();
                if (cc.length() == 2)
                    flag = m_flags[cc];
                if (!flag) {
                    QString url = QString::fromLatin1("https://www.bricklink.com/images/flagsS/%1.gif").arg(cc);

                    TransferJob *job = TransferJob::get(url);
                    int userData = cc[0].unicode() | (cc[1].unicode() << 16);
                    job->setUserData<void>(userData, nullptr);
                    m_trans->retrieve(job);
                }
                if (flag)
                    return *flag;
                break;
            }
        } else if (role == Qt::TextAlignmentRole) {
            return (col == 5) ? Qt::AlignRight : Qt::AlignLeft;
        }
        else if (role == Qt::BackgroundRole) {
            if (col == 0) {
                QColor c(cart->domestic() ? Qt::green : Qt::blue);
                c.setAlphaF(0.2);
                return c;
            }
        }
        else if (role == CartPointerRole) {
            return QVariant::fromValue(cart);
        }

        return { };
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override
    {
        if (orient == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                case  0: return ImportCartDialog::tr("Type");
                case  1: return ImportCartDialog::tr("Last Update");
                case  2: return ImportCartDialog::tr("Items");
                case  3: return ImportCartDialog::tr("Lots");
                case  4: return ImportCartDialog::tr("Seller");
                case  5: return ImportCartDialog::tr("Total");
                }
            }
            else if (role == Qt::TextAlignmentRole) {
                return (section == 5) ? Qt::AlignRight : Qt::AlignLeft;
            }
        }
        return { };
    }

    void sort(int section, Qt::SortOrder so) override
    {
        emit layoutAboutToBeChanged();
        m_sortSection = section;
        m_sortOrder = so;

        std::sort(m_carts.begin(), m_carts.end(), [section, so](const auto &cp1, const auto &cp2) {
            int d = 0;

            BrickLink::Cart *c1 = cp1;
            BrickLink::Cart *c2 = cp2;

            switch (section) {
            case  0: d = int(c1->domestic()) - int(c2->domestic()); break;
            case  1: d = c1->lastUpdated().daysTo(c2->lastUpdated()); break;
            case  2: d = c1->itemCount() - c2->itemCount(); break;
            case  3: d = c1->lotCount() - c2->lotCount(); break;
            case  4: d = c1->storeName().compare(c2->storeName(), Qt::CaseInsensitive); break;
            case  5: d = qFuzzyCompare(c1->cartTotal(), c2->cartTotal())
                        ? 0
                        : ((c1->cartTotal() < c2->cartTotal()) ? -1 : 1); break;
            }

            // this predicate needs to establish a strict weak cart:
            // if you return true for (c1, c2), you are not allowed to also return true for (c2, c1)

            if (d == 0)
                return false;
            else
                return (so == Qt::AscendingOrder) ? (d < 0) : (d > 0);
        });
        emit layoutChanged();
    }

private slots:
    void flagReceived(TransferJob *j)
    {
        if (!j || !j->data())
            return;

        QPair<int, void *> ud = j->userData<void>();
        QString cc;
        cc.append(QChar(ud.first & 0x0000ffff));
        cc.append(QChar(ud.first >> 16));

        auto *img = new QImage;
        if (img->loadFromData(*j->data())) {
            m_flags.insert(cc, img);

            for (int r = 0; r < m_carts.size(); ++r) {
                if (m_carts.at(r)->countryCode() == cc) {
                    QModelIndex idx = index(r, 4, { });
                    emit dataChanged(idx, idx);
                }
            }
        } else {
            delete img;
        }
    }

private:
    QVector<BrickLink::Cart *> m_carts;
    QCache<QString, QImage> m_flags;
    Transfer *m_trans;
    int m_sortSection = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ImportCartDialog::ImportCartDialog(QWidget *parent)
    : QDialog(parent)
    , m_trans(new Transfer(this))
{
    setupUi(this);

    m_cartModel = new CartModel(this);
    w_carts->header()->setStretchLastSection(false);
    w_carts->setModel(m_cartModel);
    w_carts->header()->setSectionResizeMode(4, QHeaderView::Stretch);

    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, &ImportCartDialog::importCarts);

    connect(w_update, &QToolButton::clicked,
            this, &ImportCartDialog::updateCarts);
    connect(w_carts->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportCartDialog::checkSelected);
    connect(w_carts, &QTreeView::activated,
            this, &ImportCartDialog::activateItem);
    connect(m_trans, &Transfer::progress, this, [this](int done, int total) {
        w_progress->setVisible(done != total);
        w_progress->setMaximum(total);
        w_progress->setValue(done);
    });

    languageChange();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportCartDialog::updateStatusLabel);

    connect(m_trans, &Transfer::finished,
            this, &ImportCartDialog::downloadFinished);

    QMetaObject::invokeMethod(this, &ImportCartDialog::login, Qt::QueuedConnection);

    QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/ImportCartDialog/Geometry"))
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
}

ImportCartDialog::~ImportCartDialog()
{
    Config::inst()->setValue("/MainWindow/ImportCartDialog/Geometry", saveGeometry());
}

void ImportCartDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ImportCartDialog::languageChange()
{
    retranslateUi(this);

    w_import->setText(tr("Import"));
}

void ImportCartDialog::login()
{
    QUrl url("https://www.bricklink.com/ajax/renovate/loginandout.ajax");
    QUrlQuery q;
    q.addQueryItem("userid",   Config::inst()->loginForBrickLink().first);
    q.addQueryItem("password", Config::inst()->loginForBrickLink().second);
    q.addQueryItem("keepme_loggedin", "1");
    url.setQuery(q);

    auto job = TransferJob::post(url, nullptr, true /* no redirects */);
    job->setUserData<void>('l', nullptr);
    m_currentUpdate << job;

    m_trans->retrieve(job);
    updateStatusLabel();
}

void ImportCartDialog::updateCarts()
{
    if (!m_currentUpdate.isEmpty())
        return;
    if (!m_loggedIn)
        return;

    w_update->setEnabled(false);
    w_import->setEnabled(false);
    w_carts->setEnabled(false);

    QUrl url("https://www.bricklink.com/v2/globalcart.page");

    auto job = TransferJob::post(url, nullptr, true /* no redirects */);
    job->setUserData<void>('g', nullptr);
    m_currentUpdate << job;

    m_trans->retrieve(job);
    updateStatusLabel();
}

void ImportCartDialog::downloadFinished(TransferJob *job)
{
    int type = job->userData<void>().first; // g_lobal, c_art

    switch (type) {
    case 'l': {
        if (job->responseCode() == 200) {
            m_loggedIn = true;
            QMetaObject::invokeMethod(this, &ImportCartDialog::updateCarts, Qt::QueuedConnection);
        } else {
            MessageBox::warning(this, tr("Import BrickLink Cart"), tr("Could not login to BrickLink."));
        }
        break;
    }
    case 'g': {
        QVector<BrickLink::Cart *> carts;
        if (!job->data()->isEmpty() && (job->responseCode() == 200)) {
            try {
                int pos = job->data()->indexOf("var GlobalCart");
                if (pos < 0)
                    throw Exception("Invalid HTML - cannot parse");
                int startPos = job->data()->indexOf('{', pos);
                int endPos = job->data()->indexOf("};\r\n", pos);

                if ((startPos <= pos) || (endPos < startPos))
                    throw Exception("Invalid HTML - found GlobalCart, but could not parse line");
                auto globalCart = job->data()->mid(startPos, endPos - startPos + 1);
                QJsonParseError err;
                auto json = QJsonDocument::fromJson(globalCart, &err);
                if (json.isNull())
                    throw Exception("Invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

                const QJsonArray jsonCarts = json["domestic"].toObject()["stores"].toArray()
                        + json["international"].toObject()["stores"].toArray();
                for (auto &&v : jsonCarts) {
                    const QJsonObject jsonCart = v.toObject();

                    int sellerId = jsonCart["sellerID"].toInt();
                    int lots = jsonCart["totalLots"].toInt();
                    int items = jsonCart["totalItems"].toInt();
                    QString totalPrice = jsonCart["totalPriceNative"].toString();

                    qWarning() << sellerId << totalPrice << lots << items;

                    if (sellerId && !totalPrice.isEmpty() && lots && items) {
                        auto cart = new BrickLink::Cart;
                        cart->setSellerId(sellerId);
                        cart->setLotCount(lots);
                        cart->setItemCount(items);
                        cart->setCurrencyCode(totalPrice.left(3));
                        cart->setCartTotal(totalPrice.mid(4).toDouble());
                        cart->setDomestic(jsonCart["type"].toString() == QLatin1String("domestic"));
                        cart->setLastUpdated(QDate::fromString(jsonCart["lastUpdated"].toString(),
                                             QLatin1String("yyyy-MM-dd")));
                        cart->setSellerName(jsonCart["sellerName"].toString());
                        cart->setStoreName(jsonCart["storeName"].toString());
                        cart->setCountryCode(jsonCart["countryID"].toString());
                        carts << cart;
                    }
                }
            } catch (const Exception &e) {
                MessageBox::warning(this, { },
                                    tr("Could not parse the cart data") % u"<br><br>" %  e.error());
            }
        }

        m_cartModel->setCarts(carts);
        break;
    }
    case 'c': {
        auto cart = job->userData<BrickLink::Cart>('c');

        if (!job->data()->isEmpty() && (job->responseCode() == 200)) {
            BrickLink::InvItemList items;
            QString currencyCode;

            try {
                int invalidCount = 0;
                QJsonParseError err;
                auto json = QJsonDocument::fromJson(*job->data(), &err);
                if (json.isNull())
                    throw Exception("Invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

                const QJsonArray cartItems = json["cart"].toObject()["items"].toArray();
                for (auto &&v : cartItems) {
                    const QJsonObject cartItem = v.toObject();

                    QString itemId = cartItem["itemNo"].toString();
                    char itemTypeId = XmlHelpers::firstCharInString(cartItem["itemType"].toString());
                    uint colorId = cartItem["colorID"].toVariant().toUInt();
                    auto cond = (cartItem["invNew"].toString() == QLatin1String("New"))
                            ? BrickLink::Condition::New : BrickLink::Condition::Used;
                    int qty = cartItem["cartQty"].toInt();
                    QString priceStr = cartItem["nativePrice"].toString(); //TODO: which one?
                    double price = priceStr.mid(4).toDouble();
                    QString ccode = priceStr.left(3);

                    if (currencyCode.isEmpty()) {
                        currencyCode = ccode;
                    } else if (currencyCode != ccode) {
                        ++invalidCount;
                        continue;
                    }

                    auto item = BrickLink::core()->item(itemTypeId, itemId);
                    auto color = BrickLink::core()->color(colorId);

                    if (!item || !color) {
                        ++invalidCount;
                    } else {
                        auto *ii = new BrickLink::InvItem(color, item);
                        ii->setCondition(cond);
                        ii->setQuantity(qty);
                        ii->setPrice(price);

                        items << ii;
                    }
                }
                if (invalidCount)
                    throw Exception(tr("%1 lots of your Shopping Cart could not be imported.").arg(invalidCount));

            } catch (const Exception &e) {
                MessageBox::warning(this, { },
                                    tr("Could not parse the cart data") % u"<br><br>" %  e.error());
            }

            if (!items.isEmpty()) {
                if (auto doc = DocumentIO::importBrickLinkCart(cart, items))
                    FrameWork::inst()->createWindow(doc);
            }
        }
        m_cartDownloads.removeOne(job);
        break;
    }
    default:
        break;
    }

    m_currentUpdate.removeOne(job);
    if (m_currentUpdate.isEmpty()) {
        m_lastUpdated = QDateTime::currentDateTime();

        w_update->setEnabled(true);
        w_carts->setEnabled(true);

        updateStatusLabel();

        if (m_cartModel->rowCount()) {
            auto tl = w_carts->model()->index(0, 0);
            w_carts->selectionModel()->select(tl, QItemSelectionModel::SelectCurrent
                                               | QItemSelectionModel::Rows);
            w_carts->scrollTo(tl);
        }
        w_carts->header()->resizeSections(QHeaderView::ResizeToContents);
        w_carts->setFocus();

        checkSelected();
    }
}

void ImportCartDialog::importCarts()
{
    const auto selection = w_carts->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto cart = m_cartModel->data(idx, CartPointerRole).value<const BrickLink::Cart *>();

        QUrl url("https://www.bricklink.com/ajax/renovate/cart/getStoreCart.ajax");
        QUrlQuery query;
        query.addQueryItem("sid", QString::number(cart->sellerId()));
        url.setQuery(query);

        auto job = TransferJob::post(url, nullptr, true /* no redirects */);

        job->setUserData('c', new BrickLink::Cart(*cart));
        m_cartDownloads << job;

        m_trans->retrieve(job);
    }
}


void ImportCartDialog::checkSelected()
{
    w_import->setEnabled(w_carts->selectionModel()->hasSelection());
}

void ImportCartDialog::activateItem()
{
    checkSelected();
    w_import->animateClick();
}

void ImportCartDialog::updateStatusLabel()
{
    if (m_currentUpdate.isEmpty()) {
        w_lastUpdated->setText(tr("Last updated %1").arg(
                                   HumanReadableTimeDelta::toString(m_lastUpdated,
                                                                    QDateTime::currentDateTime())));
    } else {
        w_lastUpdated->setText(tr("Currently updating carts"));
    }
}

#include "importcartdialog.moc"
#include "moc_importcartdialog.cpp"
