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

QT_FORWARD_DECLARE_CLASS(QFontMetrics)
QT_FORWARD_DECLARE_CLASS(QRect)
QT_FORWARD_DECLARE_CLASS(QWidget)



namespace Utility {

constexpr double fixFinite(double d)
{
    return std::isfinite(d) ? d : 0;
}

constexpr bool fuzzyCompare(double d1, double d2) // just like qFuzzyCompare, but also usable around 0
{
    return qAbs(d1 - d2) <= 1e-12 * std::max({ 1.0, qAbs(d1), qAbs(d2) });
}

int naturalCompare(const QString &s1, const QString &s2);

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

} // namespace Utility
