/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <cstdlib>
#include <clocale>
#include <climits>

#include <QApplication>
#include <QLocale>
#include <QValidator>
#include <QRegExp>
#include <QLineEdit>
#include <QKeyEvent>
#include <QDataStream>

#if defined( Q_OS_MACX )

#  include <CoreFoundation/CFLocale.h>

// copied and simplified to static functions from private/qcore_mac_p.h
class QCFString {
public:
    static QString toQString(CFStringRef cfstr);
    static CFStringRef toCFStringRef(const QString &str);
};

#endif


#include "config.h"
#include "money.h"

QDataStream &operator << (QDataStream &s, const money_t &m)
{
    s << m.val; return s;
}

QDataStream &operator >> (QDataStream &s, money_t &m)
{
    s >> m.val; return s;
}

double money_t::toDouble() const
{
    return double(val) / 1000.;
}

QString money_t::toCString(bool with_currency_symbol, int precision) const
{
    QString s = QLocale::c().toString(toDouble(), 'f', precision);
    if (with_currency_symbol)
        s.prepend("$ ");
    return s;
}

QString money_t::toLocalizedString(bool with_currency_symbol, int precision) const
{
    return Money::inst()->toString(toDouble(), with_currency_symbol, precision);
}

money_t money_t::fromCString(const QString &str)
{
    return money_t (QLocale::c().toDouble(str));
}

money_t money_t::fromLocalizedString(const QString &str)
{
    return Money::inst()->toMoney(str);
}



class MoneyData : public QObject {
public:
    MoneyData()
            : m_locale(QLocale::system())
    {
        qApp->installEventFilter(this);
    }

    virtual ~MoneyData()
    {
        qApp->removeEventFilter(this);
    }

    virtual bool eventFilter(QObject *o, QEvent *e)
    {
        bool res = false;

        if (((e->type() == QEvent::KeyPress) || (e->type() == QEvent::KeyRelease)) && qobject_cast<QLineEdit *> (o)) {
            const QValidator *val = static_cast <QLineEdit *>(o)->validator();

            if (val && qobject_cast<const MoneyValidator *> (val))
                res = static_cast <const MoneyValidator *>(val)->filterInput(static_cast <QLineEdit *>(o), static_cast <QKeyEvent *>(e));
        }
        return res;
    }

    bool    m_localized;
    int     m_precision;
    double  m_factor;

    QLocale m_locale;
    QString m_csymbol;
    QChar   m_decpoint;
};


Money *Money::s_inst = 0;

Money *Money::inst()
{
    if (!s_inst)
        s_inst = new Money();
    return s_inst;
}

Money::Money()
{
    d = new MoneyData;

    d->m_factor    = Config::inst()->value("/General/Money/Factor",    1.).toDouble();
    d->m_localized = Config::inst()->value("/General/Money/Localized", false).toBool();
    d->m_precision = Config::inst()->value("/General/Money/Precision", 3).toInt();

    QString decpoint;
    QString csymbol, csymbolint;

#if defined( Q_OS_MACX )
    // MACOSX does not set the native environment

    // we need at least 10.3 for CFLocale...stuff (weak import)
    if (CFLocaleCopyCurrent != 0 && CFLocaleGetIdentifier != 0 && CFLocaleGetValue != 0) {
        CFLocaleRef loc = CFLocaleCopyCurrent();
        CFStringRef ds = (CFStringRef) CFLocaleGetValue(loc, kCFLocaleDecimalSeparator);
        CFStringRef cs = (CFStringRef) CFLocaleGetValue(loc, kCFLocaleCurrencySymbol);
        CFStringRef cc = (CFStringRef) CFLocaleGetValue(loc, kCFLocaleCurrencyCode);

        decpoint = QCFString::toQString(ds);
        csymbol = QCFString::toQString(cs);
        csymbolint = QCFString::toQString(cc);

        QString locstr = QCFString::toQString((CFStringRef) CFLocaleGetIdentifier(loc));
        if (locstr.indexOf('.') == -1)
            locstr += QLatin1String(".UTF-8");

        ::setenv("LC_ALL", locstr.toUtf8().constData(), 1);
        ::setlocale(LC_ALL, "");
    }

#else
    ::setlocale(LC_ALL, "");     // initialize C-locale to OS supplied values
    ::lconv *lc = ::localeconv();

    if (lc->mon_decimal_point && *lc->mon_decimal_point && *lc->mon_decimal_point != CHAR_MAX)
        decpoint = QString::fromLocal8Bit(lc->mon_decimal_point)[0];

    csymbol = QString::fromLocal8Bit(lc->currency_symbol);
    csymbolint = QString::fromLocal8Bit(lc->int_curr_symbol);
#endif

    if (!decpoint.isEmpty())
        d->m_decpoint = decpoint [0];
    else
        d->m_decpoint = d->m_locale.toString(1.5, 'f', 1)[1];     // simple hack as fallback

    if (!csymbol.isEmpty())
        d->m_csymbol = csymbol;
    else
        d->m_csymbol = csymbolint;
}

Money::~Money()
{
    delete d;
    s_inst = 0;
}

QString Money::localCurrencySymbol() const
{
    return d->m_csymbol;
}

QString Money::currencySymbol() const
{
    return d->m_localized ? d->m_csymbol : QChar('$');
}

QChar Money::localDecimalPoint() const
{
    return d->m_decpoint;
}

QChar Money::decimalPoint() const
{
    return d->m_localized ? d->m_decpoint : QChar('.');
}

void Money::setLocalization(bool loc)
{
    if (loc != d->m_localized) {
        d->m_localized = loc;
        Config::inst()->setValue("/General/Money/Localized", loc);

        emit monetarySettingsChanged();
    }
}

bool Money::isLocalized() const
{
    return d->m_localized;
}

void Money::setFactor(double f)
{
    if (f != d->m_factor) {
        d->m_factor = f;
        Config::inst()->setValue("/General/Money/Factor", f);

        emit monetarySettingsChanged();
    }
}

double Money::factor() const
{
    return d->m_factor;
}

void Money::setPrecision(int prec)
{
    if (prec != d->m_precision) {
        d->m_precision = prec;
        Config::inst()->setValue("/General/Money/Precision", prec);

        emit monetarySettingsChanged();
    }
}

int Money::precision() const
{
    return d->m_precision;
}

QString Money::toString(double v, bool with_currency_symbol, int precision) const
{
    if (d->m_localized)
        v *= d->m_factor;

    QString s = QString::number(v, 'f', precision);

    if (d->m_localized)
        s.replace(QChar('.'), d->m_decpoint);

    if (with_currency_symbol)
        return currencySymbol() + " " + s;
    else
        return s;
}

money_t Money::toMoney(const QString &str, bool *ok) const
{
    QString s = str.simplified();

    if (!s.isEmpty() && s.startsWith(currencySymbol()))
        s = s.mid(currencySymbol().length()).simplified();

    if (d->m_localized)
        s.replace(d->m_decpoint, QChar('.'));

    money_t v = s.toDouble(ok);

    if (d->m_localized)
        return v /= d->m_factor;

    return v;
}



MoneyValidator::MoneyValidator(QObject *parent)
        : QDoubleValidator(parent)
{ }

MoneyValidator::MoneyValidator(money_t bottom, money_t top, int decimals, QObject *parent)
        : QDoubleValidator(bottom.toDouble(), top.toDouble(), decimals, parent)
{ }

QValidator::State MoneyValidator::validate(QString &input, int &pos) const
{
    double b = bottom(), t = top();
    int d = decimals();

    if (Money::inst()->isLocalized()) {
        double f = Money::inst()->factor();

        b *= f;
        t *= f;
    }

    QChar dp = Money::inst()->decimalPoint();

    QRegExp r(QString(" *-?\\d*\\%1?\\d* *").arg(dp));

    if (b >= 0 && input.simplified().startsWith(QString::fromLatin1("-")))
        return Invalid;

    if (r.exactMatch(input)) {
        QString s = input;
        s.replace(dp, QChar('.'));

        int i = s.indexOf('.');
        if (i >= 0) {
            // has decimal point, now count digits after that
            i++;
            int j = i;
            while (s [j].isDigit())
                j++;
            if (j > i) {
                while (s [j - 1] == '0')
                    j--;
            }

            if (j - i > d) {
                return Intermediate;
            }
        }

        double entered = s.toDouble();

        if (entered < b || entered > t)
            return Intermediate;
        else
            return Acceptable;
    }
    else if (r.matchedLength() == (int) input.length()) {
        return Intermediate;
    }
    else {
        pos = input.length();
        return Invalid;
    }
}

bool MoneyValidator::filterInput(QLineEdit * /*edit*/, QKeyEvent *ke) const
{
    QString text = ke->text();
    bool fixed = false;

    for (int i = 0; i < int(text.length()); i++) {
        QCharRef ir = text [i];
        if (ir == '.' || ir == ',') {
            ir = Money::inst()->decimalPoint();
            fixed = true;
        }
    }

    if (fixed)
        *ke = QKeyEvent(ke->type(), ke->key(), ke->modifiers(), text, ke->isAutoRepeat(), ke->count());
    return false;
}
