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
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QPalette>
#include <QVariant>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QShortcut>
#include <QKeyEvent>

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
#include "utility.h"

#include "importcartdialog.h"


using namespace std::chrono_literals;


enum {
    CartPointerRole = Qt::UserRole + 1,
    CartFilterRole,
};


class CartModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit CartModel(QObject *parent)
        : QAbstractTableModel(parent)
    {
        MODELTEST_ATTACH(this)
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
            case 0: return QLocale::system().toString(cart->lastUpdated(), QLocale::ShortFormat);
            case 1: return (cart->domestic())
                        ? ImportCartDialog::tr("Domestic") : ImportCartDialog::tr("International");
            case 2: return QString(cart->storeName() % u" (" % cart->sellerName() % u")");
            case 3: return QLocale::system().toString(cart->itemCount());
            case 4: return QLocale::system().toString(cart->lotCount());
            case 5: return Currency::toString(cart->cartTotal(), cart->currencyCode(), 2);
            }
        } else if (role == Qt::DecorationRole) {
            switch (col) {
            case 2: {
                QIcon flag;
                QString cc = cart->countryCode();
                flag = m_flags.value(cc);
                if (flag.isNull()) {
                    flag.addFile(":/assets/flags/" + cc, { }, QIcon::Normal);
                    flag.addFile(":/assets/flags/" + cc, { }, QIcon::Selected);
                    m_flags.insert(cc, flag);
                }
                return flag;
            }
            }
        } else if (role == Qt::TextAlignmentRole) {
            return int(Qt::AlignVCenter) | int((col == 5) ? Qt::AlignRight : Qt::AlignLeft);
        } else if (role == Qt::BackgroundRole) {
            if (col == 1) {
                QColor c(cart->domestic() ? Qt::green : Qt::blue);
                c.setAlphaF(0.1);
                return c;
            }
        } else if (role == CartPointerRole) {
            return QVariant::fromValue(cart);
        } else if (role == CartFilterRole) {
            switch (col) {
            case  0: return cart->lastUpdated();
            case  1: return cart->domestic() ? 1 : 0;
            case  2: return cart->storeName();
            case  3: return cart->itemCount();
            case  4: return cart->lotCount();
            case  5: return cart->cartTotal();
            }
        }

        return { };
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override
    {
        if (orient == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                case  0: return ImportCartDialog::tr("Last Update");
                case  1: return ImportCartDialog::tr("Type");
                case  2: return ImportCartDialog::tr("Seller");
                case  3: return ImportCartDialog::tr("Items");
                case  4: return ImportCartDialog::tr("Lots");
                case  5: return ImportCartDialog::tr("Total");
                }
            }
            else if (role == Qt::TextAlignmentRole) {
                return (section == 5) ? Qt::AlignRight : Qt::AlignLeft;
            }
        }
        return { };
    }

private:
    QVector<BrickLink::Cart *> m_carts;
    mutable QHash<QString, QIcon> m_flags;
    int m_sortSection = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class CartDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    CartDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QStyledItemDelegate::sizeHint(option, index)
                + QSize { option.fontMetrics.height(), 0 };
    }
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
    auto proxyModel = new QSortFilterProxyModel(m_cartModel);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortLocaleAware(true);
    proxyModel->setSortRole(CartFilterRole);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);
    proxyModel->setSourceModel(m_cartModel);
    w_carts->setModel(proxyModel);
    w_carts->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    w_carts->setItemDelegate(new CartDelegate(w_carts));

    connect(w_filter, &HistoryLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { w_filter->setFocus(); });

    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, [this]() { importCarts(w_carts->selectionModel()->selectedRows()); });
    w_showOnBrickLink = new QPushButton();
    w_showOnBrickLink->setIcon(QIcon::fromTheme("bricklink"));
    w_buttons->addButton(w_showOnBrickLink, QDialogButtonBox::ActionRole);
    connect(w_showOnBrickLink, &QAbstractButton::clicked,
            this, &ImportCartDialog::showCartsOnBrickLink);

    connect(w_update, &QToolButton::clicked,
            this, &ImportCartDialog::updateCarts);
    connect(w_carts->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportCartDialog::checkSelected);
    connect(w_carts, &QTreeView::activated,
            this, [this]() { w_import->animateClick(); });
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
    t->start();

    connect(m_trans, &Transfer::finished,
            this, &ImportCartDialog::downloadFinished);

    QMetaObject::invokeMethod(this, &ImportCartDialog::login, Qt::QueuedConnection);

    QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/ImportCartDialog/Geometry"))
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
    ba = Config::inst()->value(QLatin1String("/MainWindow/ImportCartDialog/Filter")).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportCartDialog::~ImportCartDialog()
{
    Config::inst()->setValue("/MainWindow/ImportCartDialog/Geometry", saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportCartDialog/Filter", w_filter->saveState());
}

void ImportCartDialog::keyPressEvent(QKeyEvent *e)
{
    // simulate QDialog behavior
    if (e->matches(QKeySequence::Cancel)) {
        reject();
        return;
    } else if ((!e->modifiers() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))
               || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter))) {
        // we need the animateClick here instead of triggering directly: otherwise we
        // get interference from the QTreeView::activated signal, resulting in double triggers
        if (w_import->isVisible() && w_import->isEnabled())
            w_import->animateClick();
        return;
    }

    QWidget::keyPressEvent(e);
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
    w_filter->setToolTip(Utility::toolTipLabel(tr("Filter the list for lines containing these words"),
                                               QKeySequence::Find, w_filter->instructionToolTip()));
    w_showOnBrickLink->setText(tr("Show"));
    w_showOnBrickLink->setToolTip(tr("Show on BrickLink"));
    updateStatusLabel();
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
    int type = job->userData<void>().first; // l_ogin, g_lobal, c_art

    switch (type) {
    case 'l': {
        if (job->responseCode() == 200) {
            m_loggedIn = true;
            QMetaObject::invokeMethod(this, &ImportCartDialog::updateCarts, Qt::QueuedConnection);
        } else {
            MessageBox::warning(this, tr("Import BrickLink Cart"), tr("Could not login to BrickLink."));
        }
        m_currentUpdate.removeOne(job);
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

                const QJsonArray domesticCarts = json["domestic"].toObject()["stores"].toArray();
                const QJsonArray internationalCarts = json["international"].toObject()["stores"].toArray();
                QVector<QJsonObject> jsonCarts;
                for (auto &&v : domesticCarts)
                    jsonCarts << v.toObject();
                for (auto &&v : internationalCarts)
                    jsonCarts << v.toObject();

                for (auto &&jsonCart : jsonCarts) {
                    int sellerId = jsonCart["sellerID"].toInt();
                    int lots = jsonCart["totalLots"].toInt();
                    int items = jsonCart["totalItems"].toInt();
                    QString totalPrice = jsonCart["totalPriceNative"].toString();

                    if (sellerId && !totalPrice.isEmpty() && lots && items) {
                        auto cart = new BrickLink::Cart;
                        cart->setSellerId(sellerId);
                        cart->setLotCount(lots);
                        cart->setItemCount(items);
                        if (totalPrice.mid(2, 2) == " $") // why does if have to be different?
                            cart->setCurrencyCode(totalPrice.left(2) + QChar('D'));
                        else
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

        break;
    }
    case 'c': {
        auto cart = job->userData<BrickLink::Cart>('c');
        QLocale en_US("en_US");

        if (!job->data()->isEmpty() && (job->responseCode() == 200)) {
            BrickLink::InvItemList items;

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
                    int itemSeq = cartItem["itemSeq"].toInt();
                    char itemTypeId = XmlHelpers::firstCharInString(cartItem["itemType"].toString());
                    uint colorId = cartItem["colorID"].toVariant().toUInt();
                    auto cond = (cartItem["invNew"].toString() == QLatin1String("New"))
                            ? BrickLink::Condition::New : BrickLink::Condition::Used;
                    int qty = cartItem["cartQty"].toInt();
                    QString priceStr = cartItem["nativePrice"].toString();
                    double price = en_US.toDouble(priceStr.mid(4));

                    if (itemSeq)
                        itemId = itemId % u'-' % QString::number(itemSeq);

                    auto item = BrickLink::core()->item(itemTypeId, itemId);
                    auto color = BrickLink::core()->color(colorId);
                    if (!item || !color) {
                        ++invalidCount;
                    } else {
                        auto *ii = new BrickLink::InvItem(color, item);
                        ii->setCondition(cond);

                        if (ii->itemType()->hasSubConditions()) {
                            QString scond = cartItem["invComplete"].toString();
                            if (scond == QLatin1String("Complete"))
                                ii->setSubCondition(BrickLink::SubCondition::Complete);
                            if (scond == QLatin1String("Incomplete"))
                                ii->setSubCondition(BrickLink::SubCondition::Incomplete);
                            if (scond == QLatin1String("Sealed"))
                                ii->setSubCondition(BrickLink::SubCondition::Sealed);
                        }

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
}

void ImportCartDialog::importCarts(const QModelIndexList &rows)
{
    for (auto idx : rows) {
        auto cart = idx.data(CartPointerRole).value<const BrickLink::Cart *>();

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

void ImportCartDialog::showCartsOnBrickLink()
{
    const auto selection = w_carts->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto cart = idx.data(CartPointerRole).value<const BrickLink::Cart *>();
        int sellerId = cart->sellerId();
        BrickLink::core()->openUrl(BrickLink::URL_ShoppingCart, &sellerId);
    }
}

void ImportCartDialog::checkSelected()
{
    bool b = w_carts->selectionModel()->hasSelection();
    w_import->setEnabled(b);
    w_showOnBrickLink->setEnabled(b);
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
