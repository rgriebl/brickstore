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
#ifndef __CURRENCY_H__
#define __CURRENCY_H__

#include <QString>
#include <QValidator>

class QKeyEvent;
class QLineEdit;
class QDataStream;


class Currency {
public:
    Currency(int v = 0)          : val(qint64(v) * 1000) { }
    Currency(double v)           : val(qint64(v * 1000)) { }
    Currency(const Currency &m)   : val(m.val) { }

    Currency &operator=(int v)              { val = qint64(v) * 1000; return *this; }
    Currency &operator=(double v)           { val = qint64(v * 1000); return *this; }
    Currency &operator=(const Currency &m)   { val = m.val; return *this; }
  //  Currency &operator = (const QString &str) { bool ok = false; Currency m = Money::inst()->toMoney(str, &ok); if (ok) *this = m; return *this; }

    //operator bool ( ) const { return val != 0; }

    Currency &operator+=(const Currency &m)  { val += m.val; return *this; }
    Currency &operator-=(const Currency &m)  { val -= m.val; return *this; }

    Currency &operator*=(double d)  { val = qint64(val * d); return *this; }
    Currency &operator*=(int i)     { val *= i; return *this; }
    Currency &operator/=(double d)  { val = qint64(val / d); return *this; }
    Currency &operator/=(int i)     { val /= i; return *this; }

    friend bool operator< (const Currency &m1, const Currency &m2)  { return m1.val <  m2.val; }
    friend bool operator<=(const Currency &m1, const Currency &m2)  { return m1.val <= m2.val; }
    friend bool operator==(const Currency &m1, const Currency &m2)  { return m1.val == m2.val; }
    friend bool operator!=(const Currency &m1, const Currency &m2)  { return m1.val != m2.val; }
    friend bool operator>=(const Currency &m1, const Currency &m2)  { return m1.val >= m2.val; }
    friend bool operator> (const Currency &m1, const Currency &m2)  { return m1.val >  m2.val; }

    friend Currency operator-(const Currency &m)  { Currency t(m); t.val = -t.val; return t; }
    friend Currency operator+(const Currency &m)  { Currency t(m); return t; }

    friend Currency operator+(const Currency &m1, const Currency &m2)  { Currency t = m1; t += m2; return t; }
    friend Currency operator-(const Currency &m1, const Currency &m2)  { Currency t = m1; t -= m2; return t; }

    friend Currency operator*(const Currency &m1, double d2)  { Currency t = m1; t *= d2; return t; }
    friend Currency operator*(const Currency &m1, int    i2)  { Currency t = m1; t *= i2; return t; }
    friend Currency operator*(double d1, const Currency &m2)  { Currency t = m2; t *= d1; return t; }
    friend Currency operator*(int    i1, const Currency &m2)  { Currency t = m2; t *= i1; return t; }

    friend Currency operator/(const Currency &m1, double d2)  { Currency t = m1; t /= d2; return t; }
    friend Currency operator/(const Currency &m1, int    i2)  { Currency t = m1; t /= i2; return t; }

    friend QDataStream &operator<<(QDataStream &s, const Currency &m);
    friend QDataStream &operator>>(QDataStream &s, Currency &m);

    double toDouble() const { return val / 1000.; }

    qint64 &internalValue() { return val; }

    static void setLocal(const QString &symint, const QString &sym, double rate_to_usd);

    static Currency fromUSD(const QString &);
    static Currency fromLocal(const QString &);

    enum CurrencySymbol {
        NoSymbol = 0,
        LocalSymbol,
        InternationalSymbol
    };

    QString toUSD(CurrencySymbol cs = NoSymbol, int precision = 3) const;
    QString toLocal(CurrencySymbol cs = NoSymbol, int precision = 3) const;

    static double rate();
    static QString symbol(CurrencySymbol = LocalSymbol);

private:
    qint64 val;

    static QString s_symint;
    static QString s_sym;
    static double  s_rate;
};

class CurrencyValidator : public QDoubleValidator {
    Q_OBJECT

public:
    CurrencyValidator(QObject *parent);
    CurrencyValidator(Currency bottom, Currency top, int decimals, QObject *parent);

    virtual QValidator::State validate(QString &input, int &) const;

protected:
    bool filterInput(QLineEdit *edit, QKeyEvent *ke) const;

    static bool s_once;
};

#endif
