#ifndef __CWORKSPACE_H__
#define __CWORKSPACE_H__

#include <qwidget.h>
#include <qwidgetlist.h>
#include <qptrdict.h>
#include <qtabbar.h>

class QToolButton;
class QWorkspace;
class QWidgetStack;


class CWorkspace : public QWidget {
	Q_OBJECT

public:
	CWorkspace ( QWidget *parent = 0, const char *name = 0 );

	enum Mode {
		MDI,
		Tabbed
	};

	Mode mode ( ) const;
	void setMode ( Mode m );

	enum WindowOrder { 
		CreationOrder, 
		StackingOrder 
	};

	QWidget *activeWindow ( ) const;
	QWidgetList windowList ( WindowOrder order ) const;

	void activateWindow ( QWidget *w );

public slots:
	void cascade ( );
	void tile ( );
	void closeActiveWindow ( );
	void closeAllWindows ( );
	void activateNextWindow ( );
	void activatePrevWindow ( );

signals:
	void windowActivated ( QWidget *w );

protected:
	virtual bool eventFilter ( QObject *o, QEvent *e );
	virtual void childEvent ( QChildEvent *e );
	void removeAndActivateNext ( QWidget *w );
	void checkTabBarVisible ( );

private slots:
	void activateTabbed ( int );
	void childDestroyed ( );
	void closeTabClicked ( );

private:
	Mode               m_mode        : 2;
	bool               m_block_close : 1;
	bool               m_reparenting : 1;
	QWorkspace *       m_workspace;
	QWidgetStack *     m_stack;
	QTabBar *          m_tabbar;
	QToolButton *      m_close;
	QWidgetList        m_windows;
	QPtrDict <QWidget> m_tab2window;
	QPtrDict <QTab>    m_window2tab;
};

#endif

