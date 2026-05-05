// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <array>
#include <cmath>

#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QStringBuilder>
#include <QtGui/QColor>
#include <QtGui/QPalette>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>

#if defined(Q_OS_ANDROID)
#  include <QCoreApplication>
#  include <jni.h>
#  include <QJniObject>
#endif

#include "utility.h"


std::strong_ordering Utility::naturalCompare(QAnyStringView name1, QAnyStringView name2)
{
    return name1.visit([name2](auto name1) {
        return name2.visit([name1](auto name2) {
            auto charAt = [](const auto v, const qsizetype pos) constexpr -> QChar {
                using Tv = decltype(v);
                if constexpr (std::is_same_v<Tv, const QLatin1StringView>) {
                    return QChar(v.at(pos));
                } else if constexpr (std::is_same_v<Tv, const QStringView>) {
                    return v.at(pos);
                } else {
                    Q_ASSERT_X(false, "naturalCompare", "only works on UTF-16 and Latin-1 strings");
                    return { };
                }
            };

            bool special = false;
            QChar c1, c2;
            qsizetype pos1 = 0, pos2 = 0;
            const qsizetype len1 = name1.size();
            const qsizetype len2 = name2.size();

            auto next1 = [&]() constexpr { c1 = (pos1 < len1) ? charAt(name1, pos1++) : QChar(); };
            auto next2 = [&]() constexpr { c2 = (pos2 < len2) ? charAt(name2, pos2++) : QChar(); };

            while (true) {
                next1();
                next2();

                // 1) skip white space
                int skipWs1 = 0, skipWs2 = 0;
                while (c1.isSpace()) {
                    next1();
                    ++skipWs1;
                }
                while (c2.isSpace()) {
                    next2();
                    ++skipWs2;
                }
                if (skipWs1 != skipWs2)
                    special = true;

                // 2) check for numbers
                if (c1.isDigit() && c2.isDigit()) {
                    const auto d = [&]() {
                        std::strong_ordering result = std::strong_ordering::equivalent;

                        while (true) {
                            const auto d1 = c1.digitValue();
                            const auto d2 = c2.digitValue();
                            const bool invalid1 = (d1 == -1);
                            const bool invalid2 = (d2 == -1);

                            if (invalid1 && invalid2)
                                return result;
                            else if (invalid1)
                                return std::strong_ordering::less;
                            else if (invalid2)
                                return std::strong_ordering::greater;
                            else if ((d1 != d2) && (result == 0))
                                result = d1 <=> d2;

                            next1(); next2();
                        }
                    }();
                    if (d != 0)
                        return d;
                }

                // 4) naturally the same -> let the unicode order decide
                if (c1.isNull() && c2.isNull())
                    return (special ? QAnyStringView::compare(name1, name2) : 0) <=> 0;

                // 5) found a difference
                else if (c1.isNull())
                    return std::strong_ordering::less;
                else if (c2.isNull())
                    return std::strong_ordering::greater;
                else if (c1 != c2)
                    return c1.unicode() <=> c2.unicode();
            }
        });
    });
}

QColor Utility::gradientColor(const QColor &c1, const QColor &c2, float f)
{
    float r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    f = std::clamp(f, 0.f, 1.f);
    float e = 1.f - f;

    return QColor::fromRgbF(r1 * e + r2 * f, g1 * e + g2 * f, b1 * e + b2 * f, a1 * e + a2 * f);
}

QColor Utility::textColor(const QColor &bg)
{
    // see https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef

    auto adjust = [](float c) {
        return (c <= 0.03928f) ? c / 12.92f : std::pow(((c + 0.055f) / 1.055f), 2.4f);
    };

    auto luminance = [](float r, float g, float b) {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    };

    auto r = adjust(bg.redF());
    auto g = adjust(bg.greenF());
    auto b = adjust(bg.blueF());
    auto l = luminance(r, g, b);

    auto cw = (1.f + 0.05f) / (l + 0.05f); // contrast to white
    auto cb = (l + 0.05f) / (0.f + 0.05f); // contrast to black

    return (cw > cb) ? Qt::white : Qt::black;
}

QColor Utility::contrastColor(const QColor &c, float f)
{
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);

    f = std::clamp(f, 0.f, 1.f);

    l += f * ((l <= 0.55f) ? 1.f : -1.f);
    l = std::clamp(l, 0.f, 1.f);

    return QColor::fromHslF(h, s, l, a);
}

QColor Utility::shadeColor(int n, float alpha)
{
    static std::array<QColor, 12> shades;
    static bool once = false;
    if (!once) [[unlikely]] {
        for (uint i = 0; i < 12; i++)
            shades[i] = QColor::fromHsv(30 * int(i), 255, 255).toRgb();
        once = true;
    }

    QColor c = shades[uint(n) % 12];
    if (!qFuzzyIsNull(alpha))
        c.setAlphaF(alpha);
    return c;
}

QString Utility::weightToString(double w, QLocale::MeasurementSystem ms, bool optimize, bool show_unit)
{
    QLocale loc;
    if (!optimize)
        loc.setNumberOptions(QLocale::OmitGroupSeparator);

    int decimals = 0;
    QString unit;

    if (ms != QLocale::MetricSystem) {
        w *= 0.035273961949580412915675808215204;

        decimals = 3;
        unit = u"oz"_qs;

        if (optimize && (w >= 32)) {
            unit = u"lb"_qs;
            w /= 16.;
        }
    }
    else {
        decimals = 2;
        unit = u"g"_qs;

        if (optimize && (w >= 1000.)) {
            unit = u"kg"_qs;
            w /= 1000.;

            if (w >= 1000.) {
                unit = u"t"_qs;
                w /= 1000.;
            }
        }
    }
    QString s = loc.toString(w, 'f', decimals);

    if (show_unit)
        s = s + u' ' + unit;
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
        auto r = QColor(qPremultiply(c.rgba()));
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

QString Utility::Android::fileNameFromUrl(const QUrl &url)
{
#if defined(Q_OS_ANDROID)
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject jniUrl = QJniObject::fromString(url.toString());
    auto jniFileName = activity.callObjectMethod("getFileName", "(Ljava/lang/String;)Ljava/lang/String;", jniUrl.object<jstring>());
    auto fileName = jniFileName.toString();
    qWarning() << "ANDROID: url=" << url << " -> filename=" << fileName;
    return fileName;
#else
    Q_UNUSED(url)
    return { };
#endif
}

bool Utility::Android::isSideLoaded()
{
#if defined(Q_OS_ANDROID)
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    return activity.callMethod<jboolean>("isSideLoaded");
#else
    return false;
#endif

}
