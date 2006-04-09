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
#ifndef __CAPPLICATION_H__
#define __CAPPLICATION_H__

#include <qapplication.h>
#include <qstringlist.h>

#if defined( Q_WS_MACX )
#include <Carbon/Carbon.h>
#endif

class CFrameWork;
class QTranslator;

class CApplication : public QApplication {
	Q_OBJECT
public:
	CApplication ( const char *rebuild_db_only, int argc, char **argv );
	virtual ~CApplication ( );

	void enableEmitOpenDocument ( bool b = true );
	
	QString appName ( ) const;
	QString appVersion ( ) const;
	QString appURL ( ) const;
	QString sysName ( ) const;
	QString sysVersion ( ) const;

public slots:
	void about ( );
	void checkForUpdates ( );
	void updateTranslations ( );
	void registration ( );

signals:
	void openDocument ( const QString & );

protected:
	virtual void customEvent ( QCustomEvent *e );
   	
private slots:
	void doEmitOpenDocument ( );
	void demoVersion ( );
	void rebuildDatabase ( );

private:
	bool initBrickLink ( );
	void exitBrickLink ( );

#if defined( Q_WS_MACX )
	static OSErr appleEventHandler ( const AppleEvent *event, AppleEvent *, long );
#endif

private:
	QStringList m_files_to_open;
	bool m_enable_emit;
	QString m_rebuild_db_only;

	QTranslator *m_trans_qt;
	QTranslator *m_trans_brickstore;
};

extern CApplication *cApp;

#endif
