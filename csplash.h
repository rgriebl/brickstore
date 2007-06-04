/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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
#ifndef __CSPLASH__
#define __CSPLASH__

#include <QSplashScreen>

class CSplash : public QSplashScreen {
	Q_OBJECT

private:
	CSplash ( );
	static CSplash *s_inst;
	static bool s_dont_show;

public:
	virtual ~CSplash ( );
	static CSplash *inst ( );

	void message ( const QString &msg );
};

#endif

