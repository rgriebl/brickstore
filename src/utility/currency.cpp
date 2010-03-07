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

#include "config.h"
#include "utility.h"
#include "currency.h"



QDataStream &operator<<(QDataStream &s, const Currency &m)
{
    s << m.val; return s;
}

QDataStream &operator>>(QDataStream &s, Currency &m)
{
    s >> m.val; return s;
}

QString Currency::s_symint = QLatin1String("USD");
QString Currency::s_sym = QLatin1String("$");
double  Currency::s_rate = 1;

double Currency::rate()
{
    return s_rate;
}

QString Currency::symbol(CurrencySymbol cs)
{
    return cs == LocalSymbol ? s_sym : cs == InternationalSymbol ? s_symint : QString();
}

void Currency::setLocal(const QString &symint, const QString &sym, double rate_to_usd)
{
    s_symint = symint;
    s_sym    = sym;
    s_rate   = rate_to_usd;
}

Currency Currency::fromUSD(const QString &str)
{
    return Currency(QLocale::c().toDouble(str));
}

Currency Currency::fromLocal(const QString &str)
{
    QString s = str.trimmed();

    if (s.isEmpty())
        return Currency();
    else if (s.startsWith(s_symint))
        s = s.mid(s_symint.length()).trimmed();
    else if (s.startsWith(s_sym))
        s = s.mid(s_sym.length()).trimmed();

    //s.replace(QLocale().decimalPoint(), QChar('.'));
    //return Currency(s.toDouble() / s_rate);
    return Currency(QLocale().toDouble(s) / s_rate);
}

QString Currency::toUSD(CurrencySymbol cs, int precision) const
{
    QString s = QLocale::c().toString(toDouble(), 'f', precision);

    if (cs == LocalSymbol)
        return QLatin1String("$ ") + s;
    else if (cs == InternationalSymbol)
        return QLatin1String("USD ") + s;
    else
        return s;
}

QString Currency::toLocal(CurrencySymbol cs, int precision) const
{
    //QString s = QString::number(toDouble() * s_rate, 'f', precision);
    //s.replace(QChar('.'), QLocale().decimalPoint()); //TODO
    QLocale loc;
    loc.setNumberOptions(QLocale::OmitGroupSeparator);
    QString s = loc.toString(toDouble() * s_rate, 'f', precision);

    if (cs == LocalSymbol)
        return s_sym + QLatin1String(" ") + s;
    else if (cs == InternationalSymbol)
        return s_symint + QLatin1String(" ") + s;
    else
        return s;
}




/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

class DotCommaFilter : public QObject {
public:
    DotCommaFilter(QObject *parent)
        : QObject(parent)
    { }

protected:
    bool eventFilter(QObject *o, QEvent *e)
    {
        if (((e->type() == QEvent::KeyPress) || (e->type() == QEvent::KeyRelease)) && qobject_cast<QLineEdit *>(o)) {
            const CurrencyValidator *val = qobject_cast<const CurrencyValidator *>(static_cast<QLineEdit *>(o)->validator());

            if (val) {
                QKeyEvent *ke = static_cast<QKeyEvent *>(e);

                QString text = ke->text();
                bool fixed = false;
                QLocale loc;

                for (int i = 0; i < text.length(); ++i) {
                    QCharRef ir = text[i];
                    if (ir == QLatin1Char('.') || ir == QLatin1Char(',')) {
                        ir = loc.decimalPoint();
                        fixed = true;
                    }
                }

                if (fixed)
                    *ke = QKeyEvent(ke->type(), ke->key(), ke->modifiers(), text, ke->isAutoRepeat(), ke->count());
            }
        }
        return false;
    }
};

bool CurrencyValidator::s_once = false;

CurrencyValidator::CurrencyValidator(QObject *parent)
    : QDoubleValidator(parent)
{
    if (!s_once) {
        s_once = true;
        qApp->installEventFilter(new DotCommaFilter(qApp));
    }
}

CurrencyValidator::CurrencyValidator(Currency bottom, Currency top, int decimals, QObject *parent)
    : QDoubleValidator(bottom.toDouble(), top.toDouble(), decimals, parent)
{
    if (!s_once) {
        s_once = true;
        qApp->installEventFilter(new DotCommaFilter(qApp));
    }
}

QValidator::State CurrencyValidator::validate(QString &input, int &pos) const
{
    double b = bottom(), t = top();
    int d = decimals();

    double f = Currency::rate();

    b *= f;
    t *= f;

    QChar dp = QLocale().decimalPoint();

    QRegExp r(QString(QLatin1String(" *-?\\d*\\%1?\\d* *")).arg(dp));

    if (b >= 0 && input.simplified().startsWith(QLatin1Char('-')))
        return Invalid;

    if (r.exactMatch(input)) {
        QString s = input;
        s.replace(dp, QLatin1Char('.'));

        int i = s.indexOf(QLatin1Char('.'));
        if (i >= 0) {
            // has decimal point, now count digits after that
            i++;
            int j = i;
            while (s [j].isDigit())
                j++;
            if (j > i) {
                while (s [j - 1] == QLatin1Char('0'))
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

