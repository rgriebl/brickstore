/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QDateTime>

#include "qcoro/task.h"

QT_FORWARD_DECLARE_CLASS(QNetworkAccessManager)


class Currency : public QObject
{
    Q_OBJECT

public:
    static Currency *inst();
    ~Currency() override;

    QHash<QString, qreal> rates() const;
    QHash<QString, qreal> customRates() const;
    qreal rate(const QString &currencyCode) const;
    qreal crossRate(const QString &fromCode, const QString &toCode) const;
    qreal customRate(const QString &currencyCode) const;
    QStringList currencyCodes() const;

    void setCustomRate(const QString &currencyCode, qreal rate);
    void unsetCustomRate(const QString &currenyCode);

    QDateTime lastUpdate() const;

    static QString toDisplayString(double value, const QString &currencyCode = { }, int precision = 3);
    static QString toString(double value, const QString &currencyCode = { }, int precision = 3);
    static double fromString(const QString &str);

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
    QHash<QString, qreal> m_rates;
    QHash<QString, qreal> m_customRates;
    QDateTime m_lastUpdate;
    bool m_silent = false;
};
