/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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

#include <QDateTime>
#include <QDate>
#include <QFile>
#include <QBuffer>
#include <QTextStream>
#include <QRegExp>

#include "lzmadec.h"

#include "config.h"
#include "bricklink.h"
#include "progressdialog.h"
#include "utility.h"
#include "import.h"


ImportBLStore::ImportBLStore(ProgressDialog *pd)
    : m_progress(pd)
{
    connect(pd, SIGNAL(transferFinished()), this, SLOT(gotten()));

    pd->setHeaderText(tr("Importing BrickLink Store"));
    pd->setMessageText(tr("Download: %p"));

    QUrl url("http://www.bricklink.com/invExcelFinal.asp");

    url.addQueryItem("itemType",      "");
    url.addQueryItem("catID",         "");
    url.addQueryItem("colorID",       "");
    url.addQueryItem("invNew",        "");
    url.addQueryItem("itemYear",      "");
    url.addQueryItem("viewType",      "x");    // XML
    url.addQueryItem("invStock",      "Y");
    url.addQueryItem("invStockOnly",  "");
    url.addQueryItem("invQty",        "");
    url.addQueryItem("invQtyMin",     "0");
    url.addQueryItem("invQtyMax",     "0");
    url.addQueryItem("invBrikTrak",   "");
    url.addQueryItem("invDesc",       "");
    url.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
    url.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);

    pd->post(url);

    pd->layout();
}

const BrickLink::InvItemList &ImportBLStore::items() const
{
    return m_items;
}

QString ImportBLStore::currencyCode() const
{
    return m_currencycode;
}

void ImportBLStore::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;

    if (data && data->size()) {
        QBuffer store_buffer(data);

        if (store_buffer.open(QIODevice::ReadOnly)) {
            QString emsg;
            int eline = 0, ecol = 0;
            QDomDocument doc;

            if (doc.setContent(&store_buffer, &emsg, &eline, &ecol)) {
                QDomElement root = doc.documentElement();

                BrickLink::Core::ParseItemListXMLResult result = BrickLink::core()->parseItemListXML(root, BrickLink::XMLHint_MassUpload);

                if (result.items) {
                    m_items = *result.items;
                    m_currencycode = result.currencyCode;
                    delete result.items;
                    ok = true;
                }
                else
                    m_progress->setErrorText(tr("Could not parse the XML data for the store inventory."));
            }
            else {
                if (data->startsWith("<HTML>") && data->contains("Invalid password"))
                    m_progress->setErrorText(tr("Either your username or password are incorrect."));
                else
                    m_progress->setErrorText(tr("Could not parse the XML data for the store inventory:<br /><i>Line %1, column %2: %3</i>").arg(eline).arg(ecol).arg(emsg));
            }
        }
    }
    m_progress->setFinished(ok);
}


ImportBLOrder::ImportBLOrder(const QDate &from, const QDate &to, BrickLink::OrderType type, ProgressDialog *pd)
    : m_progress(pd), m_order_from(from), m_order_to(to), m_order_type(type)
{
    init();
}

ImportBLOrder::ImportBLOrder(const QString &order, BrickLink::OrderType type, ProgressDialog *pd)
    : m_progress(pd), m_order_id(order), m_order_type(type)
{
    init();
}

void ImportBLOrder::init()
{
    m_current_address = -1;

    m_retry_placed = (m_order_type == BrickLink::Any);

    if (!m_order_id.isEmpty()) {
        m_order_to = QDate::currentDate();
        m_order_from = m_order_to.addDays(-1);
    }

    connect(m_progress, SIGNAL(transferFinished()), this, SLOT(gotten()));

    m_progress->setHeaderText(tr("Importing BrickLink Order"));
    m_progress->setMessageText(tr("Download: %p"));

    QUrl url("http://www.bricklink.com/orderExcelFinal.asp");

    url.addQueryItem("orderType",     m_order_type == BrickLink::Placed ? "placed" : "received");
    url.addQueryItem("action",        "save");
    url.addQueryItem("orderID",       m_order_id);
    url.addQueryItem("viewType",      "X");    // XML
    url.addQueryItem("getDateFormat", "1");    // YYYY/MM/DD
    url.addQueryItem("getOrders",     m_order_id.isEmpty() ? "date" : "");
    url.addQueryItem("fDD",           QString::number(m_order_from.day()));
    url.addQueryItem("fMM",           QString::number(m_order_from.month()));
    url.addQueryItem("fYY",           QString::number(m_order_from.year()));
    url.addQueryItem("tDD",           QString::number(m_order_to.day()));
    url.addQueryItem("tMM",           QString::number(m_order_to.month()));
    url.addQueryItem("tYY",           QString::number(m_order_to.year()));
    url.addQueryItem("getDetail",     "y");    // get items (that's why we do this in the first place...)
    url.addQueryItem("getFiled",      "Y");    // regardless of filed state
    url.addQueryItem("getStatus",     "");     // regardless of status
    url.addQueryItem("statusID",      "");
    url.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
    url.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);

    m_url = url;
    m_progress->post(url);
    m_progress->layout();
}

const QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > &ImportBLOrder::orders() const
{
    return m_orders;
}

void ImportBLOrder::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;
    QString error;

    if (data && data->size()) {
        if (m_current_address >= 0) {
            QString s = QString::fromUtf8(data->data(), data->size());

            QRegExp rx1(QLatin1String("<B>Name:</B></FONT></TD>\\s*<TD NOWRAP><FONT FACE=\"Tahoma, Arial\" SIZE=\"2\">(.+)</FONT></TD>"));
            QRegExp rx2(QLatin1String("<B>Address:</B></FONT></TD>\\s*<TD NOWRAP><FONT FACE=\"Tahoma, Arial\" SIZE=\"2\">(.+)</FONT></TD>"));
            QRegExp rx3(QLatin1String("<B>Country:</B></FONT></TD>\\s*<TD NOWRAP><FONT FACE=\"Tahoma, Arial\" SIZE=\"2\">(.+)</FONT></TD>"));

            rx1.setMinimal(true);
            rx1.indexIn(s);
            rx2.setMinimal(true);
            rx2.indexIn(s);
            rx3.setMinimal(true);
            rx3.indexIn(s);

            QString a = rx1.cap(1) + QLatin1Char('\n') + rx2.cap(1) + QLatin1Char('\n') + rx3.cap(1);
            a.replace(QLatin1String("<BR>"), QLatin1String("\n"));

            m_orders[m_current_address].first->setAddress(a);
        }
        else {
            QBuffer order_buffer(data);

            if (order_buffer.open(QIODevice::ReadOnly)) {
                QString emsg;
                int eline = 0, ecol = 0;
                QDomDocument doc;

                if (doc.setContent(&order_buffer, &emsg, &eline, &ecol)) {
                    QDomElement root = doc.documentElement();

                    if ((root.nodeName() == QLatin1String("ORDERS")) && (root.firstChild().nodeName() == QLatin1String("ORDER"))) {
                        for (QDomNode ordernode = root.firstChild(); !ordernode.isNull(); ordernode = ordernode.nextSibling()) {
                            if (!ordernode.isElement())
                                continue;

                            BrickLink::Core::ParseItemListXMLResult result = BrickLink::core()->parseItemListXML(ordernode.toElement(), BrickLink::XMLHint_Order);

                            if (result.items) {
                                BrickLink::Order *order = new BrickLink::Order(QLatin1String(""), BrickLink::Placed);

                                for (QDomNode node = ordernode.firstChild(); !node.isNull(); node = node.nextSibling()) {
                                    if (!node. isElement())
                                        continue;

                                    QString tag = node.toElement().tagName();
                                    QString val = node.toElement().text();

                                    if (tag == QLatin1String("BUYER"))
                                        order->setBuyer(val);
                                    else if (tag == QLatin1String("SELLER"))
                                        order->setSeller(val);
                                    else if (tag == QLatin1String("ORDERID"))
                                        order->setId(val);
                                    else if (tag == QLatin1String("ORDERDATE"))
                                        order->setDate(QDateTime(ymd2date(val)));
                                    else if (tag == QLatin1String("ORDERSTATUSCHANGED"))
                                        order->setStatusChange(QDateTime(ymd2date(val)));
                                    else if (tag == QLatin1String("ORDERSHIPPING"))
                                        order->setShipping(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERINSURANCE"))
                                        order->setInsurance(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERADDCHRG1"))
                                        order->setDelivery(order->delivery() + QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERADDCHRG2"))
                                        order->setDelivery(order->delivery() + QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERCREDIT"))
                                        order->setCredit(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("BASEGRANDTOTAL"))
                                        order->setGrandTotal(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERSTATUS"))
                                        order->setStatus(val);
                                    else if (tag ==QLatin1String("PAYMENTTYPE"))
                                        order->setPayment(val);
                                    else if (tag == QLatin1String("ORDERREMARKS"))
                                        order->setRemarks(val);
                                    else if (tag == QLatin1String("BASECURRENCYCODE"))
                                        order->setCurrencyCode(val);
                                    else if (tag == QLatin1String("LOCATION"))
                                        order->setCountryName(val.section(QLatin1String(", "), 0, 0));
                                }

                                if (!order->id().isEmpty()) {
                                    m_orders << qMakePair(order, result.items);
                                    ok = true;
                                }
                                else {
                                    delete result.items;
                                }
                            }
                        }
                    }
                }
                // find a better way - we shouldn't display widgets here
                //else
                //    MessageBox::warning ( 0, tr( "Could not parse the XML data for your orders:<br /><i>Line %1, column %2: %3</i>" ). arg ( eline ). arg ( ecol ). arg ( emsg ));
            }
        }
    }

    if (m_retry_placed) {
        QList<QPair<QString, QString> > query = m_url.queryItems();
        query[0].second = QLatin1String("placed");
        m_url.setQueryItems(query);

        m_progress->post(m_url);
        m_progress->layout();

        m_retry_placed = false;
    }
    else if ((m_current_address + 1) < m_orders.size()) {
        m_current_address++;

        QString url = QLatin1String("http://www.bricklink.com/memberInfo.asp?u=") + m_orders[m_current_address].first->other();
        m_progress->setHeaderText(tr("Importing address records"));
        m_progress->get(url);
        m_progress->layout();
    }
    else {
        m_progress->setFinished(true);
    }
}

QDate ImportBLOrder::ymd2date(const QString &ymd)
{
    QDate d;
    QStringList sl = ymd.split(QLatin1Char('/'));
    d.setYMD(sl [0].toInt(), sl [1].toInt(), sl [2].toInt());
    return d;
}


ImportBLCart::ImportBLCart(int shopid, int cartid, ProgressDialog *pd)
    : m_progress(pd)
{
    connect(pd, SIGNAL(transferFinished()), this, SLOT(gotten()));

    pd->setHeaderText(tr("Importing BrickLink Shopping Cart"));
    pd->setMessageText(tr("Download: %p"));

    QUrl url("http://www.bricklink.com/storeCart.asp");

    url.addQueryItem("h", QString::number(shopid));
    url.addQueryItem("b", QString::number(cartid));

    pd->get(url);
    pd->layout();
}

const BrickLink::InvItemList &ImportBLCart::items() const
{
    return m_items;
}

QString ImportBLCart::currencyCode() const
{
    return m_currencycode;
}

void ImportBLCart::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;

    if (data && data->size()) {
        QBuffer cart_buffer(data);

        if (cart_buffer.open(QIODevice::ReadOnly)) {
            QTextStream ts(&cart_buffer);
            QString line;
            QString items_line;
            QString sep = QLatin1String("<TR CLASS=\"tm\"><TD HEIGHT=\"65\" ALIGN=\"CENTER\">");
            int invalid_items = 0;
            bool parsing_items = false;

            while (true) {
                line = ts.readLine();
                if (line.isNull())
                    break;
                if (line.startsWith(sep, Qt::CaseInsensitive) && !parsing_items)
                    parsing_items = true;

                if (parsing_items)
                    items_line += line;

                if ((line.compare(QLatin1String("</TABLE>"), Qt::CaseInsensitive) == 0) && parsing_items)
                    break;
            }

            QStringList strlist = items_line.split(sep, QString::SkipEmptyParts);

            foreach(const QString &str, strlist) {
                BrickLink::InvItem *ii = 0;

                QRegExp rx_ids(QLatin1String("HEIGHT='60' SRC='http://img.bricklink.com/([A-Z])/([^ ]+).gif' NAME="), Qt::CaseInsensitive);
                QRegExp rx_qty_price(QLatin1String(" VALUE=\"([0-9]+)\">(&nbsp;\\(x[0-9]+\\))?<BR>Qty Available: <B>[0-9]+</B><BR>Each:&nbsp;<B>([A-Z $]+)([0-9.]+)</B>"), Qt::CaseInsensitive);
                QRegExp rx_names(QLatin1String("</TD><TD>(.+)</TD><TD VALIGN=\"TOP\" NOWRAP>"), Qt::CaseInsensitive);
                QString str_cond(QLatin1String("<B>New</B>"));

                rx_ids.indexIn(str);
                rx_names.indexIn(str);

                const BrickLink::Item *item = 0;
                const BrickLink::Color *col = 0;

                if (rx_ids.cap(1).length() == 1) {
                    int slash = rx_ids.cap(2).indexOf(QLatin1Char('/'));

                    if (slash >= 0) {   // with color
                        item = BrickLink::core()->item(rx_ids.cap(1)[0].toLatin1(), rx_ids.cap(2).mid(slash + 1).toLatin1());
                        col = BrickLink::core()->color(rx_ids.cap(2).left(slash).toInt());
                    }
                    else {
                        item = BrickLink::core()->item(rx_ids.cap(1)[0].toLatin1(), rx_ids.cap(2).toLatin1().constData());
                        col = BrickLink::core()->color(0);
                    }
                }

                QString color_and_item = rx_names.cap(1).trimmed();

                if (!col || !color_and_item.startsWith(col->name())) {
                    int longest_match = 0;

                    foreach(const BrickLink::Color *blcolor, BrickLink::core()->colors()) {
                        QString n(blcolor->name());

                        if ((n.length() > longest_match) &&
                                (color_and_item.startsWith(n))) {
                            longest_match = n.length();
                            col = blcolor;
                        }
                    }

                    if (!longest_match)
                        col = BrickLink::core()->color(0);
                }

                if (!item /*|| !color_and_item.endsWith ( item->name ( ))*/) {
                    int longest_match = 0;

                    const QVector<const BrickLink::Item *> &all_items = BrickLink::core()->items();
                    for (int i = 0; i < all_items.count(); i++) {
                        const BrickLink::Item *it = all_items [i];
                        QString n(it->name());
                        n = n.trimmed();

                        if ((n.length() > longest_match) &&
                                (color_and_item.indexOf(n)) >= 0) {
                            longest_match = n.length();
                            item = it;
                        }
                    }

                    if (!longest_match)
                        item = 0;
                }

                if (item && col) {
                    rx_qty_price.indexIn(str);

                    int qty = rx_qty_price.cap(1).toInt();
                    if (m_currencycode.isEmpty()) {
                        m_currencycode = rx_qty_price.cap(3).trimmed();
                        m_currencycode.replace(QLatin1String(" $"), QLatin1String("D")); // 'US $' -> 'USD', 'AU $' -> 'AUD'
                        if (m_currencycode.length() != 3)
                            m_currencycode.clear();
                    }
                    double price = QLocale::c().toDouble(rx_qty_price.cap(4));

                    BrickLink::Condition cond = (str.indexOf(str_cond, 0, Qt::CaseInsensitive) >= 0 ? BrickLink::New : BrickLink::Used);

                    QString comment;
                    int comment_pos = color_and_item.indexOf(item->name());

                    if (comment_pos >= 0)
                        comment = color_and_item.mid(comment_pos + QString(item->name()).length() + 1);

                    if (qty && (price != 0)) {
                        ii = new BrickLink::InvItem(col, item);
                        ii->setCondition(cond);
                        ii->setQuantity(qty);
                        ii->setPrice(price);
                        ii->setComments(comment);
                    }
                }

                if (ii)
                    m_items << ii;
                else
                    invalid_items++;
            }

            if (!m_items.isEmpty()) {
                ok = true;

                if (invalid_items) {
                    m_progress->setMessageText(tr("%1 lots of your Shopping Cart could not be imported.").arg(invalid_items));
                    m_progress->setAutoClose(false);
                }
            }
            else {
                m_progress->setErrorText(tr("Could not parse the Shopping Cart contents."));
            }
        }
    }
    m_progress->setFinished(ok);
}


ImportPeeronInventory::ImportPeeronInventory(const QString &peeronid, ProgressDialog *pd)
    : m_progress(pd), m_peeronid(peeronid)
{
    connect(pd, SIGNAL(transferFinished()), this, SLOT(gotten()));

    pd->setHeaderText(tr("Importing Peeron Inventory"));
    pd->setMessageText(tr("Download: %p"));

    QString url = QString("http://www.peeron.com/inv/sets/%1").arg(peeronid);

    pd->get(url);
    pd->layout();
}

const BrickLink::InvItemList &ImportPeeronInventory::items() const
{
    return m_items;
}

QString ImportPeeronInventory::currencyCode() const
{
    return QLatin1String("USD");
}

void ImportPeeronInventory::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;

    if (data && data->size()) {
        QBuffer peeron_buffer(data);

        if (peeron_buffer.open(QIODevice::ReadOnly)) {
            BrickLink::InvItemList *items = fromPeeron(&peeron_buffer);

            if (items) {
                m_items += *items;
                delete items;
                ok = true;
            }
            else
                m_progress->setErrorText(tr("Could not parse the Peeron inventory."));
        }
    }
    m_progress->setFinished(ok);
}

BrickLink::InvItemList *ImportPeeronInventory::fromPeeron(QIODevice *peeron)
{
    if (!peeron)
        return false;

    QTextStream in(peeron);

    QDomDocument doc(QString::null);
    QDomElement root = BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_Inventory, BrickLink::InvItemList());
    doc.appendChild(root);

    QString line;
    bool next_is_item = false;
    int count = 0;

    QRegExp itempattern(QLatin1String("<a href=[^>]+>(.+)</a>"));

    while (!(line = in.readLine()).isNull()) {
        if (next_is_item && line.startsWith(QLatin1String("<td>")) && line.endsWith(QLatin1String("</td>"))) {
            QString tmp = line.mid(4, line.length() - 9);
            QStringList sl = tmp.split(QLatin1String("</td><td>"), QString::KeepEmptyParts);

            bool line_ok = false;

            if (sl.count() >= 3) {
                int qty, colorid;
                QString itemid, itemname, colorname;
                char itemtype = 'P';

                qty = sl [0].toInt();

                if (itempattern.exactMatch(sl [1]))
                    itemid = itempattern.cap(1);

                colorname = sl [2];
                const BrickLink::Color *color = BrickLink::core()->colorFromPeeronName(colorname.toLatin1().constData());
                colorid = color ? int(color->id()) : -1;

                itemname = sl [3];

                int pos = itemname.indexOf(" <");
                if (pos > 0)
                    itemname.truncate(pos);

                if (itemid.indexOf(QRegExp(QLatin1String("-\\d+$"))) > 0)
                    itemtype = 'S';

                if (qty > 0) {
                    QDomElement item = doc.createElement(QLatin1String("ITEM"));
                    root.appendChild(item);

                    // <ITEMNAME> and <COLORNAME> are inofficial extension
                    // to help with incomplete items in Peeron inventories.

                    item.appendChild(doc.createElement(QLatin1String("ITEMTYPE")).appendChild(doc.createTextNode(QChar(itemtype))).parentNode());
                    item.appendChild(doc.createElement(QLatin1String("ITEMID")).appendChild(doc.createTextNode(itemid)).parentNode());
                    item.appendChild(doc.createElement(QLatin1String("ITEMNAME")).appendChild(doc.createTextNode(itemname)).parentNode());
                    item.appendChild(doc.createElement(QLatin1String("COLOR")).appendChild(doc.createTextNode(QString::number(colorid))).parentNode());
                    item.appendChild(doc.createElement(QLatin1String("COLORNAME")).appendChild(doc.createTextNode(colorname)).parentNode());
                    item.appendChild(doc.createElement(QLatin1String("QTY")).appendChild(doc.createTextNode(QString::number(qty))).parentNode());

                    line_ok = true;
                    count++;
                }
            }
            if (!line_ok)
                qWarning("Failed to parse item line: %s", qPrintable(line));

            next_is_item = false;
        }

        if ((line == QLatin1String("<tr bgcolor=\"#dddddd\">")) || (line == QLatin1String("<tr bgcolor=\"#eeeeee\">")))
            next_is_item = true;
        else
            next_is_item = false;
    }

    return count ? BrickLink::core()->parseItemListXML(root, BrickLink::XMLHint_Inventory).items : 0;
}
