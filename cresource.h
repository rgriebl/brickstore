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
#ifndef __CRESOURCE_H__
#define __CRESOURCE_H__

#include <q3asciidict.h>
#include <qstringlist.h>
#include <qicon.h>
#include <qpixmap.h>
//Added by qt3to4:
#include <QTranslator>

#if defined( Q_OS_MACX )
#include <CoreFoundation/CFLocale.h>
#endif

class QTranslator;
class QLocale;

class CResource {
private:
	CResource ( );
	static CResource *s_inst;

public:
	~CResource ( );
	static CResource *inst ( );

	bool pixmapAlphaSupported ( ) const;

	enum LocateType { LocateFile, LocateDir };
	
	QString locate ( const QString &name, LocateType lt = LocateFile );

	QTranslator *translation ( const char *name, const QLocale &locale );
    QIcon icon ( const char *name );
	QPixmap pixmap ( const char *name );

#if defined( Q_OS_MACX )
    QString toQString(CFStringRef str);
#endif

private:
	QStringList m_paths;
    Q3AsciiDict<QIcon> m_iconset_dict;
	Q3AsciiDict<QPixmap>  m_pixmap_dict;
	bool m_has_alpha;
};

#endif
