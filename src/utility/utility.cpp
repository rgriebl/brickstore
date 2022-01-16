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

#include <cmath>

#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QStringBuilder>
#include <QtGui/QColor>
#include <QtGui/QPalette>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>

#include "utility.h"


static int naturalCompareNumbers(const QChar *&n1, const QChar *&n2)
{
    int result = 0;

    while (true) {
        const auto d1 = (n1++)->digitValue();
        const auto d2 = (n2++)->digitValue();

        if (d1 == -1 && d2 == -1)
            return result;
        else if (d1 == -1)
            return -1;
        else if (d2 == -1)
            return 1;
        else if (d1 != d2 && !result)
            result = d1 - d2;
    }
}

int Utility::naturalCompare(const QString &name1, const QString &name2)
{
    bool empty1 = name1.isEmpty();
    bool empty2 = name2.isEmpty();

    if (empty1 && empty2)
        return 0;
    else if (empty1)
        return -1;
    else if (empty2)
        return 1;

    bool special = false;
    const QChar *n1 = name1.constData();
    const QChar *n1e = n1 + name1.size();
    const QChar *n2 = name2.constData();
    const QChar *n2e = n2 + name2.size();

    while (true) {
        // 1) skip white space
        while ((n1 < n1e) && n1->isSpace()) {
            n1++;
            special = true;
        }
        while ((n2 < n2e) && n2->isSpace()) {
            n2++;
            special = true;
        }

        // 2) check for numbers
        if ((n1 < n1e) && n1->isDigit() && (n2 < n2e) && n2->isDigit()) {
            int d = naturalCompareNumbers(n1, n2);
            if (d)
                return d;
            special = true;
        }


        // 4) naturally the same -> let the unicode order decide
        if ((n1 >= n1e) && (n2 >= n2e))
            return special ? name1.localeAwareCompare(name2) : 0;

        // 5) found a difference
        if (n1 >= n1e)
            return -1;
        else if (n2 >= n2e)
            return 1;
        else if (*n1 != *n2)
            return n1->unicode() - n2->unicode();

        n1++; n2++;
    }
}

QColor Utility::gradientColor(const QColor &c1, const QColor &c2, qreal f)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
#else
    float r1, g1, b1, a1, r2, g2, b2, a2;
#endif
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    f = qBound(qreal(0), f, qreal(1));
    qreal e = qreal(1) - f;

    return QColor::fromRgbF(r1 * e + r2 * f, g1 * e + g2 * f, b1 * e + b2 * f, a1 * e + a2 * f);
}

QColor Utility::textColor(const QColor &bg)
{
    // see https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef

    auto adjust = [](qreal c) {
        return (c <= 0.03928) ? c / 12.92 : std::pow(((c + 0.055) / 1.055), 2.4);
    };

    auto luminance = [](qreal r, qreal g, qreal b) {
        return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    };

    auto r = adjust(bg.redF());
    auto g = adjust(bg.greenF());
    auto b = adjust(bg.blueF());
    auto l = luminance(r, g, b);

    auto cw = (1. + 0.05) / (l + 0.05); // contrast to white
    auto cb = (l + 0.05) / (0. + 0.05); // contrast to black

    return (cw > cb) ? Qt::white : Qt::black;
}

QColor Utility::contrastColor(const QColor &c, qreal f)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qreal h, s, l, a;
#else
    float h, s, l, a;
#endif
    c.getHslF(&h, &s, &l, &a);

    f = qBound(qreal(0), f, qreal(1));

    l += f * ((l <= qreal(0.55)) ? qreal(1) : qreal(-1));
    l = qBound(qreal(0), l, qreal(1));

    return QColor::fromHslF(h, s, l, a);
}

QString Utility::weightToString(double w, QLocale::MeasurementSystem ms, bool optimize, bool show_unit)
{
    QLocale loc;
    if (!optimize)
        loc.setNumberOptions(QLocale::OmitGroupSeparator);

    int decimals = 0;
    const char *unit = nullptr;

    if (ms != QLocale::MetricSystem) {
        w *= 0.035273961949580412915675808215204;

        decimals = 3;
        unit = "oz";

        if (optimize && (w >= 32)) {
            unit = "lb";
            w /= 16.;
        }
    }
    else {
        decimals = 2;
        unit = "g";

        if (optimize && (w >= 1000.)) {
            unit = "kg";
            w /= 1000.;

            if (w >= 1000.) {
                unit = "t";
                w /= 1000.;
            }
        }
    }
    QString s = loc.toString(w, 'f', decimals);

    if (show_unit) {
        s.append(QLatin1Char(' '));
        s.append(QLatin1String(unit));
    }
    return s;
}

double Utility::stringToWeight(const QString &s, QLocale::MeasurementSystem ms)
{
    QLocale loc;

    double w = loc.toDouble(s);
    int decimals = 2;

    if (ms == QLocale::ImperialSystem) {
        w *= 28.349523125000000000000000000164;
        decimals = 3;
    }

    return roundTo(w, decimals);
}

QString Utility::localForInternationalCurrencySymbol(const QString &international_symbol)
{
    const auto allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                                     QLocale::AnyCountry);

    for (const auto &locale : allLocales) {
        if (locale.currencySymbol(QLocale::CurrencyIsoCode) == international_symbol)
            return locale.currencySymbol(QLocale::CurrencySymbol);
    }
    return international_symbol;
}


QColor Utility::premultiplyAlpha(const QColor &c)
{
    if (c.alpha()) {
        QColor r = QColor(qPremultiply(c.rgba()));
        r.setAlpha(255);
        return r;
    }
    return c;

}

double Utility::roundTo(double d, int n)
{
    Q_ASSERT((n >= 0) && (n <= 4));
    const int f = n == 0 ? 1 : n == 1 ? 10 : n == 2 ? 100 : n == 3 ? 1000 : 10000;
    return std::round(d * f) / f;
}

QImage Utility::stripeImage(int h, const QColor &stripeColor, const QColor &baseColor)
{
    QImage img(2 * h, 2 * h, QImage::Format_ARGB32);
    img.fill(baseColor);
    QPainter imgp(&img);
    imgp.setPen(Qt::transparent);
    QLinearGradient grad(0, h, h, 0);
    QColor c = stripeColor;
    c.setAlpha(16);
    grad.setColorAt(0, c);
    c.setAlpha(64);
    grad.setColorAt(0.2, c);
    c.setAlpha(32);
    grad.setColorAt(0.5, c);
    c.setAlpha(64);
    grad.setColorAt(0.8, c);
    c.setAlpha(16);
    grad.setColorAt(1, c);
    imgp.setBrush(grad);
    imgp.drawPolygon(QPolygon() << QPoint(h, 0) << QPoint(2 * h, 0) << QPoint(0, 2 * h) << QPoint(0, h));
    imgp.setBrush(c);
    imgp.drawPolygon(QPolygon() << QPoint(2 * h, h) << QPoint(2 * h, 2 * h) << QPoint(h, 2 * h));
    imgp.end();
    return img;
}

QString Utility::urlQueryEscape(const QString &str)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(str));
}

