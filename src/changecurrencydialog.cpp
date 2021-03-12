#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QValidator>
#include <QButtonGroup>
#include <QMouseEvent>

#include "currency.h"
#include "utility.h"
#include "config.h"
#include "smartvalidator.h"
#include "changecurrencydialog.h"


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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(w_newCurrency, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
#else
    connect(w_newCurrency, &QComboBox::currentTextChanged,
#endif
            this, &ChangeCurrencyDialog::currencyChanged);

    auto *grp = new QButtonGroup(this);
    grp->addButton(w_radioEcb);
    grp->addButton(w_radioCustom);

    w_labelEcb->installEventFilter(this);
    w_labelCustom->installEventFilter(this);

    if (m_from != "USD"_l1)
        m_wasLegacy = false;

    if (m_wasLegacy) {
        auto legacy = Config::inst()->legacyCurrencyCodeAndRate();
        if (!legacy.first.isEmpty() && !qFuzzyIsNull(legacy.second) && (m_to == legacy.first)) {
            w_labelLegacy->setText(w_labelLegacy->text().arg(legacy.first,
                                                             Currency::toString(legacy.second)));
            w_labelLegacy->installEventFilter(this);
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
    w_editCustom->setText(Currency::toString(1));

    ratesUpdated();
}

bool ChangeCurrencyDialog::eventFilter(QObject *o, QEvent *e)
{
    if ((e->type() == QEvent::MouseButtonPress)
            && (static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)) {
        if (o == w_labelEcb)
            w_radioEcb->setChecked(true);
        else if (o == w_labelCustom)
            w_radioCustom->setChecked(true);
        else if (o == w_labelLegacy)
            w_radioLegacy->setChecked(true);
    }

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
    bool wasUSD = (m_from == "USD"_l1);

    if (!wasUSD) {
        currencies.removeOne("USD"_l1);
        currencies.prepend("USD"_l1);
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

    w_labelEcb->setText(m_labelEcbFormat.arg(to, Currency::toString(m_rate)));
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
