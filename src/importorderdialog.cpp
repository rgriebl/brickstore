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
#include <QKeyEvent>

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
#include "betteritemdelegate.h"

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

static const std::pair<BrickLink::OrderStatus, const char *> orderStatus[] = {
    { BrickLink::OrderStatus::Unknown,    QT_TRANSLATE_NOOP("OrderModel", "Unknown")    },
    { BrickLink::OrderStatus::Pending,    QT_TRANSLATE_NOOP("OrderModel", "Pending")    },
    { BrickLink::OrderStatus::Updated,    QT_TRANSLATE_NOOP("OrderModel", "Updated")    },
    { BrickLink::OrderStatus::Processing, QT_TRANSLATE_NOOP("OrderModel", "Processing") },
    { BrickLink::OrderStatus::Ready,      QT_TRANSLATE_NOOP("OrderModel", "Ready")      },
    { BrickLink::OrderStatus::Paid,       QT_TRANSLATE_NOOP("OrderModel", "Paid")       },
    { BrickLink::OrderStatus::Packed,     QT_TRANSLATE_NOOP("OrderModel", "Packed")     },
    { BrickLink::OrderStatus::Shipped,    QT_TRANSLATE_NOOP("OrderModel", "Shipped")    },
    { BrickLink::OrderStatus::Received,   QT_TRANSLATE_NOOP("OrderModel", "Received")   },
    { BrickLink::OrderStatus::Completed,  QT_TRANSLATE_NOOP("OrderModel", "Completed")  },
    { BrickLink::OrderStatus::OCR,        QT_TRANSLATE_NOOP("OrderModel", "OCR")        },
    { BrickLink::OrderStatus::NPB,        QT_TRANSLATE_NOOP("OrderModel", "NPB")        },
    { BrickLink::OrderStatus::NPX,        QT_TRANSLATE_NOOP("OrderModel", "NPX")        },
    { BrickLink::OrderStatus::NRS,        QT_TRANSLATE_NOOP("OrderModel", "NRS")        },
    { BrickLink::OrderStatus::NSS,        QT_TRANSLATE_NOOP("OrderModel", "NSS")        },
    { BrickLink::OrderStatus::Cancelled,  QT_TRANSLATE_NOOP("OrderModel", "Cancelled")  },
};

BrickLink::OrderStatus orderStatusFromString(const QString &s)
{
    for (const auto &os : orderStatus) {
        if (s == QLatin1String(os.second))
            return os.first;
    }
    return BrickLink::OrderStatus::Unknown;
}

QString orderStatusToString(BrickLink::OrderStatus status, bool translated = true)
{
    for (const auto &os : orderStatus) {
        if (status == os.first) {
            return translated ? qApp->translate("OrderModel", os.second)
                              : QString::fromLatin1(os.second);
        }
    }
    return { };
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
        return parent.isValid() ? 0 : 8;
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
            case 2: return orderStatusToString(order->status(), true);
            case 3: return order->id();
            case 4: {
                int firstline = order->address().indexOf('\n'_l1);
                if (firstline > 0) {
                    return QString::fromLatin1("%2 (%1)")
                            .arg(order->address().left(firstline), order->otherParty());
                }
                return order->otherParty();
            }
            case 5: return QLocale::system().toString(order->itemCount());
            case 6: return QLocale::system().toString(order->lotCount());
            case 7: return Currency::toString(order->grandTotal(), order->currencyCode(), 2);
            }
        } else if (role == Qt::DecorationRole) {
            switch (col) {
            case 4: {
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
            return int(Qt::AlignVCenter) | int((col == 7) ? Qt::AlignRight : Qt::AlignLeft);
        } else if (role == Qt::BackgroundRole) {
            if (col == 1) {
                QColor c((order->type() == BrickLink::OrderType::Received) ? Qt::green : Qt::blue);
                c.setAlphaF(0.1);
                return c;
            } else if (col == 2) {
                QColor c = QColor::fromHslF(qreal(order->status()) / qreal(BrickLink::OrderStatus::Count),
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
        } else if (role == OrderFilterRole) {
            switch (col) {
            case 0: return order->date();
            case 1: return int(order->type());
            case 2: return orderStatusToString(order->status(), true);
            case 3: return order->id();
            case 4: return order->otherParty();
            case 5: return order->itemCount();
            case 6: return order->lotCount();
            case 7: return order->grandTotal();
            }
        }

        return { };
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override
    {
        if (orient == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                case 0: return ImportOrderDialog::tr("Date");
                case 1: return ImportOrderDialog::tr("Type");
                case 2: return ImportOrderDialog::tr("Status");
                case 3: return ImportOrderDialog::tr("Order ID");
                case 4: return ImportOrderDialog::tr("Buyer/Seller");
                case 5: return ImportOrderDialog::tr("Items");
                case 6: return ImportOrderDialog::tr("Lots");
                case 7: return ImportOrderDialog::tr("Total");
                }
            }
            else if (role == Qt::TextAlignmentRole) {
                return (section == 7) ? Qt::AlignRight : Qt::AlignLeft;
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
    w_orders->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    w_orders->setItemDelegate(new BetterItemDelegate(BetterItemDelegate::AlwaysShowSelection
                                                     | BetterItemDelegate::MoreSpacing, w_orders));

    connect(w_filter, &HistoryLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { w_filter->setFocus(); });

    w_importCombined = new QPushButton();
    w_importCombined->setDefault(false);
    w_buttons->addButton(w_importCombined, QDialogButtonBox::ActionRole);
    connect(w_importCombined, &QAbstractButton::clicked,
            this, [this]() { importOrders(w_orders->selectionModel()->selectedRows(), true); });
    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, [this]() { importOrders(w_orders->selectionModel()->selectedRows(), false); });
    w_showOnBrickLink = new QPushButton();
    w_showOnBrickLink->setIcon(QIcon::fromTheme("bricklink"_l1));
    w_buttons->addButton(w_showOnBrickLink, QDialogButtonBox::ActionRole);
    connect(w_showOnBrickLink, &QAbstractButton::clicked,
            this, &ImportOrderDialog::showOrdersOnBrickLink);

    connect(w_update, &QToolButton::clicked,
            this, &ImportOrderDialog::updateOrders);
    connect(w_orders->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportOrderDialog::checkSelected);
    connect(w_orders, &QTreeView::activated,
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
            &ImportOrderDialog::updateStatusLabel);
    t->start();

    connect(m_trans, &Transfer::finished,
            this, &ImportOrderDialog::downloadFinished);

    QMetaObject::invokeMethod(this, &ImportOrderDialog::updateOrders, Qt::QueuedConnection);

    QByteArray ba = Config::inst()->value("/MainWindow/ImportOrderDialog/Geometry"_l1)
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
    int daysBack = Config::inst()->value("/MainWindow/ImportOrderDialog/DaysBack"_l1, -1).toInt();
    if (daysBack > 0)
        w_daysBack->setValue(daysBack);
    ba = Config::inst()->value("/MainWindow/ImportOrderDialog/Filter"_l1).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportOrderDialog::~ImportOrderDialog()
{
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Geometry"_l1, saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/DaysBack"_l1, w_daysBack->value());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Filter"_l1, w_filter->saveState());
}

void ImportOrderDialog::keyPressEvent(QKeyEvent *e)
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
    w_importCombined->setText(tr("Import combined"));
    w_filter->setToolTip(Utility::toolTipLabel(tr("Filter the list for lines containing these words"),
                                               QKeySequence::Find, w_filter->instructionToolTip()));
    w_showOnBrickLink->setText(tr("Show"));
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
        QUrl url("https://www.bricklink.com/orderExcelFinal.asp"_l1);
        QUrlQuery query;
        query.addQueryItem("action"_l1,        "save"_l1);
        query.addQueryItem("orderType"_l1,     QLatin1String(type));
        query.addQueryItem("viewType"_l1,      "X"_l1);
        query.addQueryItem("getOrders"_l1,     "date"_l1);
        query.addQueryItem("fMM"_l1,           QString::number(fromDate.month()));
        query.addQueryItem("fDD"_l1,           QString::number(fromDate.day()));
        query.addQueryItem("fYY"_l1,           QString::number(fromDate.year()));
        query.addQueryItem("tMM"_l1,           QString::number(today.month()));
        query.addQueryItem("tDD"_l1,           QString::number(today.day()));
        query.addQueryItem("tYY"_l1,           QString::number(today.year()));
        query.addQueryItem("getStatusSel"_l1,  "I"_l1);
        query.addQueryItem("getFiled"_l1,      "Y"_l1);
        query.addQueryItem("getDateFormat"_l1, "0"_l1);    // MM/DD/YYYY
        query.addQueryItem("frmUsername"_l1,   Config::inst()->brickLinkCredentials().first);
        query.addQueryItem("frmPassword"_l1,   Config::inst()->brickLinkCredentials().second);
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
    int type = job->userData<void>().first; // r_eceived, p_laced, a_ddress, o_rder

    switch (type) {
    case 'o': {
        auto order = job->userData<BrickLink::Order>('o');
        orderDownloadFinished(order, job, *job->data());
        break;
    }
    case 'a': {
        auto order = job->userData<BrickLink::Order>('a');

        if (!job->data()->isEmpty()) {
            QString s = QString::fromUtf8(*job->data());
            QString a;

            QRegularExpression regExp(R"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Name & Address:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)"_l1);
            auto matches = regExp.globalMatch(s);
            if (order->type() == BrickLink::OrderType::Placed) {
                // skip our own address
                if (matches.hasNext())
                    matches.next();
            }
            if (matches.hasNext()) {
                QRegularExpressionMatch match = matches.next();
                a = match.captured(1);
                a.replace(QRegularExpression(R"(<[bB][rR] ?/?>)"_l1), "\n"_l1);
                order->setAddress(a);
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
                        throw Exception("Order without ORDERID");

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
                    order->setStatus(orderStatusFromString(p.elementText(e, "ORDERSTATUS")));
                    order->setPaymentType(p.elementText(e, "PAYMENTTYPE", ""));
                    order->setTrackingNumber(p.elementText(e, "ORDERTRACKNO", ""));
                    auto location = p.elementText(e, "LOCATION");
                    if (!location.isEmpty()) {
                        order->setCountryCode(BrickLink::core()->countryIdFromName(
                                                  location.section(", "_l1, 0, 0)));
                    }

                    orders << order;
                });
            } catch (const Exception &e) {
                MessageBox::warning(this, { },
                                    tr("Could not parse the receive order XML data") % u"<br><br>" %  e.error());
            }
        }

        m_orderModel->setOrders(orders, type == 'r' ? BrickLink::OrderType::Received
                                                    : BrickLink::OrderType::Placed);

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
        break;
    }
    default:
        break;
    }
}

void ImportOrderDialog::orderDownloadFinished(BrickLink::Order *order, TransferJob *job,
                                              const QByteArray &xml)
{
    bool hasCombinedFinished = false;

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
            if (!it->m_combine) {
                LotList lots = parseOrderXML(it->m_order, it->m_xmlData);
                if (!lots.isEmpty()) {
                    if (auto doc = DocumentIO::importBrickLinkOrder(order, lots))
                        FrameWork::inst()->createWindow(doc);
                }
                m_orderDownloads.erase(it);
            } else {
                it->m_finished = true;
                hasCombinedFinished = true;
            }
            break;
        }
    }

    if (hasCombinedFinished) {
        bool allCombinedFinished = true;
        int combinedCount = 0;
        for (auto it = m_orderDownloads.begin(); it != m_orderDownloads.end(); ++it) {
            if (it->m_combine) {
                allCombinedFinished = allCombinedFinished && it->m_finished;
                ++combinedCount;
            }
        }

        if (allCombinedFinished) {
            auto *order = new BrickLink::Order(tr("Multiple"), BrickLink::OrderType::Any);
            LotList lots;

            int orderCount = 0;
            QString defaultCCode = Config::inst()->defaultCurrencyCode();

            for (auto it = m_orderDownloads.begin(); it != m_orderDownloads.end(); ) {
                if (it->m_combine) {
                    double crate = 0;

                    if (it->m_combineCCode && (it->m_order->currencyCode() != defaultCCode))
                        crate = Currency::inst()->crossRate(it->m_order->currencyCode(), defaultCCode);

                    LotList orderLots = parseOrderXML(it->m_order, it->m_xmlData);
                    if (!orderLots.isEmpty()) {
                        QColor col = QColor::fromHsl(360 * orderCount / combinedCount, 128, 128);
                        for (auto &orderLot : orderLots) {
                            orderLot->setMarkerText(it->m_order->id() % u' ' % it->m_order->otherParty());
                            orderLot->setMarkerColor(col);

                            if (crate) {
                                orderLot->setCost(orderLot->cost() * crate);
                                orderLot->setPrice(orderLot->price() * crate);
                                for (int i = 0; i < 3; ++i)
                                    orderLot->setTierPrice(i, orderLot->tierPrice(i) * crate);
                            }
                        }

                        lots.append(orderLots);
                        order->setCurrencyCode(it->m_combineCCode ? defaultCCode
                                                                  : it->m_order->currencyCode());
                        order->setItemCount(order->itemCount() + it->m_order->itemCount());
                        order->setLotCount(order->lotCount() + it->m_order->lotCount());
                    }
                    delete it->m_order;
                    it = m_orderDownloads.erase(it);
                } else {
                    ++it;
                }
                ++orderCount;
            }

            if (auto doc = DocumentIO::importBrickLinkOrder(order, lots))
                FrameWork::inst()->createWindow(doc);
        }
    }
}

LotList ImportOrderDialog::parseOrderXML(BrickLink::Order *order, const QByteArray &orderXml)
{
    // we should really parse the XML ourselves
    int start = orderXml.indexOf("\n      <ITEM>");
    int end = orderXml.lastIndexOf("</ITEM>");
    QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><INVENTORY>\n" %
            orderXml.mid(start, end - start + 8) % "</INVENTORY>";

    try {
        auto result = DocumentIO::fromBrickLinkXML(xml);
        return result.lots;
    } catch (const Exception &e) {
        MessageBox::warning(nullptr, { }, tr("Failed to import order %1").arg(order->id())
                            % u"<br><br>" % e.error());
        return { };
    }
}

void ImportOrderDialog::importOrders(const QModelIndexList &rows, bool combined)
{
    bool combineCCode = false;
    if (combined && m_selectedCurrencyCodes.size() > 1) {
        if (MessageBox::question(this, { },
                                 tr("You have selected multiple orders with differing currencies, which cannot be combined as-is.")
                                 % "<br><br>"_l1
                                 % tr("Do you want to continue and convert all prices to your default currency (%1)?")
                                 .arg(Config::inst()->defaultCurrencyCode())) == QMessageBox::No) {
            return;
        }
        combineCCode = true;
    }

    for (auto idx : rows) {
        auto order = idx.data(OrderPointerRole).value<const BrickLink::Order *>();

        QUrl url("https://www.bricklink.com/orderExcelFinal.asp"_l1);
        QUrlQuery query;
        query.addQueryItem("action"_l1,        "save"_l1);
        query.addQueryItem("orderType"_l1,     QLatin1String((order->type() == BrickLink::OrderType::Received)
                                                       ? "received" : "placed"));
        query.addQueryItem("viewType"_l1,      "X"_l1);    // XML - this has to go last, otherwise we get HTML
        query.addQueryItem("getOrders"_l1,     ""_l1);
        query.addQueryItem("getStatusSel"_l1,  "I"_l1);
        query.addQueryItem("getFiled"_l1,      "Y"_l1);
        query.addQueryItem("getDetail"_l1,     "y"_l1);
        query.addQueryItem("orderID"_l1,       order->id());
        query.addQueryItem("getDateFormat"_l1, "0"_l1);    // MM/DD/YYYY
        query.addQueryItem("frmUsername"_l1,   Config::inst()->brickLinkCredentials().first);
        query.addQueryItem("frmPassword"_l1,   Config::inst()->brickLinkCredentials().second);
        url.setQuery(query);

        auto job = TransferJob::post(url, nullptr, true /* no redirects */);

        QUrl addressUrl("https://www.bricklink.com/orderDetail.asp"_l1);
        QUrlQuery addressQuery;
        addressQuery.addQueryItem("ID"_l1, order->id());
        addressUrl.setQuery(addressQuery);

        auto addressJob = TransferJob::get(addressUrl);

        auto orderCopy = new BrickLink::Order(*order);
        job->setUserData('o', orderCopy);
        addressJob->setUserData('a', orderCopy);
        m_orderDownloads << OrderDownload { orderCopy, job, addressJob, combined, combineCCode };

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
    m_selectedCurrencyCodes.clear();

    // combined import only makes sense for the same type

    if (b) {
        BrickLink::OrderType otype = BrickLink::OrderType::Any;

        const auto rows = w_orders->selectionModel()->selectedRows();
        if (rows.size() > 1) {
            for (auto idx : rows) {
                auto order = idx.data(OrderPointerRole).value<const BrickLink::Order *>();
                m_selectedCurrencyCodes.insert(order->currencyCode());

                if (otype == BrickLink::OrderType::Any) {
                    otype = order->type();
                } else if (otype != order->type()) {
                    otype = BrickLink::OrderType::Any;
                    break;
                }
            }
            if (otype == BrickLink::OrderType::Any)
                b = false;
        } else {
            b = false;
        }
    }
    w_importCombined->setEnabled(b);
}

void ImportOrderDialog::updateStatusLabel()
{
    if (m_currentUpdate.isEmpty()) {
        w_lastUpdated->setText(tr("Last updated %1").arg(
                                   HumanReadableTimeDelta::toString(QDateTime::currentDateTime(),
                                                                    m_lastUpdated)));
    } else {
        w_lastUpdated->setText(tr("Currently updating orders"));
    }
}

#include "importorderdialog.moc"
#include "moc_importorderdialog.cpp"
