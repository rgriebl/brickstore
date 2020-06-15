/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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

#include <qstring.h>
#include <qcolor.h>
#include <QtGui/qimage.h>

class QFontMetrics;
class QDateTime;

class CUtility {
public:
	static QString ellipsisText ( const QString &org, const QFontMetrics &fm, int width, int align );

	static int compareColors ( const QColor &c1, const QColor &c2 );
	static QColor gradientColor ( const QColor &c1, const QColor &c2, float f = 0.5f );
	static QColor contrastColor ( const QColor &c, float f = 0.04f );

	static QImage createGradient ( const QSize &size, Qt::Orientation orient, const QColor &c1, const QColor &c2, float f = 100.f );
	static QImage shadeImage ( const QImage &oimg, const QColor &col );

	static bool openUrl ( const QString &url );
	
	static void setPopupPos ( QWidget *w, const QRect &pos );

	static QString weightToString ( double w, bool imperial = false, bool optimize = false, bool show_unit = false );
	static double stringToWeight ( const QString &s, bool imperial = false );

	static QString safeOpen ( const QString &basepath );
	static QString safeRename ( const QString &basepath );

	static time_t toUTC ( const QDateTime &dt, const char *settz = 0 );

    static bool copyDir ( const QString source, const QString destination, const bool override );
    static bool emptyDir ( const QString dirName, const bool remove = false );
};

#endif

