// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QDateTime>

#include <QCoro/QCoroTask>

QT_FORWARD_DECLARE_CLASS(QNetworkAccessManager)


class Currency : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QDateTime lastUpdate READ lastUpdate NOTIFY ratesChanged)
    Q_PROPERTY(QStringList currencyCodes READ currencyCodes NOTIFY ratesChanged)

public:
    static Currency *inst();
    ~Currency() override;

    Q_INVOKABLE double rate(const QString &currencyCode) const;
    Q_INVOKABLE double crossRate(const QString &fromCode, const QString &toCode) const;
    double legacyRate() const;

    QStringList currencyCodes() const;

    enum class RateProvider {
        ECB = 0,
        FloatRates,

        Invalid = -1
    };

    QVector<RateProvider> rateProviders() const;
    QString rateProviderName(RateProvider provider) const;
    QUrl rateProviderUrl(RateProvider provider) const;
    QStringList rateProviderCurrencyCodes(RateProvider provider) const;
    RateProvider rateProviderForCurrencyCode(const QString &currencyCode) const;

    QDateTime lastUpdate() const;

    static QString toDisplayString(double value, const QString &currencyCode = { }, int precision = 3);
    static QString toString(double value, const QString &currencyCode = { }, int precision = 3);
    static double fromString(const QString &str);

    Q_INVOKABLE QString format(double value, const QString &currencyCode = { }, int precision = 3) const;

public slots:
    QCoro::Task<> updateRates(bool silent = false);

signals:
    void ratesChanged();
    void updateRatesFailed(const QString &errorString);

private:
    Currency();
    Currency(const Currency &);
    static Currency *s_inst;

    QNetworkAccessManager *m_nam;
    QHash<RateProvider, QHash<QString, double>> m_rates;
    std::vector<std::tuple<QString, RateProvider, double>> m_r; // ccode (sorted), provider, rate
    QDateTime m_lastUpdate;
    bool m_silent = false;
    double m_legacyRate = 0;
};
