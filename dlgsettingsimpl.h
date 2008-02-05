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
#ifndef __DLGSETTINGSIMPL_H__
#define __DLGSETTINGSIMPL_H__

#include <qvaluelist.h>

#include "dlgsettings.h"

class CItemTypeCombo;

class DlgSettingsImpl : public DlgSettings {
    Q_OBJECT

public:
    DlgSettingsImpl ( QWidget *parent = 0, const char * name = 0, bool modal = false, WFlags fl = 0 );
    ~DlgSettingsImpl ( );

	void setCurrentPage ( const char *page );

protected slots:
	void selectCacheDir ( );
	void selectDocDir ( );
	void done ( int );

private:
	bool readAvailableLanguages ( );

private:
	CItemTypeCombo *m_def_add_itemtype;
	CItemTypeCombo *m_def_inv_itemtype;

	typedef QValueList<QPair<QString, QString> >   LanguageList;
	LanguageList m_languages;
};

#endif
