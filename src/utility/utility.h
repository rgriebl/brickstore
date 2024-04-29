// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <algorithm>
#include <cmath>

#include <QtCore/QString>
#include <QtCore/QLocale>
#include <QtCore/QPair>
#include <QtGui/QColor>
#include <QtGui/QImage>

#if defined(Q_OS_WINDOWS) && defined(max)
#  undef max
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
using QLatin1StringView = QLatin1String;
#endif

QT_FORWARD_DECLARE_CLASS(QFontMetrics)
QT_FORWARD_DECLARE_CLASS(QRect)
QT_FORWARD_DECLARE_CLASS(QWidget)



namespace Utility {

constexpr double fixFinite(double d)
{
    return std::isfinite(d) ? d : 0;
}

constexpr std::partial_ordering fuzzyCompare(double d1, double d2) // just like qFuzzyCompare, but also usable around 0
{
    bool equal = qAbs(d1 - d2) <= 1e-12 * std::max({ 1.0, qAbs(d1), qAbs(d2) });
    return equal ? std::partial_ordering::equivalent : (d1 <=> d2);
}

std::strong_ordering naturalCompare(QAnyStringView s1, QAnyStringView s2);

QColor gradientColor(const QColor &c1, const QColor &c2, float f = 0.5);
QColor textColor(const QColor &backgroundColor);
QColor contrastColor(const QColor &c, float f);
QColor shadeColor(int n, float alpha = 0.f);
QColor premultiplyAlpha(const QColor &c);

QImage stripeImage(int h, const QColor &stripeColor, const QColor &baseColor = Qt::transparent);

QString weightToString(double gramm, QLocale::MeasurementSystem ms, bool optimize = false, bool show_unit = false);
double stringToWeight(const QString &s, QLocale::MeasurementSystem ms);

double roundTo(double f, int decimals);

QString localForInternationalCurrencySymbol(const QString &international_symbol);

QString urlQueryEscape(const QString &str);

inline QString urlQueryEscape(const char *str)  { return urlQueryEscape(QString::fromLatin1(str)); }
inline QString urlQueryEscape(const QByteArray &str)   { return urlQueryEscape(QString::fromLatin1(str)); }

namespace Android {

QString fileNameFromUrl(const QUrl &url);
bool isSideLoaded();

}

} // namespace Utility
