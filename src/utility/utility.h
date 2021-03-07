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

#include <QString>
#include <QColor>
#include <QLocale>
#include <QPair>
#include <QKeySequence>

QT_FORWARD_DECLARE_CLASS(QFontMetrics)
QT_FORWARD_DECLARE_CLASS(QRect)
QT_FORWARD_DECLARE_CLASS(QWidget)

#define qL1S(x) QLatin1String(x)


namespace Utility {

int naturalCompare(const QString &s1, const QString &s2);

QColor gradientColor(const QColor &c1, const QColor &c2, qreal f = 0.5);
QColor textColor(const QColor &backgroundColor);
QColor contrastColor(const QColor &c, qreal f);
QColor premultiplyAlpha(const QColor &c);

void setPopupPos(QWidget *w, const QRect &pos);

QString safeRename(const QString &basepath);
QString sanitizeFileName(const QString &name);

quint64 physicalMemory();

QString weightToString(double gramm, QLocale::MeasurementSystem ms, bool optimize = false, bool show_unit = false);
double stringToWeight(const QString &s, QLocale::MeasurementSystem ms);

double roundTo(double f, int decimals);

QString localForInternationalCurrencySymbol(const QString &international_symbol);

QString toolTipLabel(const QString &label, QKeySequence shortcut = { }, const QString &extended = { });

} // namespace Utility
