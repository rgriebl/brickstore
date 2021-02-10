#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QValidator>
#include <QButtonGroup>

#include "currency.h"
#include "smartvalidator.h"
#include "changecurrencydialog.h"

ChangeCurrencyDialog::ChangeCurrencyDialog(const QString &from, const QString &to, QWidget *parent)
    : QDialog(parent), m_from(from), m_to(to)
{
    setupUi(this);

    w_oldCurrency->setText(w_oldCurrency->text().arg(from));

    m_labelEcbFormat = w_labelEcb->text().arg(from);
    m_labelCustomFormat = w_labelCustom->text().arg(from);

    connect(w_updateEcb, &QAbstractButton::clicked,
            Currency::inst(), &Currency::updateRates);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &ChangeCurrencyDialog::ratesUpdated);
    connect(w_newCurrency, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
            this, &ChangeCurrencyDialog::currencyChanged);

    auto *grp = new QButtonGroup(this);
    grp->addButton(w_radioEcb);
    grp->addButton(w_radioCustom);

    w_radioEcb->setChecked(true);

    w_labelEcb->installEventFilter(this);
    w_labelCustom->installEventFilter(this);

    w_editCustom->setValidator(new SmartDoubleValidator(0, 100000, 3, 1, w_editCustom));
    w_editCustom->setText(Currency::toString(1));

    ratesUpdated();
}

bool ChangeCurrencyDialog::eventFilter(QObject *o, QEvent *e)
{
    if (o == w_labelEcb && e->type() == QEvent::MouseButtonPress)
        w_radioEcb->setChecked(true);
    else if (o == w_labelCustom && e->type() == QEvent::MouseButtonPress)
        w_radioCustom->setChecked(true);

    return QDialog::eventFilter(o, e);
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
    bool wasUSD = (m_from == QLatin1String("USD"));

    if (!wasUSD) {
        currencies.removeOne(QLatin1String("USD"));
        currencies.prepend(QLatin1String("USD"));
    }
    w_newCurrency->clear();
    w_newCurrency->insertItems(0, currencies);
    if (!wasUSD && currencies.count() > 1)
        w_newCurrency->insertSeparator(1);
    w_newCurrency->setCurrentIndex(qMax(0, w_newCurrency->findText(oldto)));
}

void ChangeCurrencyDialog::languageChange()
{
    retranslateUi(this);
}

void ChangeCurrencyDialog::currencyChanged(const QString &to)
{
    qreal rateFrom = Currency::inst()->rate(m_from);
    qreal rateTo = Currency::inst()->rate(to);
    m_rate = 1;
    if (!qFuzzyIsNull(rateFrom) && !qFuzzyIsNull(rateTo))
        m_rate = rateTo / rateFrom;

    w_labelEcb->setText(m_labelEcbFormat.arg(to).arg(Currency::toString(m_rate)));
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
