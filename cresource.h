/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#ifndef __CRESOURCE_H__
#define __CRESOURCE_H__

#include <qasciidict.h>
#include <qstringlist.h>
#include <qiconset.h>
#include <qpixmap.h>

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
	QIconSet iconSet ( const char *name );
	QPixmap pixmap ( const char *name );

private:
	QStringList m_paths;
	QAsciiDict<QIconSet> m_iconset_dict;
	QAsciiDict<QPixmap>  m_pixmap_dict;
	bool m_has_alpha;
};

#endif
