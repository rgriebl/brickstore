/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <QString>
#include <QColor>
#include <QLocale>
#include <QPair>

class QFontMetrics;
class QRect;
class QWidget;


namespace Utility {

int naturalCompare(const char *s1, const char *s2);

QColor gradientColor(const QColor &c1, const QColor &c2, qreal f = 0.5f);
QColor contrastColor(const QColor &c, qreal f = 0.04f);
qreal colorDifference(const QColor &c1, const QColor &c2);

void setPopupPos(QWidget *w, const QRect &pos);

QString safeRename(const QString &basepath);

quint64 physicalMemory();

QString weightToString(double gramm, QLocale::MeasurementSystem ms, bool optimize = false, bool show_unit = false);
double stringToWeight(const QString &s, QLocale::MeasurementSystem ms);

QPair<QString, QString> currencySymbolsForCountry(QLocale::Country c);
QLocale::Country countryForCurrencySymbol(const QString &international_symbol);

} // namespace Utility

#endif

