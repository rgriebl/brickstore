#ifndef __CUNDO_P_H__
#define __CUNDO_P_H__

#include <qaction.h>

class QListBox;
class QListBoxItem;
class QPopupMenu;
class QLabel;
class CUndoManager;
class CUndoStack;


class CUndoAction : public QAction {
	Q_OBJECT

public slots:
	virtual void setTextSlot ( const QString &str ); // why isn't QAction::setText() a slot?

private:
	friend class CUndoManager;
	friend class CUndoStack;
	
	CUndoAction ( QObject *parent, const char *name = 0 );
};

class CUndoListAction : public QAction {
	Q_OBJECT

signals:
	void activated ( int );

protected:
	virtual void addedTo ( QWidget *w, QWidget *cont );
	virtual bool eventFilter ( QObject *o, QEvent *e );

protected slots:
	void updateDescriptions ( );
	void fixMenu ( );
	void itemSelected ( QListBoxItem * );
	void selectRange ( QListBoxItem * );
	void setCurrentItemSlot ( QListBoxItem *item );

private:
	friend class CUndoManager;
	friend class CUndoStack;
	
	enum Type {
		Undo,
		Redo
	};

	CUndoListAction ( Type t, QObject *parent, const char *name = 0 );
	
private:
	Type        m_type;
	QPopupMenu *m_menu;
	QListBox *  m_list;
	QLabel *    m_label;

	static const char *s_strings [];
};

#endif
