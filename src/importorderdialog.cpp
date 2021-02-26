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
#include <QHash>
#include <QBuffer>
#include <QUrl>
#include <QUrlQuery>
#include <QShortcut>

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "bricklink.h"
#include "transfer.h"
#include "messagebox.h"
#include "xmlhelpers.h"
#include "exception.h"
#include "config.h"
#include "framework.h"
#include "documentio.h"
#include "humanreadabletimedelta.h"
#include "utility.h"

#include "importorderdialog.h"


using namespace std::chrono_literals;


enum {
    OrderPointerRole = Qt::UserRole + 1,
    OrderFilterRole,
};


static QDate mdy2date(const QString &mdy)
{
    QDate d;
    QStringList sl = mdy.split(QLatin1Char('/'));
    d.setDate(sl[2].toInt(), sl[0].toInt(), sl[1].toInt());
    return d;
}


class OrderModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit OrderModel(QObject *parent)
        : QAbstractTableModel(parent)
    {
        MODELTEST_ATTACH(this)
    }

    ~OrderModel() override
    {
        qDeleteAll(m_orders);
    }

    void setOrders(const QVector<BrickLink::Order *> &orders, BrickLink::OrderType type)
    {
        beginResetModel();
        for (auto it = m_orders.begin(); it != m_orders.end(); ) {
            if ((*it)->type() == type) {
                delete *it;
                it = m_orders.erase(it);
            } else {
                ++it;
            }
        }
        m_orders.append(orders);
        sort(m_sortSection, m_sortOrder);
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = { }) const override
    {
        return parent.isValid() ? 0 : m_orders.size();
    }

    int columnCount(const QModelIndex &parent = { }) const override
    {
        return parent.isValid() ? 0 : 7;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || (index.row() < 0) || (index.row() >= m_orders.size()))
            return QVariant();

        const BrickLink::Order *order = m_orders.at(index.row());
        int col = index.column();

        if (role == Qt::DisplayRole) {
            switch (col) {
            case 0: return QLocale::system().toString(order->date(), QLocale::ShortFormat);
            case 1: return (order->type() == BrickLink::OrderType::Received)
                        ? ImportOrderDialog::tr("Received") : ImportOrderDialog::tr("Placed");
            case 2: return order->id();
            case 3: {
                int firstline = order->address().indexOf('\n');
                if (firstline > 0) {
                    return QString::fromLatin1("%2 (%1)")
                            .arg(order->address().left(firstline), order->otherParty());
                }
                return order->otherParty();
            }
            case 4: return QLocale::system().toString(order->itemCount());
            case 5: return QLocale::system().toString(order->lotCount());
            case 6: return Currency::toString(order->grandTotal(), order->currencyCode(), 2);
            }
        } else if (role == Qt::DecorationRole) {
            switch (col) {
            case 3: {
                QIcon flag;
                QString cc = order->countryCode();
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
            return int(Qt::AlignVCenter) | int((col == 6) ? Qt::AlignRight : Qt::AlignLeft);
        } else if (role == Qt::BackgroundRole) {
            if (col == 1) {
                QColor c((order->type() == BrickLink::OrderType::Received) ? Qt::green : Qt::blue);
                c.setAlphaF(0.1);
                return c;
            }
        } else if (role == Qt::ToolTipRole) {
            QString tt = data(index, Qt::DisplayRole).toString();

            if (!order->address().isEmpty())
                tt = tt + QLatin1String("\n\n") + order->address();
            return tt;
        } else if (role == OrderPointerRole) {
            return QVariant::fromValue(order);
        } else if (role == OrderFilterRole) {
            switch (col) {
            case  0: return order->date();
            case  1: return int(order->type());
            case  2: return order->id();
            case  3: return order->otherParty();
            case  4: return order->itemCount();
            case  5: return order->lotCount();
            case  6: return order->grandTotal();
            }
        }

        return { };
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override
    {
        if (orient == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                case  0: return ImportOrderDialog::tr("Date");
                case  1: return ImportOrderDialog::tr("Type");
                case  2: return ImportOrderDialog::tr("Order ID");
                case  3: return ImportOrderDialog::tr("Buyer/Seller");
                case  4: return ImportOrderDialog::tr("Items");
                case  5: return ImportOrderDialog::tr("Lots");
                case  6: return ImportOrderDialog::tr("Total");
                }
            }
            else if (role == Qt::TextAlignmentRole) {
                return (section == 6) ? Qt::AlignRight : Qt::AlignLeft;
            }
        }
        return { };
    }

private:
    QVector<BrickLink::Order *> m_orders;
    mutable QHash<QString, QIcon> m_flags;
    int m_sortSection = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class OrderDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    OrderDelegate(QObject *parent = nullptr)
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


ImportOrderDialog::ImportOrderDialog(QWidget *parent)
    : QDialog(parent)
    , m_trans(new Transfer(this))
{
    setupUi(this);

    m_orderModel = new OrderModel(this);
    w_orders->header()->setStretchLastSection(false);
    auto proxyModel = new QSortFilterProxyModel(m_orderModel);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortLocaleAware(true);
    proxyModel->setSortRole(OrderFilterRole);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);
    proxyModel->setSourceModel(m_orderModel);
    w_orders->setModel(proxyModel);
    w_orders->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    w_orders->setItemDelegate(new OrderDelegate(w_orders));

    connect(w_filter, &HistoryLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { w_filter->setFocus(); });

    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, &ImportOrderDialog::importOrders);
    w_showOnBrickLink = new QPushButton();
    w_showOnBrickLink->setIcon(QIcon::fromTheme("bricklink"));
    w_buttons->addButton(w_showOnBrickLink, QDialogButtonBox::ActionRole);
    connect(w_showOnBrickLink, &QAbstractButton::clicked,
            this, &ImportOrderDialog::showOrdersOnBrickLink);

    connect(w_update, &QToolButton::clicked,
            this, &ImportOrderDialog::updateOrders);
    connect(w_orders->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportOrderDialog::checkSelected);
    connect(w_orders, &QTreeView::activated,
            this, &ImportOrderDialog::activateItem);
    connect(m_trans, &Transfer::progress, this, [this](int done, int total) {
        w_progress->setVisible(done != total);
        w_progress->setMaximum(total);
        w_progress->setValue(done);
    });

    languageChange();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportOrderDialog::updateStatusLabel);
    t->start();

    connect(m_trans, &Transfer::finished,
            this, &ImportOrderDialog::downloadFinished);

    QMetaObject::invokeMethod(this, &ImportOrderDialog::updateOrders, Qt::QueuedConnection);

    QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/ImportOrderDialog/Geometry"))
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
    int daysBack = Config::inst()->value("/MainWindow/ImportOrderDialog/DaysBack", -1).toInt();
    if (daysBack > 0)
        w_daysBack->setValue(daysBack);
    ba = Config::inst()->value(QLatin1String("/MainWindow/ImportOrderDialog/Filter")).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportOrderDialog::~ImportOrderDialog()
{
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Geometry", saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/DaysBack", w_daysBack->value());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Filter", w_filter->saveState());
}

void ImportOrderDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ImportOrderDialog::languageChange()
{
    retranslateUi(this);

    w_import->setText(tr("Import"));
    w_filter->setToolTip(Utility::toolTipLabel(tr("Filter the list for lines containing these words"),
                                               QKeySequence::Find, w_filter->instructionToolTip()));
    w_showOnBrickLink->setToolTip(tr("Show on BrickLink"));
    updateStatusLabel();
}

void ImportOrderDialog::updateOrders()
{
    if (!m_currentUpdate.isEmpty())
        return;
    w_update->setEnabled(false);
    w_import->setEnabled(false);
    w_orders->setEnabled(false);

    QDate today = QDate::currentDate();
    QDate fromDate = today.addDays(-w_daysBack->value());

    static const char *types[] = { "received", "placed" };
    for (auto &type : types) {
        QUrl url("https://www.bricklink.com/orderExcelFinal.asp");
        QUrlQuery query;
        query.addQueryItem("action",        "save");
        query.addQueryItem("orderType",     type);
        query.addQueryItem("viewType",      "X");    // XML - this has to go last, otherwise we get HTML
        query.addQueryItem("getOrders",     "date");
        query.addQueryItem("fMM",           QString::number(fromDate.month()));
        query.addQueryItem("fDD",           QString::number(fromDate.day()));
        query.addQueryItem("fYY",           QString::number(fromDate.year()));
        query.addQueryItem("tMM",           QString::number(today.month()));
        query.addQueryItem("tDD",           QString::number(today.day()));
        query.addQueryItem("tYY",           QString::number(today.year()));
        query.addQueryItem("getStatusSel",  "I");
        query.addQueryItem("getDateFormat", "0");    // MM/DD/YYYY
        query.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
        query.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);
        url.setQuery(query);

        auto job = TransferJob::post(url, nullptr, true /* no redirects */);
        job->setUserData<void>(type[0], nullptr);
        m_currentUpdate << job;

        m_trans->retrieve(job);
    }
    updateStatusLabel();
}

void ImportOrderDialog::downloadFinished(TransferJob *job)
{
    int type = job->userData<void>().first; // r_eceived, p_laced, a_ddress

    switch (type) {
    case 'a': {
        auto order = job->userData<BrickLink::Order>('a');

        if (!job->data()->isEmpty()) {
            QString s = QString::fromUtf8(*job->data());
            QString a;

            QRegularExpression regExp(R"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Name & Address:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)");
            auto matches = regExp.globalMatch(s);
            if (matches.hasNext()) {
                matches.next();
                if (matches.hasNext()) { // skip our own address
                    QRegularExpressionMatch match = matches.next();
                    a = match.captured(1);
                    a.replace(QRegularExpression(R"(<[bB][rR] ?/?>)"), QLatin1String("\n"));
                    order->setAddress(a);
                }
            }
        }
        if (order->address().isEmpty())
            order->setAddress(tr("Address not available"));

        orderDownloadFinished(order, job);
        break;
    }
    case 'r':
    case 'p': {
        QVector<BrickLink::Order *> orders;

        if (!job->data()->isEmpty() && (job->responseCode() == 200)) {
            auto buf = new QBuffer(job->data());
            buf->open(QIODevice::ReadOnly);
            try {
                XmlHelpers::ParseXML p(buf, "ORDERS", "ORDER");
                p.parse([&p, &type, &orders](QDomElement e) {
                    auto id = p.elementText(e, "ORDERID");

                    if (id.isEmpty())
                        throw("Order without ORDERID");

                    auto order = new BrickLink::Order(id, (type == 'r')
                                                      ? BrickLink::OrderType::Received
                                                      : BrickLink::OrderType::Placed);

                    order->setDate(mdy2date(p.elementText(e, "ORDERDATE")));
                    order->setStatusChange(mdy2date(p.elementText(e, "ORDERSTATUSCHANGED")));
                    order->setOtherParty(p.elementText(e, (type == 'r') ? "BUYER" : "SELLER"));
                    order->setShipping(p.elementText(e, "ORDERSHIPPING").toDouble());
                    order->setInsurance(p.elementText(e, "ORDERINSURANCE").toDouble());
                    order->setAdditionalCharges1(p.elementText(e, "ORDERADDCHRG1").toDouble());
                    order->setAdditionalCharges2(p.elementText(e, "ORDERADDCHRG2").toDouble());
                    order->setCredit(p.elementText(e, "ORDERCREDIT", "0").toDouble());
                    order->setCreditCoupon(p.elementText(e, "ORDERCREDITCOUPON", "0").toDouble());
                    order->setOrderTotal(p.elementText(e, "ORDERTOTAL").toDouble());
                    order->setSalesTax(p.elementText(e, "ORDERSALESTAX", "0").toDouble());
                    order->setGrandTotal(p.elementText(e, "BASEGRANDTOTAL").toDouble());
                    order->setVatCharges(p.elementText(e, "VATCHARGES", "0").toDouble());
                    order->setCurrencyCode(p.elementText(e, "BASECURRENCYCODE"));
                    order->setPaymentCurrencyCode(p.elementText(e, "PAYCURRENCYCODE"));
                    order->setItemCount(p.elementText(e, "ORDERITEMS").toInt());
                    order->setLotCount(p.elementText(e, "ORDERLOTS").toInt());
                    order->setStatus(p.elementText(e, "ORDERSTATUS"));
                    order->setPaymentType(p.elementText(e, "PAYMENTTYPE", ""));
                    order->setTrackingNumber(p.elementText(e, "ORDERTRACKNO", ""));
                    auto location = p.elementText(e, "LOCATION");
                    if (!location.isEmpty())
                        order->setCountryName(location.section(QLatin1String(", "), 0, 0));

                    orders << order;
                });
            } catch (const Exception &e) {
                MessageBox::warning(this, { },
                                    tr("Could not parse the receive order XML data") % u"<br><br>" %  e.error());
            }
        }

        m_orderModel->setOrders(orders, type == 'r' ? BrickLink::OrderType::Received
                                                    : BrickLink::OrderType::Placed);

        break;
    }
    case 'o': {
        auto order = job->userData<BrickLink::Order>('o');
        orderDownloadFinished(order, job, *job->data());
        break;
    }
    default:
        break;
    }

    m_currentUpdate.removeOne(job);
    if (m_currentUpdate.isEmpty()) {
        m_lastUpdated = QDateTime::currentDateTime();

        w_update->setEnabled(true);
        w_orders->setEnabled(true);

        updateStatusLabel();

        if (m_orderModel->rowCount()) {
            auto tl = w_orders->model()->index(0, 0);
            w_orders->selectionModel()->select(tl, QItemSelectionModel::SelectCurrent
                                               | QItemSelectionModel::Rows);
            w_orders->scrollTo(tl);
        }
        w_orders->header()->resizeSections(QHeaderView::ResizeToContents);
        w_orders->setFocus();

        checkSelected();
    }
}

void ImportOrderDialog::orderDownloadFinished(BrickLink::Order *order, TransferJob *job,
                                              const QByteArray &xml)
{
    for (auto it = m_orderDownloads.begin(); it != m_orderDownloads.end(); ++it) {
        if (it->m_order != order)
            continue;

        if (it->m_addressJob == job) {
            it->m_addressJob = nullptr;
        } else if (it->m_xmlJob == job) {
            it->m_xmlJob = nullptr;
            it->m_xmlData = xml;
        }
        if (!it->m_xmlJob && !it->m_addressJob) {
            if (auto doc = DocumentIO::importBrickLinkOrder(order, it->m_xmlData))
                FrameWork::inst()->createWindow(doc);
            m_orderDownloads.erase(it);
            break;
        }
    }
}

void ImportOrderDialog::importOrders()
{
    const auto selection = w_orders->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto order = idx.data(OrderPointerRole).value<const BrickLink::Order *>();

        QUrl url("https://www.bricklink.com/orderExcelFinal.asp");
        QUrlQuery query;
        query.addQueryItem("action",        "save");
        query.addQueryItem("orderType",     (order->type() == BrickLink::OrderType::Received)
                           ? "received" : "placed");
        query.addQueryItem("viewType",      "X");    // XML - this has to go last, otherwise we get HTML
        query.addQueryItem("getOrders",     "");
        query.addQueryItem("getStatusSel",  "I");
        query.addQueryItem("getDetail",     "y");
        query.addQueryItem("orderID",       order->id());
        query.addQueryItem("getDateFormat", "0");    // MM/DD/YYYY
        query.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
        query.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);
        url.setQuery(query);

        auto job = TransferJob::post(url, nullptr, true /* no redirects */);

        QUrl addressUrl("https://www.bricklink.com/orderDetail.asp");
        QUrlQuery addressQuery;
        addressQuery.addQueryItem("ID", order->id());
        addressUrl.setQuery(addressQuery);

        auto addressJob = TransferJob::get(addressUrl);

        auto orderCopy = new BrickLink::Order(*order);
        job->setUserData('o', orderCopy);
        addressJob->setUserData('a', orderCopy);
        m_orderDownloads << OrderDownload { orderCopy, job, addressJob };

        m_trans->retrieve(job);
        m_trans->retrieve(addressJob);
    }
}

void ImportOrderDialog::showOrdersOnBrickLink()
{
    const auto selection = w_orders->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto order = idx.data(OrderPointerRole).value<const BrickLink::Order *>();
        QByteArray orderId = order->id().toLatin1();
        BrickLink::core()->openUrl(BrickLink::URL_OrderDetails, orderId.constData());
    }
}

void ImportOrderDialog::checkSelected()
{
    bool b = w_orders->selectionModel()->hasSelection();
    w_import->setEnabled(b);
    w_showOnBrickLink->setEnabled(b);
}

void ImportOrderDialog::activateItem()
{
    checkSelected();
    w_import->animateClick();
}

void ImportOrderDialog::updateStatusLabel()
{
    if (m_currentUpdate.isEmpty()) {
        w_lastUpdated->setText(tr("Last updated %1").arg(
                                   HumanReadableTimeDelta::toString(m_lastUpdated,
                                                                    QDateTime::currentDateTime())));
    } else {
        w_lastUpdated->setText(tr("Currently updating orders"));
    }
}

#include "importorderdialog.moc"
#include "moc_importorderdialog.cpp"
