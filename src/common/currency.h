// Copyright (C) 2004-2024 Robert Griebl
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

    QHash<QString, double> rates() const;
    QHash<QString, double> customRates() const;
    Q_INVOKABLE double rate(const QString &currencyCode) const;
    Q_INVOKABLE double crossRate(const QString &fromCode, const QString &toCode) const;
    Q_INVOKABLE double customRate(const QString &currencyCode) const;
    QStringList currencyCodes() const;
    double legacyRate() const;

    Q_INVOKABLE void setCustomRate(const QString &currencyCode, double rate);
    Q_INVOKABLE void unsetCustomRate(const QString &currenyCode);

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

    static void parseRates(const QStringList &ratesList, QHash<QString, double> &ratesMap);

    QNetworkAccessManager *m_nam;
    QHash<QString, double> m_rates;
    QHash<QString, double> m_customRates;
    QDateTime m_lastUpdate;
    bool m_silent = false;
    double m_legacyRate = 0;
};
