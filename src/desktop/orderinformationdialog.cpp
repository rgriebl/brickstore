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
#include <QGuiApplication>
#include <QClipboard>
#include <QLabel>
#include <QToolButton>
#include "utility/utility.h"
#include "utility/currency.h"
#include "orderinformationdialog.h"


OrderInformationDialog::OrderInformationDialog(const BrickLink::Order *order, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    if (!order)
        return;

    static auto setup = [](QLineEdit *label, QToolButton *button, const QString &value,
            bool visible = true) {
        if (visible) {
            label->setText(value);
            if (button)
                QObject::connect(button, &QToolButton::clicked, button, [label]() {
                    QGuiApplication::clipboard()->setText(label->text(), QClipboard::Clipboard);
                });
        } else {
            label->hide();
            if (button)
                button->hide();
        }
    };

    QLocale loc;

    const QString bon("<b>"_l1);
    const QString boff("</b>"_l1);

    w_info->setText(tr("Order %1, %2 %3 on %4").arg(
                        bon % order->id() % boff,
                        order->type() == BrickLink::OrderType::Received ? tr("received from")
                                                                        : tr("placed at"),
                        bon % order->otherParty() % boff,
                        loc.toString(order->date())));
    w_status->setText(BrickLink::Order::statusToString(order->status()));
    w_lastUpdated->setText(loc.toString(order->lastUpdated()));
    setup(w_tracking, w_trackingCopy, order->trackingNumber());

    w_addressCopy->setProperty("bsAddress", order->address());
    QObject::connect(w_addressCopy, &QToolButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(w_addressCopy->property("bsAddress").toString(),
                                              QClipboard::Clipboard);
    });

    const QStringList adr = order->address().split("\n"_l1);
    int adrSize = adr.size();

    setup(w_address1, w_address1Copy, adr.value(0), adrSize >= 1);
    setup(w_address2, w_address2Copy, adr.value(1), adrSize >= 2);
    setup(w_address3, w_address3Copy, adr.value(2), adrSize >= 3);
    setup(w_address4, w_address4Copy, adr.value(3), adrSize >= 4);
    setup(w_address5, w_address5Copy, adr.value(4), adrSize >= 5);
    setup(w_address6, w_address6Copy, adr.value(5), adrSize >= 6);
    setup(w_address7, w_address7Copy, adr.value(6), adrSize >= 7);
    setup(w_address8, w_address8Copy, adr.value(7), adrSize >= 8);

    if (order->currencyCode() == order->paymentCurrencyCode()) {
        w_currencyCode->setText(order->currencyCode());
    } else {
        w_currencyCode->setText(order->currencyCode() % u", " % tr("Payment in") % u' '
                                % order->paymentCurrencyCode());
    }

    setup(w_shipping,     w_shippingCopy,     Currency::toDisplayString(order->shipping()));
    setup(w_insurance,    w_insuranceCopy,    Currency::toDisplayString(order->insurance()));
    setup(w_addCharges1,  w_addCharges1Copy,  Currency::toDisplayString(order->additionalCharges1()));
    setup(w_addCharges2,  w_addCharges2Copy,  Currency::toDisplayString(order->additionalCharges2()));
    setup(w_credit,       w_creditCopy,       Currency::toDisplayString(order->credit()));
    setup(w_creditCoupon, w_creditCouponCopy, Currency::toDisplayString(order->creditCoupon()));
    setup(w_total,        w_totalCopy,        Currency::toDisplayString(order->orderTotal()));
    setup(w_salesTax,     w_salesTaxCopy,     Currency::toDisplayString(order->salesTax()));
    setup(w_grandTotal,   w_grandTotalCopy,   Currency::toDisplayString(order->grandTotal()));
    setup(w_vat,          w_vatCopy,          Currency::toDisplayString(order->vatCharges()));

}
