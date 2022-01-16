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

#include <algorithm>
#include <cmath>

#include <QString>
#include <QColor>
#include <QLocale>
#include <QPair>
#include <QImage>
#include <QStringBuilder>

#if defined(Q_OS_WINDOWS) && defined(max)
#  undef max
#endif

QT_FORWARD_DECLARE_CLASS(QFontMetrics)
QT_FORWARD_DECLARE_CLASS(QRect)
QT_FORWARD_DECLARE_CLASS(QWidget)


constexpr inline QLatin1String operator ""_l1(const char *c, std::size_t s)
{
    return QLatin1String(c, int(s));
}

constexpr inline QChar operator ""_l1(const char c)
{
    return QLatin1Char(c);
}

inline static double fixFinite(double d)
{
    return std::isfinite(d) ? d : 0;
}

// just like qFuzzyCompare, but also usable around 0
inline bool fuzzyCompare(double d1, double d2)
{
    return std::abs(d1 - d2) <= 1e-12 * std::max({ 1.0, std::abs(d1), std::abs(d2) });
}

namespace Utility {

int naturalCompare(const QString &s1, const QString &s2);

QColor gradientColor(const QColor &c1, const QColor &c2, qreal f = 0.5);
QColor textColor(const QColor &backgroundColor);
QColor contrastColor(const QColor &c, qreal f);
QColor premultiplyAlpha(const QColor &c);

QImage stripeImage(int h, const QColor &stripeColor, const QColor &baseColor = Qt::transparent);

QString weightToString(double gramm, QLocale::MeasurementSystem ms, bool optimize = false, bool show_unit = false);
double stringToWeight(const QString &s, QLocale::MeasurementSystem ms);

double roundTo(double f, int decimals);

QString localForInternationalCurrencySymbol(const QString &international_symbol);

QString urlQueryEscape(const QString &str);

inline QString urlQueryEscape(const char *str)  { return urlQueryEscape(QString::fromLatin1(str)); }
inline QString urlQueryEscape(const QByteArray &str)   { return urlQueryEscape(QString::fromLatin1(str)); }

} // namespace Utility
