#ifndef __CCOMMAND_H__
#define __CCOMMAND_H__

#include <qptrlist.h>

#include "cundo.h"
#include "bricklink.h"

class CWindow;


namespace Command {

class AddRemoveItems : public CUndoCmd {
	Q_OBJECT

public:
	virtual ~AddRemoveItems ( );

	virtual void redo ( );
	virtual void undo ( );
	virtual bool mergeMeWith ( CUndoCmd *other );

protected:
	enum Type { Add, Remove };

	AddRemoveItems ( CWindow *win, Type t, const QPtrList<BrickLink::InvItem> &list, bool can_merge );

	static QPtrList <BrickLink::InvItem> listFromItem ( BrickLink::InvItem *item );

private:
	void add ( );
	void remove ( );

	static QString genDesc ( Type t, uint count );

private:
	QPtrList<BrickLink::InvItem> m_list;
	int *    m_positions;
	CWindow *m_window;
	Type     m_type        : 1;
};


class AddItems : public AddRemoveItems {
public:
	AddItems ( CWindow *win, BrickLink::InvItem *item, bool can_merge = false );
	AddItems ( CWindow *win, const QPtrList<BrickLink::InvItem> &list, bool can_merge = false );
};


class RemoveItems : public AddRemoveItems {
public:
	RemoveItems ( CWindow *win, BrickLink::InvItem *item, bool can_merge = false );
	RemoveItems ( CWindow *win, const QPtrList<BrickLink::InvItem> &list, bool can_merge = false );
};


class EditItems : public CUndoCmd {
	Q_OBJECT

public:
	EditItems ( CWindow *win, BrickLink::InvItem *item_from, BrickLink::InvItem *item_to, bool can_merge = false );
	EditItems ( CWindow *win, const QPtrList<BrickLink::InvItem> &list_from, const QPtrList<BrickLink::InvItem> &list_to, bool can_merge = false );

	virtual void redo ( );
	virtual void undo ( );
	virtual bool mergeMeWith ( CUndoCmd *other );

private:
	static QString genDesc ( uint count );

private:
	QPtrList<BrickLink::InvItem> m_list_from;
	QPtrList<BrickLink::InvItem> m_list_to;
	CWindow *m_window;
	bool     m_from_is_to  : 1;
};

} // namespace Command

#endif
