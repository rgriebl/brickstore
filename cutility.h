/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CUTILITY_H__
#define __CUTILITY_H__

#include <time.h>

#include <QString>
#include <QColor>

class QFontMetrics;
class QDateTime;
class QRect;
class QWidget;


class CUtility {
public:
    static QString ellipsisText(const QString &org, const QFontMetrics &fm, int width, int align);

    static QColor gradientColor(const QColor &c1, const QColor &c2, qreal f = 0.5f);
    static QColor contrastColor(const QColor &c, qreal f = 0.04f);

// static QImage shadeImage ( const QImage &oimg, const QColor &col );

    static void setPopupPos(QWidget *w, const QRect &pos);

// static QString weightToString ( double w, bool imperial = false, bool optimize = false, bool show_unit = false );
// static double stringToWeight ( const QString &s, bool imperial = false );

    //static QString safeOpen ( const QString &basepath );
    static QString safeRename(const QString &basepath);

    static time_t toUTC(const QDateTime &dt, const char *settz = 0);
};

#endif

