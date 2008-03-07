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
#ifndef __CUTILITY_H__
#define __CUTILITY_H__

#include <QString>
#include <QColor>

class QFontMetrics;
class QRect;
class QWidget;


class CUtility {
public:
    static QString ellipsisText(const QString &org, const QFontMetrics &fm, int width, int align);

    static QColor gradientColor(const QColor &c1, const QColor &c2, qreal f = 0.5f);
    static QColor contrastColor(const QColor &c, qreal f = 0.04f);
    static float colorDifference(const QColor &c1, const QColor &c2);

    static void setPopupPos(QWidget *w, const QRect &pos);

    static QString safeRename(const QString &basepath);
};

#endif

