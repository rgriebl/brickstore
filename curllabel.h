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
#ifndef __CURLLABEL_H__
#define __CURLLABEL_H__

#include <q3textbrowser.h>

class CUrlLabel : public Q3TextBrowser {
public:
	CUrlLabel ( QWidget *parent, const char *name = 0 );

	virtual ~CUrlLabel ( );

	virtual QSize minimumSizeHint() const;
	virtual QSize sizeHint() const;
	
protected:
	virtual void setSource ( const QString &src );
};

#endif
