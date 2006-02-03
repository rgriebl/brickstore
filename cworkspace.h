#ifndef __CWORKSPACE_H__
#define __CWORKSPACE_H__

#include <qworkspace.h>
#include <qwidgetlist.h>
#include <qptrdict.h>
#include <qtabbar.h>

class QToolButton;
class QMainWindow;


class CWorkspace : public QWorkspace {
	Q_OBJECT

public:
	CWorkspace ( QMainWindow *parent, const char *name = 0 );

	bool showTabs ( ) const;
	void setShowTabs ( bool b );

	bool spreadSheetTabs ( ) const;
	void setSpreadSheetTabs ( bool b );

protected:
	virtual bool eventFilter ( QObject *o, QEvent *e );
	virtual void childEvent ( QChildEvent *e );

private slots:
	void tabClicked ( int );
	void setActiveTab ( QWidget * );

private:
	void refillContainer ( QWidget *container );

private:
	bool               m_showtabs  : 1;
	bool               m_exceltabs : 1;
	QMainWindow *      m_mainwindow;
	QTabBar *          m_tabbar;
	QPtrDict <QTab>    m_widget2tab;
};

#endif

