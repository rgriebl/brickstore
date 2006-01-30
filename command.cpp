#include "cwindow.h"

#include "command.h"


namespace {

static QPtrList <BrickLink::InvItem> makeList ( BrickLink::InvItem *item )
{
	QPtrList <BrickLink::InvItem> list;
	list. append ( item );
	return list;
}

} // namespace


namespace Command {

AddItems::AddItems ( CWindow *win, BrickLink::InvItem *item, bool can_merge )
	: AddRemoveItems ( win, Add, makeList ( item ), can_merge )
{ }

AddItems::AddItems ( CWindow *win, const QPtrList<BrickLink::InvItem> &list, bool can_merge )
	: AddRemoveItems ( win, Add, list, can_merge )
{ }

RemoveItems::RemoveItems ( CWindow *win, BrickLink::InvItem *item, bool can_merge )
	: AddRemoveItems ( win, Remove, makeList ( item ), can_merge )
{ }

RemoveItems::RemoveItems ( CWindow *win, const QPtrList<BrickLink::InvItem> &list, bool can_merge )
	: AddRemoveItems ( win, Remove, list, can_merge )
{ }


AddRemoveItems::AddRemoveItems ( CWindow *win, Type t, const QPtrList<BrickLink::InvItem> &list, bool can_merge )
	: CUndoCmd ( genDesc ( t, list. count ( )), can_merge && ( t == Add )), m_list ( list ), m_positions ( 0 ), m_window ( win ), m_type ( t )
{ 
	m_list. setAutoDelete ( t == Remove );
}

AddRemoveItems::~AddRemoveItems ( )
{
	delete [] m_positions;
}

void AddRemoveItems::redo ( )
{
	( m_type == Add ) ? add ( ) : remove ( );
}

void AddRemoveItems::undo ( )
{
	( m_type == Remove ) ? add ( ) : remove ( );
}

bool AddRemoveItems::mergeMeWith ( CUndoCmd *other )
{
	AddRemoveItems *that = ::qt_cast <AddRemoveItems *> ( other );

	if ( this-> m_type == that-> m_type ) {
		while ( !this-> m_list. isEmpty ( ))
			that-> m_list. append ( this-> m_list. take ( 0 ));
		that-> setDescription ( genDesc ( that-> m_type, that-> m_list. count ( )));
		return true;
	}
	return false;
}

void AddRemoveItems::add ( )
{
	if ( !m_positions )
		m_window-> appendItems ( m_list );
	else
		m_window-> insertItems ( m_list, m_positions );
}

void AddRemoveItems::remove ( )
{
	if (( m_type == Remove ) && ( !m_positions )) {
		// find positions to correctly undo the remove operation
		m_positions = new int [m_list. count ( )];
	}
	m_window-> removeItems ( m_list, m_positions );
}

QString AddRemoveItems::genDesc ( Type t, uint count )
{
	if ( t == Add )
		return ( count == 1 ) ? tr( "Added %1 Items" ) : tr( "Added Item" );
	else
		return ( count == 1 ) ? tr( "Removed %1 Items" ) : tr( "Removed Item" );
}

EditItems::EditItems ( CWindow *win, BrickLink::InvItem *item_from, BrickLink::InvItem *item_to, bool can_merge )
	: CUndoCmd ( genDesc ( 1 ), can_merge ), m_list_from ( makeList ( item_from )), m_list_to ( makeList ( item_to )), m_window ( win ), m_from_is_to ( false )
{
}

EditItems::EditItems ( CWindow *win, const QPtrList <BrickLink::InvItem> &list_from, const QPtrList <BrickLink::InvItem> &list_to, bool can_merge )
	: CUndoCmd ( genDesc ( list_from. count ( )), can_merge ), m_list_from ( list_from ), m_list_to ( list_to ), m_window ( win ),  m_from_is_to ( false )
{
}

void EditItems::redo ( )
{
}

void EditItems::undo ( )
{
}

bool EditItems::mergeMeWith ( CUndoCmd *other )
{
	EditItems *that = ::qt_cast <EditItems *> ( other );

	if ( this-> m_from_is_to == that-> m_from_is_to ) {
		while ( !this-> m_list_from. isEmpty ( ))
			that-> m_list_from. append ( this-> m_list_from. take ( 0 ));
		while ( !this-> m_list_to. isEmpty ( ))
			that-> m_list_to. append ( this-> m_list_to. take ( 0 ));
		that-> setDescription ( genDesc ( that-> m_list_from. count ( )));
		return true;
	}
	return false;
}

QString EditItems::genDesc ( uint count )
{
	return ( count == 1 ) ? tr( "Changed %1 Items" ) : tr( "Changed Item" );
}

} // namespace Command
