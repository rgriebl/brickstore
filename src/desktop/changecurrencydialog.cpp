// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QLocale>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QValidator>
#include <QButtonGroup>
#include <QMouseEvent>
#include <QPushButton>
#include <QMenu>

#include "common/config.h"
#include "common/currency.h"
#include "common/eventfilter.h"
#include "changecurrencydialog.h"
#include "smartvalidator.h"

using namespace std::placeholders;

ChangeCurrencyDialog::ChangeCurrencyDialog(const QString &from, QWidget *parent)
    : QDialog(parent)
    , m_from(from)
    , m_to(from)
{
    setupUi(this);

    w_updateProvider->setProperty("iconScaling", true);

    w_oldCurrency->setText(w_oldCurrency->text().arg(from));

    m_labelProviderFormat = w_labelProvider->text().arg(from);
    m_labelCustomFormat = w_labelCustom->text().arg(from);

    connect(w_updateProvider, &QAbstractButton::clicked,
            Currency::inst(), &Currency::updateRates);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &ChangeCurrencyDialog::ratesUpdated);

    auto *m = new QMenu(w_newCurrency);
    w_newCurrency->setMenu(m);
    connect(m, &QMenu::aboutToShow, this, [this, m]() {
        m->clear();

        auto setup = [this](QAction *a, const QString &ccode) {
            a->setObjectName(ccode);
            if (ccode == m_from)
                a->setEnabled(false);
            return a;
        };

        const QString defaultCode = Config::inst()->defaultCurrencyCode();
        if (!defaultCode.isEmpty()) {
            setup(m->addAction(tr("Default currency (%1)").arg(defaultCode)), defaultCode);
            m->addSeparator();
        }

        const auto rateProviders = Currency::inst()->rateProviders();
        for (const auto &rp : rateProviders) {
            auto rpm = m->addMenu(Currency::inst()->rateProviderName(rp));

            const auto ccodes = Currency::inst()->rateProviderCurrencyCodes(rp);
            for (const auto &ccode : ccodes)
                setup(rpm->addAction(ccode), ccode);
        }
    });
    connect(m, &QMenu::triggered, this, [this](QAction *a) {
         currencyChanged(a->objectName());
    });
    currencyChanged(m_from);

    auto *grp = new QButtonGroup(this);
    grp->addButton(w_radioProvider);
    grp->addButton(w_radioCustom);
    w_radioProvider->setChecked(true);

    auto checkRadioOnLabelClick = [](QRadioButton *r, QObject *, QEvent *e) {
        if (static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)
            r->setChecked(true);
        return EventFilter::ContinueEventProcessing;
    };

    new EventFilter(w_labelProvider, { QEvent::MouseButtonPress }, std::bind(checkRadioOnLabelClick, w_radioProvider, _1, _2));
    new EventFilter(w_labelCustom,   { QEvent::MouseButtonPress }, std::bind(checkRadioOnLabelClick, w_radioCustom, _1, _2));

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
    currencyChanged(m_to);
}

void ChangeCurrencyDialog::languageChange()
{
    retranslateUi(this);
}

void ChangeCurrencyDialog::currencyChanged(const QString &to)
{
    m_to = to;
    m_rate = Currency::inst()->crossRate(m_from, m_to);

    auto rp = Currency::inst()->rateProviderForCurrencyCode(m_to);

    w_newCurrency->setText(m_to + u"  ");
    w_labelProvider->setText(m_labelProviderFormat.arg(to, Currency::toDisplayString(m_rate),
                                                       Currency::inst()->rateProviderName(rp),
                                                       Currency::inst()->rateProviderUrl(rp).toString()));
    w_labelCustom->setText(m_labelCustomFormat.arg(m_to));

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(m_from != m_to);
}

QString ChangeCurrencyDialog::currencyCode() const
{
    return m_to;
}

double ChangeCurrencyDialog::exchangeRate() const
{
    double rate = m_rate;
    if (w_radioCustom->isChecked())
        rate = Currency::fromString(w_editCustom->text());
    return rate;
}

#include "moc_changecurrencydialog.cpp"
