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
#include <QGuiApplication>
#include <QClipboard>
#include <QStringBuilder>
#include <QLabel>
#include <QToolButton>
#include "utility/utility.h"
#include "common/currency.h"
#include "orderinformationdialog.h"


OrderInformationDialog::OrderInformationDialog(const BrickLink::Order *order, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    if (!order)
        return;

    static auto setup = [](QLineEdit *label, QToolButton *button, const QString &value,
            bool visible = true, QWidget *alsoHide = nullptr) {
        if (visible) {
            label->setText(value);
            label->setMinimumWidth(label->fontMetrics().horizontalAdvance(value) + 64);
            if (button)
                QObject::connect(button, &QToolButton::clicked, button, [label]() {
                    QGuiApplication::clipboard()->setText(label->text(), QClipboard::Clipboard);
                });
        } else {
            label->hide();
            if (button)
                button->hide();
            if (alsoHide)
                alsoHide->hide();
        }
    };

    QLocale loc;

    const QString bon(u"<b>"_qs);
    const QString boff(u"</b>"_qs);

    w_info->setText(tr("Order %1, %2 %3 on %4").arg(
                        bon % order->id() % boff,
                        order->type() == BrickLink::OrderType::Received ? tr("received from")
                                                                        : tr("placed at"),
                        bon % order->otherParty() % boff,
                        loc.toString(order->date())));
    w_status->setText(order->statusAsString());
    w_lastUpdated->setText(loc.toString(order->lastUpdated()));
    setup(w_tracking, w_trackingCopy, order->trackingNumber());

    w_addressCopy->setProperty("bsAddress", order->address());
    QObject::connect(w_addressCopy, &QToolButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(w_addressCopy->property("bsAddress").toString(),
                                              QClipboard::Clipboard);
    });

    const QStringList adr = order->address().split(u"\n"_qs);
    int adrSize = int(adr.size());

    setup(w_address1, w_address1Copy, adr.value(0), adrSize >= 1);
    setup(w_address2, w_address2Copy, adr.value(1), adrSize >= 2);
    setup(w_address3, w_address3Copy, adr.value(2), adrSize >= 3);
    setup(w_address4, w_address4Copy, adr.value(3), adrSize >= 4);
    setup(w_address5, w_address5Copy, adr.value(4), adrSize >= 5);
    setup(w_address6, w_address6Copy, adr.value(5), adrSize >= 6);
    setup(w_address7, w_address7Copy, adr.value(6), adrSize >= 7);
    setup(w_address8, w_address8Copy, adr.value(7), adrSize >= 8);
    setup(w_phone, w_phoneCopy, order->phone(), !order->phone().isEmpty(), w_phoneLabel);

    w_countryFlag->setPixmap(QPixmap(u":/assets/flags/" % order->countryCode()));

    w_paymentType->setText(order->paymentType());
    if (!order->paymentStatus().isEmpty() && !order->paymentLastUpdated().isValid()) {
        w_paymentStatus->setText(order->paymentStatus() % u" ("
                                 % loc.toString(order->paymentLastUpdated(), QLocale::ShortFormat)
                                 % u')');
    } else {
        w_paymentStatus->hide();
        w_paymentStatusLabel->hide();
    }

    if (order->currencyCode() == order->paymentCurrencyCode()) {
        w_currencyCode->setText(order->currencyCode());
    } else {
        w_currencyCode->setText(order->currencyCode() % u", " % tr("Payment in") % u' '
                                % order->paymentCurrencyCode());
    }

    bool vatFromSeller = !qFuzzyIsNull(order->vatChargeSeller());
    bool vatFromBL = !qFuzzyIsNull(order->vatChargeBrickLink());

    double vatCharge = vatFromSeller ? order->vatChargeSeller()
                                     : vatFromBL ? order->vatChargeBrickLink()
                                                 : 0;
    double gtGross = order->grandTotal();
    double gtNet = gtGross - vatCharge;
    double vatPercent = Utility::roundTo(100 * (gtGross / (qFuzzyIsNull(gtNet) ? gtGross : gtNet) - 1.0), 0);

    w_vatInfoLabel->setVisible(vatFromSeller);
    w_vatSeparator->setVisible(vatFromSeller);
    w_vatSellerLabel->setText(w_vatSellerLabel->text()
                              .arg(loc.toString(vatPercent, 'f', QLocale::FloatingPointShortest) % u'%'));
    w_vatBLLabel->setText(w_vatBLLabel->text()
                          .arg(loc.toString(vatPercent, 'f', QLocale::FloatingPointShortest) % u'%'));

    setup(w_shipping,        w_shippingCopy,        Currency::toDisplayString(order->shipping(), { }, 2),
          !qFuzzyIsNull(order->shipping()),         w_shippingLabel);
    setup(w_insurance,       w_insuranceCopy,       Currency::toDisplayString(order->insurance(), { }, 2),
          !qFuzzyIsNull(order->insurance()),        w_insuranceLabel);
    setup(w_addCharges1,     w_addCharges1Copy,     Currency::toDisplayString(order->additionalCharges1(), { }, 2),
          !qFuzzyIsNull(order->additionalCharges1()), w_addCharges1Label);
    setup(w_addCharges2,     w_addCharges2Copy,     Currency::toDisplayString(order->additionalCharges2(), { }, 2),
          !qFuzzyIsNull(order->additionalCharges2()), w_addCharges2Label);
    setup(w_credit,          w_creditCopy,          Currency::toDisplayString(order->credit(), { }, 2),
          !qFuzzyIsNull(order->credit()),           w_creditLabel);
    setup(w_creditCoupon,    w_creditCouponCopy,    Currency::toDisplayString(order->creditCoupon(), { }, 2),
          !qFuzzyIsNull(order->creditCoupon()),     w_creditCouponLabel);
    setup(w_total,           w_totalCopy,           Currency::toDisplayString(order->orderTotal(), { }, 2));
    setup(w_salesTax,        w_salesTaxCopy,        Currency::toDisplayString(order->usSalesTax(), { }, 2),
          !qFuzzyIsNull(order->usSalesTax()),       w_salesTaxLabel);
    setup(w_vatBL,           w_vatBLCopy,           Currency::toDisplayString(order->vatChargeBrickLink(), { }, 2),
          vatFromBL,         w_vatBLLabel);
    setup(w_grandTotal,      w_grandTotalCopy,      Currency::toDisplayString(order->grandTotal(), { }, 2));
    setup(w_grossGrandTotal, w_grossGrandTotalCopy, Currency::toDisplayString(gtGross, { }, 2),
          vatFromSeller,     w_grossGrandTotalLabel);
    setup(w_netGrandTotal,   w_netGrandTotalCopy,   Currency::toDisplayString(gtNet, { }, 2),
          vatFromSeller,     w_netGrandTotalLabel);
    setup(w_vatSeller,       w_vatSellerCopy,       Currency::toDisplayString(vatCharge, { }, 2),
          vatFromSeller,     w_vatSellerLabel);

    resize(sizeHint());
}

#include "moc_orderinformationdialog.cpp"
