/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CMONEY_H__
#define __CMONEY_H__

#include <qstring.h>
#include <qvalidator.h>
#include <qmap.h>
#include <qvariant.h>

class CMoneyData;
class QKeyEvent;
class QLineEdit;
class QDataStream;

class money_t;


class CMoney : public QObject {
    Q_OBJECT

private:
    CMoney();
    static CMoney *s_inst;

public:
    ~CMoney();
    static CMoney *inst();

    void setFactor(double f);
    double factor() const;

    void setPrecision(int prec);
    int precision() const;

    void setLocalization(bool b);
    bool isLocalized() const;

    QString currencySymbol() const;
    QString localCurrencySymbol() const;

    QChar localDecimalPoint() const;
    QChar decimalPoint() const;

private:
    QString toString(double dv, bool with_currency_symbol, int precision) const;
    money_t toMoney(const QString &s, bool *ok = 0) const;

    friend class money_t;

signals:
    void monetarySettingsChanged();

private:
    CMoneyData *d;
};

class CMoneyValidator : public QDoubleValidator {
    Q_OBJECT

public:
    CMoneyValidator(QObject *parent);
    CMoneyValidator(money_t bottom, money_t top, int decimals, QObject *parent);

    virtual QValidator::State validate(QString &input, int &) const;

protected:
    bool filterInput(QLineEdit *edit, QKeyEvent *ke) const;

    friend class CMoneyData;
};

class money_t {
public:
    money_t (int v = 0)          : val(qint64(v) * 1000) { }
    money_t (double v)           : val(qint64(v * 1000)) { }
    money_t (const money_t &m)   : val(m.val) { }

    money_t &operator = (int v)              { val = qint64(v) * 1000; return *this; }
    money_t &operator = (double v)           { val = qint64(v * 1000); return *this; }
    money_t &operator = (const money_t &m)   { val = m.val; return *this; }
    money_t &operator = (const QString &str) { bool ok = false; money_t m = CMoney::inst()->toMoney(str, &ok); if (ok) *this = m; return *this; }

    //operator bool ( ) const { return val != 0; }

    money_t &operator += (const money_t &m)  { val += m.val; return *this; }
    money_t &operator -= (const money_t &m)  { val -= m.val; return *this; }

    money_t &operator *= (double d)  { val = qint64(val * d); return *this; }
    money_t &operator *= (int i)     { val *= i; return *this; }
    money_t &operator /= (double d)  { val = qint64(val / d); return *this; }
    money_t &operator /= (int i)     { val /= i; return *this; }

    friend bool operator < (const money_t &m1, const money_t &m2)  { return m1.val <  m2.val; }
    friend bool operator <= (const money_t &m1, const money_t &m2)  { return m1.val <= m2.val; }
    friend bool operator == (const money_t &m1, const money_t &m2)  { return m1.val == m2.val; }
    friend bool operator != (const money_t &m1, const money_t &m2)  { return m1.val != m2.val; }
    friend bool operator >= (const money_t &m1, const money_t &m2)  { return m1.val >= m2.val; }
    friend bool operator > (const money_t &m1, const money_t &m2)  { return m1.val >  m2.val; }

    friend money_t operator - (const money_t &m)  { money_t t(m); t.val = -t.val; return t; }
    friend money_t operator + (const money_t &m)  { money_t t(m); return t; }

    friend money_t operator + (const money_t &m1, const money_t &m2)  { money_t t = m1; t += m2; return t; }
    friend money_t operator - (const money_t &m1, const money_t &m2)  { money_t t = m1; t -= m2; return t; }

    friend money_t operator * (const money_t &m1, double d2)  { money_t t = m1; t *= d2; return t; }
    friend money_t operator * (const money_t &m1, int    i2)  { money_t t = m1; t *= i2; return t; }
    friend money_t operator * (double d1, const money_t &m2)  { money_t t = m2; t *= d1; return t; }
    friend money_t operator * (int    i1, const money_t &m2)  { money_t t = m2; t *= i1; return t; }

    friend money_t operator / (const money_t &m1, double d2)  { money_t t = m1; t /= d2; return t; }
    friend money_t operator / (const money_t &m1, int    i2)  { money_t t = m1; t /= i2; return t; }

    friend QDataStream &operator << (QDataStream &s, const money_t &m);
    friend QDataStream &operator >> (QDataStream &s, money_t &m);

    QString toCString(bool with_currency_symbol = false, int precision = 3) const;
    QString toLocalizedString(bool with_currency_symbol = false, int precision = 3) const;

    static money_t fromCString(const QString &);
    static money_t fromLocalizedString(const QString &);

    double toDouble() const;

    qint64 &internalValue() { return val; }

private:
    qint64 val;
};



#endif
