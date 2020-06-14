/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#include <QMap>
#include <QDateTime>

class QKeyEvent;
class QLineEdit;
class QNetworkAccessManager;
class QBuffer;

class Currency : public QObject
{
    Q_OBJECT

public:
    static Currency *inst();
    ~Currency();

    QMap<QString, qreal> rates() const;
    QMap<QString, qreal> customRates() const;
    qreal rate(const QString &currencyCode) const;
    qreal customRate(const QString &currencyCode) const;
    QStringList currencyCodes() const;

    void setCustomRate(const QString &currencyCode, qreal rate);
    void unsetCustomRate(const QString &currenyCode);

    QDateTime lastUpdate() const;

    enum Symbol {
        NoSymbol = 0,
        LocalSymbol,
        InternationalSymbol
    };

    static QString toString(double value, const QString &intSymbol = QLatin1String("USD"), Symbol cs = NoSymbol, int precision = 3);
    static double fromString(const QString &str);

    static QString localSymbol(const QString &intSymbol);

public slots:
    void updateRates();

signals:
    void ratesChanged();

private:
    Currency();
    Currency(const Currency &);
    static Currency *s_inst;

    static void parseRates(const QStringList &ratesList, QMap<QString, double> &ratesMap);

    QNetworkAccessManager *m_nam;
    QMap<QString, qreal> m_rates;
    QMap<QString, qreal> m_customRates;
    QDateTime m_lastUpdate;
};


class CurrencyValidator : public QDoubleValidator {
    Q_OBJECT

public:
    CurrencyValidator(QObject *parent);
    CurrencyValidator(double bottom, double top, int decimals, QObject *parent);

    virtual QValidator::State validate(QString &input, int &) const;

protected:
    bool filterInput(QLineEdit *edit, QKeyEvent *ke) const;

    static bool s_once;
};

#endif
