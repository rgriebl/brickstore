/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include <QtCore/QLocale>
#include <QtCore/QStringBuilder>
#include <QtCore/QDataStream>
#include <QtCore/QBuffer>
#include <QtCore/QXmlStreamReader>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "common/config.h"
#include "common/onlinestate.h"
#include "qcoro/qcoronetworkreply.h"
#include "currency.h"


// all rates as downloaded are relative to the Euro, but
// we need everything relative to the US Dollar
static void baseConvert(QHash<QString, double> &rates)
{
    QHash<QString, double>::iterator usd_it = rates.find(u"USD"_qs);

    if (usd_it == rates.end()) {
        rates.clear();
        return;
    }
    double usd_eur = double(1) / usd_it.value();

    for (QHash<QString, double>::iterator it = rates.begin(); it != rates.end(); ++it)
        it.value() = (it == usd_it) ? 1 : it.value() *= usd_eur;

    rates.insert(u"EUR"_qs, usd_eur);
}


Currency *Currency::s_inst = nullptr;

Currency::Currency()
    : m_nam(nullptr)
{
    qint64 tt = Config::inst()->value(u"Rates/LastUpdate"_qs, 0).toLongLong();
    m_lastUpdate = QDateTime::fromSecsSinceEpoch(tt);
    QStringList rates = Config::inst()->value(u"Rates/Normal"_qs).toString().split(u',');
    QStringList customRates = Config::inst()->value(u"Rates/Custom"_qs).toString().split(u',');
    m_legacyRate = Config::inst()->value(u"Rates/Legacy"_qs, 0).toDouble();

    parseRates(rates, m_rates);
    parseRates(customRates, m_customRates);
}

Currency::~Currency()
{
    Config::inst()->setValue(u"Rates/LastUpdate"_qs, m_lastUpdate.toSecsSinceEpoch());

    QStringList sl;
    for (auto it = m_rates.constBegin(); it != m_rates.constEnd(); ++it) {
        QString s = u"%1|%2"_qs;
        sl << s.arg(it.key()).arg(it.value());
    }
    Config::inst()->setValue(u"Rates/Normal"_qs, sl.join(u","));

    sl.clear();
    for (auto it = m_customRates.constBegin(); it != m_customRates.constEnd(); ++it) {
        QString s = u"%1|%2"_qs;
        sl << s.arg(it.key()).arg(it.value());
    }
    Config::inst()->setValue(u"Rates/Custom"_qs, sl.join(u","));

    s_inst = nullptr;
}

Currency *Currency::inst()
{
    if (!s_inst)
        s_inst = new Currency;
    return s_inst;
}

void Currency::parseRates(const QStringList &ratesList, QHash<QString, double> &ratesMap)
{
    for (const QString &s : ratesList) {
        QStringList sl = s.split(QLatin1Char('|'));
        if (sl.count() == 2) {
            QString sym = sl[0];
            double rate = sl[1].toDouble();

            if (!sym.isEmpty() && rate > 0)
                ratesMap.insert(sym, rate);
        }
    }
}


QStringList Currency::currencyCodes() const
{
    QStringList sl = m_rates.keys();
    sl.sort();
    return sl;
}

double Currency::legacyRate() const
{
    return m_legacyRate;
}

QHash<QString, double> Currency::rates() const
{
    return m_rates;
}

QHash<QString, double> Currency::customRates() const
{
    return m_customRates;
}

double Currency::rate(const QString &currencyCode) const
{
    return m_rates.value(currencyCode);
}

double Currency::crossRate(const QString &fromCode, const QString &toCode) const
{
    double f = m_rates.value(fromCode);
    return m_rates.value(toCode) / (qFuzzyIsNull(f) ? 1 : f);
}

double Currency::customRate(const QString &currencyCode) const
{
    return m_customRates.value(currencyCode);
}

void Currency::setCustomRate(const QString &currencyCode, double rate)
{
    m_customRates.insert(currencyCode, rate);
}

void Currency::unsetCustomRate(const QString &currencyCode)
{
    m_customRates.remove(currencyCode);
}

QCoro::Task<> Currency::updateRates(bool silent)
{
    if (!OnlineState::inst()->isOnline())
        co_return;

    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }

    m_silent = silent;
    auto reply = co_await m_nam->get(QNetworkRequest(QUrl(u"https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"_qs)));

    if (reply->error() != QNetworkReply::NoError) {
        if (OnlineState::inst()->isOnline() && !m_silent)
            emit updateRatesFailed(tr("There was an error downloading the exchange rates from the ECB server:")
                                   + u"<br>" + reply->errorString());
    } else {
        auto r = reply->readAll();
        QXmlStreamReader reader(r);
        QHash<QString, double> newRates;

        while (!reader.atEnd()) {
            if ((reader.readNext() == QXmlStreamReader::StartElement) &&
                (reader.name() == u"Cube") &&
                reader.attributes().hasAttribute(u"currency"_qs)) {

                QString currency = reader.attributes().value(u"currency"_qs).toString();
                double rate = reader.attributes().value(u"rate"_qs).toDouble();

                if (currency.length() == 3 && rate > 0)
                    newRates.insert(currency, rate);
            }
        }
        baseConvert(newRates);

        if (reader.hasError() || newRates.isEmpty()) {
            if (!m_silent) {
                QString err;
                if (reader.hasError())
                    err = tr("%1 (line %2, column %3)").arg(reader.errorString())
                            .arg(reader.lineNumber()).arg(reader.columnNumber());
                else
                    err = tr("no currency data found");
                emit updateRatesFailed(tr("There was an error parsing the exchange rates from the ECB server:")
                                       + u"<br>" + err);
            }
        } else {
            m_rates = newRates;
            m_lastUpdate = QDateTime::currentDateTime();
            emit ratesChanged();
        }
    }
    reply->deleteLater();
}

QDateTime Currency::lastUpdate() const
{
    return m_lastUpdate;
}

QString Currency::toDisplayString(double value, const QString &currencyCode, int precision)
{
    static QLocale loc;
    if (currencyCode.isEmpty())
        return loc.toString(value, 'f', precision);
    else
        return currencyCode + u' ' + loc.toString(value, 'f', precision);
}

QString Currency::toString(double value, const QString &currencyCode, int precision)
{
    static QLocale loc;
    loc.setNumberOptions(QLocale::OmitGroupSeparator);
    if (currencyCode.isEmpty())
        return loc.toString(value, 'f', precision);
    else
        return loc.toString(value, 'f', precision) + u' ' + currencyCode;
}

double Currency::fromString(const QString &str)
{
    QString s = str.trimmed();
    if (s.isEmpty())
        return 0;
    return QLocale().toDouble(s);
}

QString Currency::format(double value, const QString &currencyCode, int precision) const
{
    return toDisplayString(value, currencyCode, precision);
}

#include "moc_currency.cpp"
