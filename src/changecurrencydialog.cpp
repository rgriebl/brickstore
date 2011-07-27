#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QValidator>

#include "currency.h"

#include "changecurrencydialog.h"

ChangeCurrencyDialog::ChangeCurrencyDialog(const QString &from, const QString &to, QWidget *parent)
    : QDialog(parent), m_from(from), m_to(to)
{
    setupUi(this);

    QString s;
    s = w_oldCurrency->text();
    w_oldCurrency->setText(w_oldCurrency->text().arg(from));

    m_labelEcbFormat = w_labelEcb->text().arg(from);
    m_labelCustomFormat = w_labelCustom->text().arg(from);

    connect(w_updateEcb, SIGNAL(clicked()), Currency::inst(), SLOT(updateRates()));
    connect(Currency::inst(), SIGNAL(ratesChanged()), this, SLOT(ratesUpdated()));
    connect(w_newCurrency, SIGNAL(currentIndexChanged(QString)), this, SLOT(currencyChanged(QString)));

    QButtonGroup *grp = new QButtonGroup(this);
    grp->addButton(w_radioEcb);
    grp->addButton(w_radioCustom);

    w_radioEcb->setChecked(true);

    w_labelEcb->installEventFilter(this);
    w_labelCustom->installEventFilter(this);

    w_editCustom->setValidator(new QDoubleValidator(0, 100000, 3, w_editCustom));
    w_editCustom->setText(QString::number(double(1), 'f', 3));

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

void ChangeCurrencyDialog::currencyChanged(const QString &to)
{
    qreal rateFrom = Currency::inst()->rate(m_from);
    qreal rateTo = Currency::inst()->rate(to);
    m_rate = 1;
    if (rateFrom && rateTo)
        m_rate = rateTo / rateFrom;

    w_labelEcb->setText(m_labelEcbFormat.arg(to).arg(m_rate, 0, 'f', 3));
    w_labelCustom->setText(m_labelCustomFormat.arg(to));
}

double ChangeCurrencyDialog::exchangeRate() const
{
    double rate = m_rate;
    if (w_radioCustom->isChecked())
        rate = w_editCustom->text().toDouble();
    return rate;
}
