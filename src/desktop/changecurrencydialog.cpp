// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QLocale>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QValidator>
#include <QButtonGroup>
#include <QMouseEvent>

#include "common/currency.h"
#include "common/eventfilter.h"
#include "changecurrencydialog.h"
#include "smartvalidator.h"

using namespace std::placeholders;


ChangeCurrencyDialog::ChangeCurrencyDialog(const QString &from, const QString &to,
                                           bool wasLegacy, QWidget *parent)
    : QDialog(parent)
    , m_from(from)
    , m_to(to)
    , m_wasLegacy(wasLegacy)
{
    setupUi(this);

    w_oldCurrency->setText(w_oldCurrency->text().arg(from));

    m_labelEcbFormat = w_labelEcb->text().arg(from);
    m_labelCustomFormat = w_labelCustom->text().arg(from);

    connect(w_updateEcb, &QAbstractButton::clicked,
            Currency::inst(), &Currency::updateRates);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &ChangeCurrencyDialog::ratesUpdated);
    connect(w_newCurrency, &QComboBox::currentTextChanged,
            this, &ChangeCurrencyDialog::currencyChanged);

    auto *grp = new QButtonGroup(this);
    grp->addButton(w_radioEcb);
    grp->addButton(w_radioCustom);

    auto checkRadioOnLabelClick = [](QRadioButton *r, QObject *, QEvent *e) {
        if (static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)
            r->setChecked(true);
        return EventFilter::ContinueEventProcessing;
    };

    new EventFilter(w_labelEcb,    { QEvent::MouseButtonPress }, std::bind(checkRadioOnLabelClick, w_radioEcb, _1, _2));
    new EventFilter(w_labelCustom, { QEvent::MouseButtonPress }, std::bind(checkRadioOnLabelClick, w_radioCustom, _1, _2));
    new EventFilter(w_labelLegacy, { QEvent::MouseButtonPress }, std::bind(checkRadioOnLabelClick, w_radioLegacy, _1, _2));

    if (m_from != u"USD")
        m_wasLegacy = false;

    if (m_wasLegacy) {
        const QString legacyCCode = QLocale::system().currencySymbol(QLocale::CurrencyIsoCode);
        double legacyRate = Currency::inst()->legacyRate();

        if (!legacyCCode.isEmpty() && !qFuzzyIsNull(legacyRate) && (m_to == legacyCCode)) {
            w_labelLegacy->setText(w_labelLegacy->text().arg(legacyCCode,
                                                             Currency::toDisplayString(legacyRate)));
        } else {
            m_wasLegacy = false;
        }
    }
    if (m_wasLegacy) {
        grp->addButton(w_radioLegacy);
        w_radioLegacy->setChecked(true);
    } else {
        w_widgetLegacy->hide();
        w_radioEcb->setChecked(true);
    }

    w_editCustom->setValidator(new SmartDoubleValidator(0, 100000, 3, 1, w_editCustom));
    w_editCustom->setText(Currency::toDisplayString(1));

    ratesUpdated();
}

void ChangeCurrencyDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ChangeCurrencyDialog::ratesUpdated()
{
    QString oldto = m_to;
    QStringList currencies = Currency::inst()->currencyCodes();
    currencies.sort();
    currencies.removeOne(m_from);
    bool wasUSD = (m_from == u"USD");

    if (!wasUSD) {
        currencies.removeOne(u"USD"_qs);
        currencies.prepend(u"USD"_qs);
    }
    w_newCurrency->clear();
    w_newCurrency->insertItems(0, currencies);
    if (!wasUSD && currencies.count() > 1)
        w_newCurrency->insertSeparator(1);
    w_newCurrency->setCurrentIndex(std::max(0, w_newCurrency->findText(oldto)));
}

void ChangeCurrencyDialog::languageChange()
{
    retranslateUi(this);
}

void ChangeCurrencyDialog::currencyChanged(const QString &to)
{
    double rateFrom = Currency::inst()->rate(m_from);
    double rateTo = Currency::inst()->rate(to);
    m_rate = 1;
    if (!qFuzzyIsNull(rateFrom) && !qFuzzyIsNull(rateTo))
        m_rate = rateTo / rateFrom;

    w_labelEcb->setText(m_labelEcbFormat.arg(to, Currency::toDisplayString(m_rate)));
    w_labelCustom->setText(m_labelCustomFormat.arg(to));
}

double ChangeCurrencyDialog::exchangeRate() const
{
    double rate = m_rate;
    if (w_radioCustom->isChecked())
        rate = Currency::fromString(w_editCustom->text());
    return rate;
}

#include "moc_changecurrencydialog.cpp"
