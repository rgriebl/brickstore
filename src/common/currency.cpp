// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QLocale>
#include <QtCore/QStringBuilder>
#include <QtCore/QDataStream>
#include <QtCore/QBuffer>
#include <QtCore/QXmlStreamReader>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <QCoro/QCoroNetworkReply>

#include "common/config.h"
#include "utility/exception.h"
#include "common/onlinestate.h"
#include "currency.h"

struct RateProviderDefinition {
    Currency::RateProvider rateProvider;
    QString id;
    QString name;
    QString homepageUrl;
    QString euroFeedUrl;
    std::function<QHash<QString, double>(const QByteArray &)> feedParser;
};

static const QVector<RateProviderDefinition> &rateProviderDefinitions()
{
    static const QVector<RateProviderDefinition> p = {
        {
         Currency::RateProvider::ECB,
         u"ECB"_qs,
         u"European Central Bank"_qs,
         u"https://www.ecb.europa.eu/"_qs,
         u"https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"_qs,
         [](const QByteArray &data) -> QHash<QString, double> {
             QHash<QString, double> rates;
             QXmlStreamReader xml(data);
             while (!xml.atEnd()) {
                 xml.readNext();
                 if (xml.isStartElement() && xml.name() == u"Cube"_qs) {
                     QString ccode = xml.attributes().value(u"currency"_qs).toString();
                     double rate = xml.attributes().value(u"rate"_qs).toDouble();
                     if ((ccode.size() == 3) && (rate > 0))
                         rates.insert(ccode, rate);
                 }
             }
             if (xml.hasError()) {
                 throw Exception(u"%1 (line %2, column %3)"_qs.arg(xml.errorString())
                                     .arg(xml.lineNumber()).arg(xml.columnNumber()));
             }
             // all rates as downloaded are relative to the Euro, but
             // we need everything relative to the US Dollar
             auto usd_it = rates.find(u"USD"_qs);
             if ((usd_it == rates.end()) || (*usd_it == 0))
                 throw Exception("missing USD rate in ECB data");

             double eur_usd = 1 / *usd_it;
             for (auto &r : rates)
                 r *= eur_usd;
             rates.insert(u"EUR"_qs, eur_usd);

             return rates;
         } },
        {
         Currency::RateProvider::FloatRates,
         u"FloatRates"_qs,
         u"FloatRates"_qs,
         u"https://www.floatrates.com/"_qs,
         u"https://www.floatrates.com/daily/usd.xml"_qs,
         [](const QByteArray &data) -> QHash<QString, double> {
             QHash<QString, double> rates;
             QXmlStreamReader xml(data);
             while (!xml.atEnd()) {
                 xml.readNext();
                 if (xml.isStartElement() && xml.name() == u"item"_qs) {
                     QString ccode;
                     double rate = 0;
                     while (xml.readNextStartElement()) {
                         if (xml.name() == u"targetCurrency"_qs)
                             ccode = xml.readElementText();
                         else if (xml.name() == u"exchangeRate"_qs)
                             rate = xml.readElementText().toDouble();
                         else
                             xml.skipCurrentElement();
                     }
                     if ((ccode.size() == 3) && (rate > 0))
                         rates.insert(ccode, rate);
                 }
             }
             if (xml.hasError()) {
                 throw Exception(u"%1 (line %2, column %3)"_qs.arg(xml.errorString())
                                     .arg(xml.lineNumber()).arg(xml.columnNumber()));
             }
             return rates;
         } }
    };
    return p;
}


Currency *Currency::s_inst = nullptr;

Currency::Currency()
    : m_nam(nullptr)
{
    qint64 tt = Config::inst()->value(u"Rates/LastUpdate"_qs, 0).toLongLong();
    m_lastUpdate = QDateTime::fromSecsSinceEpoch(tt);
    m_legacyRate = Config::inst()->value(u"Rates/Legacy"_qs, 0).toDouble();

    for (const auto &p : rateProviderDefinitions()) {
        const QStringList ratesList = Config::inst()->value(u"Rates/" + p.id).toString().split(u',');
        for (const QString &s : ratesList) {
            const QStringList sl = s.split(u'|');
            if (sl.count() == 2) {
                QString ccode = sl[0];
                double rate = sl[1].toDouble();

                if ((ccode.length() == 3) && (rate > 0))
                    m_rates[p.rateProvider][ccode] = rate;
            }
        }
    }
}

Currency::~Currency()
{
    Config::inst()->setValue(u"Rates/LastUpdate"_qs, m_lastUpdate.toSecsSinceEpoch());

    for (const auto &p : rateProviderDefinitions()) {
        QStringList sl;
        sl.reserve(rateProviderDefinitions().size());

        const auto rateHash = std::as_const(m_rates).value(p.rateProvider);
        for (auto it = rateHash.cbegin(); it != rateHash.cend(); ++it)
            sl << u"%1|%2"_qs.arg(it.key()).arg(it.value());
        Config::inst()->setValue(u"Rates/" + p.id, sl.join(u","));
    }
    s_inst = nullptr;
}

Currency *Currency::inst()
{
    if (!s_inst)
        s_inst = new Currency;
    return s_inst;
}

QStringList Currency::currencyCodes() const
{
    QStringList ccodes;
    for (const auto &p : rateProviderDefinitions())
        ccodes << m_rates[p.rateProvider].keys();
    ccodes.removeDuplicates();
    ccodes.sort();
    return ccodes;
}

QVector<Currency::RateProvider> Currency::rateProviders() const
{
    QVector<RateProvider> result;
    result.reserve(rateProviderDefinitions().size());
    for (const auto &p : rateProviderDefinitions())
        result << p.rateProvider;
    return result;
}

QString Currency::rateProviderName(RateProvider provider) const
{
    for (const auto &p : rateProviderDefinitions()) {
        if (p.rateProvider == provider)
            return p.name;
    }
    return { };
}

QUrl Currency::rateProviderUrl(RateProvider provider) const
{
    for (const auto &p : rateProviderDefinitions()) {
        if (p.rateProvider == provider)
            return p.homepageUrl;
    }
    return { };
}

QStringList Currency::rateProviderCurrencyCodes(RateProvider provider) const
{
    auto ccodes = m_rates.value(provider).keys();
    ccodes.sort();
    return ccodes;
}

Currency::RateProvider Currency::rateProviderForCurrencyCode(const QString &currencyCode) const
{
    for (const auto &p : rateProviderDefinitions()) {
        if (m_rates.value(p.rateProvider).contains(currencyCode))
            return p.rateProvider;
    }
    return RateProvider::Invalid;
}

double Currency::legacyRate() const
{
    return m_legacyRate;
}

double Currency::rate(const QString &currencyCode) const
{
    for (const auto &p : rateProviderDefinitions()) {
        const auto rates = m_rates.value(p.rateProvider);
        if (auto it = rates.constFind(currencyCode); it != rates.cend())
            return it.value();
    }
    return 0;
}

double Currency::crossRate(const QString &fromCode, const QString &toCode) const
{
    double f = rate(fromCode);
    return rate(toCode) / (qFuzzyIsNull(f) ? 1 : f);
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

    QVector<QNetworkReply *> replies;
    replies.reserve(rateProviderDefinitions().size());
    for (const auto &p : rateProviderDefinitions())
        replies << m_nam->get(QNetworkRequest(p.euroFeedUrl));

    decltype(m_rates) newRates;
    QSet<QString> seenCurrencyCodes { u"RUB"_qs, u"BYN"_qs };

    for (qsizetype i = 0; i < replies.size(); ++i) {
        auto reply = replies.at(i);
        co_await reply;
        reply->deleteLater();

        auto &p = rateProviderDefinitions().at(i);

        QHash<QString, double> rates;
        try {
            if (reply->error() != QNetworkReply::NoError)
                throw Exception(reply->errorString());

            rates = p.feedParser(reply->readAll());
            if (rates.isEmpty())
                throw Exception(tr("no currency data found"));

            rates.removeIf([&](auto it) {
                if (seenCurrencyCodes.contains(it.key())) {
                    return true;
                } else {
                    seenCurrencyCodes.insert(it.key());
                    return false;
                }
            });
            newRates.insert(p.rateProvider, rates);
        } catch (const Exception &e) {
            if (OnlineState::inst()->isOnline() && !m_silent) {
                emit updateRatesFailed(tr("Failed to download exchange rates. <a href='%2'>%1</a>:")
                                           .arg(p.name, p.homepageUrl) + u"<br>" + e.errorString());
            }
        }
    }

    if (!newRates.isEmpty()) {
        m_rates = newRates;
        m_lastUpdate = QDateTime::currentDateTime();
        emit ratesChanged();
    }
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
