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
#include <QBuffer>
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
#include "messagebox.h"
#include "xmlhelpers.h"
#include "exception.h"
#include "config.h"
#include "framework.h"
#include "documentio.h"
#include "humanreadabletimedelta.h"

#include "importorderdialog.h"

Q_DECLARE_METATYPE(const BrickLink::Order *)

using namespace std::chrono_literals;


enum {
    OrderPointerRole = Qt::UserRole + 1,
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

        m_trans = new Transfer(this);
        connect(m_trans, &Transfer::finished,
                this, &OrderModel::flagReceived);
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

    void updateOrder(BrickLink::Order *order)
    {
        int r = m_orders.indexOf(order);
        if (r >= 0)
            emit dataChanged(index(r, 0), index(r, columnCount() - 1));
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
            case 0: return (order->type() == BrickLink::OrderType::Received)
                        ? ImportOrderDialog::tr("Received") : ImportOrderDialog::tr("Placed");
            case 1: return order->id();
            case 2: return QLocale::system().toString(order->date(), QLocale::ShortFormat);
            case 3: return QLocale::system().toString(order->itemCount());
            case 4: return QLocale::system().toString(order->lotCount());
            case 5: {
                int firstline = order->address().indexOf('\n');
                if (firstline > 0) {
                    return QString::fromLatin1("%2 (%1)")
                            .arg(order->address().left(firstline), order->otherParty());
                }
                return order->otherParty();
            }
            case 6: return Currency::toString(order->grandTotal(), order->currencyCode(),
                                              Currency::InternationalSymbol, 2);
            }
        } else if (role == Qt::DecorationRole) {
            switch (col) {
            case 5:
                QImage *flag = nullptr;
                QString cc = order->countryCode();
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
            return (col == 6) ? Qt::AlignRight : Qt::AlignLeft;
        }
        else if (role == Qt::BackgroundRole) {
            if (col == 0) {
                QColor c((order->type() == BrickLink::OrderType::Received) ? Qt::green : Qt::blue);
                c.setAlphaF(0.2);
                return c;
            }
        }
        else if (role == Qt::ToolTipRole) {
            QString tt = data(index, Qt::DisplayRole).toString();

            if (!order->address().isEmpty())
                tt = tt + QLatin1String("\n\n") + order->address();
            return tt;
        }
        else if (role == OrderPointerRole) {
            return QVariant::fromValue(order);
        }

        return { };
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override
    {
        if (orient == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                case  0: return ImportOrderDialog::tr("Type");
                case  1: return ImportOrderDialog::tr("Order #");
                case  2: return ImportOrderDialog::tr("Date");
                case  3: return ImportOrderDialog::tr("Items");
                case  4: return ImportOrderDialog::tr("Lots");
                case  5: return ImportOrderDialog::tr("Buyer/Seller");
                case  6: return ImportOrderDialog::tr("Total");
                }
            }
            else if (role == Qt::TextAlignmentRole) {
                return (section == 6) ? Qt::AlignRight : Qt::AlignLeft;
            }
        }
        return { };
    }

    void sort(int section, Qt::SortOrder so) override
    {
        emit layoutAboutToBeChanged();
        m_sortSection = section;
        m_sortOrder = so;

        std::sort(m_orders.begin(), m_orders.end(), [section, so](const auto &op1, const auto &op2) {
            int d = 0;

            BrickLink::Order *o1 = op1;
            BrickLink::Order *o2 = op2;

            switch (section) {
            case  0: d = int(o1->type()) - int(o2->type()); break;
            case  1: d = o1->id().compare(o2->id()); break;
            case  2: d = o1->date().daysTo(o2->date()); break;
            case  3: d = o1->itemCount() - o2->itemCount(); break;
            case  4: d = o1->lotCount() - o2->lotCount(); break;
            case  5: d = o1->otherParty().compare(o2->otherParty(), Qt::CaseInsensitive); break;
            case  6: d = qFuzzyCompare(o1->grandTotal(), o2->grandTotal())
                        ? 0
                        : ((o1->grandTotal() < o2->grandTotal()) ? -1 : 1); break;
            }

            // this predicate needs to establish a strict weak order:
            // if you return true for (o1, o2), you are not allowed to also return true for (o2, o1)

            if (d == 0)
                return false;
            else
                return (so == Qt::DescendingOrder) ? (d > 0) : (d < 0);
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

            for (int r = 0; r < m_orders.size(); ++r) {
                if (m_orders.at(r)->countryCode() == cc) {
                    QModelIndex idx = index(r, 5, { });
                    emit dataChanged(idx, idx);
                }
            }
        } else {
            delete img;
        }
    }

private:
    QVector<BrickLink::Order *> m_orders;
    QCache<QString, QImage> m_flags;
    Transfer *m_trans;
    int m_sortSection = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};


class TransHighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    TransHighlightDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    { }

    void makeTrans(QPalette &pal, QPalette::ColorGroup cg, QPalette::ColorRole cr, const QColor &bg, qreal factor) const
    {
        QColor hl = pal.color(cg, cr);
        QColor th;
        th.setRgbF(hl.redF()   * (1.0 - factor) + bg.redF()   * factor,
                   hl.greenF() * (1.0 - factor) + bg.greenF() * factor,
                   hl.blueF()  * (1.0 - factor) + bg.blueF()  * factor);

        pal.setColor(cg, cr, th);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (option.state & QStyle::State_Selected) {
            QVariant v = index.data(Qt::BackgroundRole);
            QColor c = qvariant_cast<QColor> (v);
            if (v.isValid() && c.isValid()) {
                QStyleOptionViewItem myoption(option);

                makeTrans(myoption.palette, QPalette::Active,   QPalette::Highlight, c, 0.2);
                makeTrans(myoption.palette, QPalette::Inactive, QPalette::Highlight, c, 0.2);
                makeTrans(myoption.palette, QPalette::Disabled, QPalette::Highlight, c, 0.2);

                QStyledItemDelegate::paint(painter, myoption, index);
                return;
            }
        }

        QStyledItemDelegate::paint(painter, option, index);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto s = QStyledItemDelegate::sizeHint(option, index);
        s.rwidth() += option.fontMetrics.height() * 1;
        return s;
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
    w_orders->setModel(m_orderModel);
    w_orders->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    w_orders->setItemDelegate(new TransHighlightDelegate(this));

    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, &ImportOrderDialog::importOrders);

    connect(w_update, &QToolButton::clicked,
            this, &ImportOrderDialog::updateOrders);
    connect(w_orders->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportOrderDialog::checkSelected);
    connect(w_orders, &QTreeView::activated,
            this, &ImportOrderDialog::activateItem);

    languageChange();
    update();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportOrderDialog::updateStatusLabel);

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
}

ImportOrderDialog::~ImportOrderDialog()
{
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Geometry", saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/DaysBack", w_daysBack->value());
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
                    m_orderModel->updateOrder(order);
                }
            }
        }
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
                p.parse([this, &p, &type, &orders](QDomElement e) {
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

                    QUrl addressUrl("https://www.bricklink.com/orderDetail.asp");
                    QUrlQuery query;
                    query.addQueryItem("ID", id);
                    addressUrl.setQuery(query);

                    auto addressJob = TransferJob::get(addressUrl);
                    addressJob->setUserData('a', order);
                    m_currentUpdate << addressJob;

                    m_trans->retrieve(addressJob);

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

        if (auto doc = DocumentIO::importBrickLinkOrder(order, *job->data()))
            FrameWork::inst()->createWindow(doc);

        m_orderDownloads.removeOne(job);
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

void ImportOrderDialog::importOrders()
{
    const auto selection = w_orders->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto order = m_orderModel->data(idx, OrderPointerRole).value<const BrickLink::Order *>();

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

        job->setUserData('o', new BrickLink::Order(*order));
        m_orderDownloads << job;

        m_trans->retrieve(job);
    }
}


void ImportOrderDialog::checkSelected()
{
    w_import->setEnabled(w_orders->selectionModel()->hasSelection());
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
