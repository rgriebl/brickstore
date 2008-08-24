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

#include <qapplication.h>
#include <qtoolbutton.h>
#include <qlistbox.h>
#include <qlabel.h>
#include <qpopupmenu.h>
#include <qstyle.h>
#include <qpainter.h>

#include "cundo.h"
#include "cundo_p.h"


class CUndoEmitter {
public:
	CUndoEmitter ( CUndoStack *stack = 0 )
	{
		m_explicit  = ( stack != 0 );
		m_stack     = stack ? stack : CUndoManager::inst ( )-> currentStack ( );
		m_can_undo  = m_stack ? m_stack-> canUndo ( ) : false;
		m_can_redo  = m_stack ? m_stack-> canRedo ( ) : false;
		m_undo_desc = m_stack ? m_stack-> undoDescription ( ) : QString ( );
		m_redo_desc = m_stack ? m_stack-> redoDescription ( ) : QString ( );
		m_current   = m_stack ? m_stack-> m_current. current ( ) : 0;
		m_clean     = m_stack ? m_stack-> isClean ( ) : false;
	}

	~CUndoEmitter ( )
	{
		int mask = None;
		CUndoStack *stack = m_explicit ? m_stack : CUndoManager::inst ( )-> currentStack ( );

		if ( stack != m_stack ) {
			mask = All;
		}
		else if ( stack ) {
			if ( m_can_undo != stack-> canUndo ( ))
				mask |= CanUndo;
			if ( m_can_redo != stack-> canRedo ( ))
				mask |= CanRedo;
			if ( m_undo_desc != stack-> undoDescription ( ))
				mask |= UndoDesc;
			if ( m_redo_desc != stack-> redoDescription ( ))
				mask |= RedoDesc;
			if ( m_current != stack-> m_current. current ( ))
				mask |= ( UndoDesc | RedoDesc );
			if ( m_clean != stack-> isClean ( ))
				mask |= Clean;
		}
		if ( m_explicit )
			doEmits ( mask, stack, stack );
		else
			doEmits ( mask, stack, CUndoManager::inst ( ));
	}

	template <typename T> void doEmits ( int mask, CUndoStack *stack, T *sender )
	{
		if ( mask & CanUndo )
			emit sender-> canUndoChanged ( stack ? stack-> canUndo ( ) : false );
		if ( mask & CanRedo )
			emit sender-> canRedoChanged ( stack ? stack-> canRedo ( ) : false );
		if ( mask & UndoDesc )
			emit sender-> undoDescriptionChanged ( stack ? stack-> undoDescription ( ) : CUndoManager::undoText ( ));
		if ( mask & RedoDesc )
			emit sender-> redoDescriptionChanged ( stack ? stack-> redoDescription ( ) : CUndoManager::redoText ( ));
		if ( mask & Clean )
			emit sender-> cleanChanged ( stack ? stack-> isClean ( ) : false );
	}

private:
	enum {
		None     = 0x00,

		CanUndo  = 0x01,
		CanRedo  = 0x02,
		UndoDesc = 0x04,
		RedoDesc = 0x08,
		Clean    = 0x10,

		All      = 0xff
	};

	CUndoStack *m_stack;
	bool        m_explicit;
	bool        m_clean;
	bool        m_can_undo;
	bool        m_can_redo;
	QString     m_undo_desc;
	QString     m_redo_desc;
	CUndoCmd *  m_current;
};


//----------------------------------------------------------------------------------


CUndoCmd::CUndoCmd ( Type type, const QString &description, bool can_merge ) 
	: QObject ( 0, "undo_cmd" ), m_desc ( description ), m_type ( type ), m_can_merge ( can_merge )
{ }

CUndoCmd::CUndoCmd ( const QString &description, bool can_merge )
	: QObject ( 0, "undo_cmd" ), m_desc ( description ), m_type ( Command ), m_can_merge ( can_merge )
{ }

CUndoCmd::~CUndoCmd ( )
{ }

QString CUndoCmd::description ( ) const
{
	return m_desc;
}

void CUndoCmd::setDescription ( const QString &s )
{
	m_desc = s;
}

CUndoCmd::Type CUndoCmd::type ( ) const
{
	return m_type;
}

bool CUndoCmd::isCommand ( ) const
{
	return !( isMacroBegin ( ) || isMacroEnd ( ));
}

bool CUndoCmd::isMacroBegin ( ) const
{
	return ( type ( ) == MacroBegin );
}

bool CUndoCmd::isMacroEnd ( ) const
{
	return ( type ( ) == MacroEnd );
}

void CUndoCmd::initdo ( )
{
	redo ( );
}

void CUndoCmd::redo ( )
{ }

void CUndoCmd::undo ( )
{ }

bool CUndoCmd::canMerge ( ) const
{
	return m_can_merge;
}

void CUndoCmd::setCanMerge ( bool b )
{
	m_can_merge = b;
}

bool CUndoCmd::mergeMeWith ( CUndoCmd * /*other*/ )
{
	return false;
}

//----------------------------------------------------------------------------------

CUndoStack::CUndoStack ( QObject *parent )
	: QObject ( parent ), m_current ( m_stack )
{
	m_stack. setAutoDelete ( true );
	m_clean_valid = true;
	m_clean = 0;
	m_macro_level = 0;
	m_last_macro_begin = 0;

	CUndoManager::inst ( )-> associateView ( parent, this );
}

bool CUndoStack::canRedo ( ) const
{
	return !m_current. atLast ( ) && !m_macro_level;
}

bool CUndoStack::canUndo ( ) const
{
	return ( m_current != 0 ) && !m_macro_level;
}

QAction *CUndoStack::createRedoAction ( QObject *parent, const char *name ) const
{
	QAction *a = new CUndoAction ( CUndoAction::Redo, parent, name );
	a-> setEnabled ( canRedo ( ));

	connect ( this, SIGNAL( redoDescriptionChanged ( const QString & )), a, SLOT( updateDescriptions ( )));
	connect ( this, SIGNAL( canRedoChanged ( bool )), a, SLOT( setEnabled ( bool )));
	connect ( a, SIGNAL( activated ( int )), this, SLOT( redo ( int )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( redo ( )));

	return a;
}

QAction *CUndoStack::createUndoAction ( QObject *parent, const char *name ) const
{
	QAction *a = new CUndoAction ( CUndoAction::Undo, parent, name );
	a-> setEnabled ( canUndo ( ));

	connect ( this, SIGNAL( undoDescriptionChanged ( const QString & )), a, SLOT( updateDescriptions ( )));
	connect ( this, SIGNAL( canUndoChanged ( bool )), a, SLOT( setEnabled ( bool )));
	connect ( a, SIGNAL( activated ( int )), this, SLOT( undo ( int )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( undo ( )));

	return a;
}


bool CUndoStack::isClean ( ) const
{
	return m_clean_valid && ( m_current == m_clean );
}

void CUndoStack::setClean ( )
{
	m_clean_valid = true;
	m_clean = *m_current;
	emit cleanChanged ( true );
}

void CUndoStack::push ( CUndoCmd *cmd )
{
	if ( !cmd )
		return;
	
	if ( cmd-> isCommand ( ))
		cmd-> initdo ( );

	CUndoEmitter e ( this );

	while ( !m_stack. isEmpty ( ) && !m_current. atLast ( )) {
		if ( m_stack. getLast ( ) == m_clean )
			m_clean_valid = false;
		m_stack. removeLast ( );
	}

	bool merged = false;

	if (( cmd-> isCommand ( )) && 
		( cmd-> canMerge ( )) && 
	    ( *m_current ) &&
		( *m_current )-> isCommand ( ) && 
	    ( *m_current )-> canMerge ( ) && 
	    ( cmd-> className ( ) == ( *m_current )-> className ( )) && 
	    ( !m_clean_valid || ( m_clean != m_current ))) {
		merged = ( *m_current )-> mergeMeWith ( cmd );
	}

	if ( merged ) {
		delete cmd;
		cmd = *m_current;
	}
	else {
		bool last_was_macro_begin = (( *m_current ) && ( *m_current )-> isMacroBegin ( ));

		m_stack. append ( cmd );
		m_current. toLast ( );

		if ( cmd-> isMacroBegin ( )) {
			m_last_macro_begin = cmd;
			m_macro_level++;
		}
		else if ( cmd-> isMacroEnd ( )) {
			if ( m_last_macro_begin ) {
				if ( !cmd-> description ( ). isNull ( ))
					m_last_macro_begin-> setDescription ( cmd-> description ( ));
				else
					cmd-> setDescription ( m_last_macro_begin-> description ( ));
				m_last_macro_begin = 0;
			}
			m_macro_level--;

			// yank out empty macros
			if ( last_was_macro_begin ) {
				m_current -= 2;
				m_stack. removeLast ( );
				m_stack. removeLast ( );
			}
		}
		
		if ( !m_macro_level )
			emit commandExecuted ( );
	}
}

void CUndoStack::setCurrent ( )
{
	CUndoManager::inst ( )-> setCurrentStack ( this );
}

QString CUndoStack::redoDescription ( ) const
{
	QPtrListIterator <CUndoCmd> it = m_current;
	if ( *it )
		++it;
	else
		it. toFirst ( );

	return ( *it ) ? ( *it )-> description ( ) : QString ( );
}

QString CUndoStack::undoDescription ( ) const
{
	QPtrListIterator <CUndoCmd> it = m_current;

	return ( *it ) ? ( *it )-> description ( ) : QString ( );
}

QStringList CUndoStack::redoList ( ) const
{
	QStringList sl;
	QPtrListIterator <CUndoCmd> it = m_current;
	if ( *it )
		++it;
	else
		it. toFirst ( );

	int macrolevel = 0;

	while ( *it ) {
		if (( *it )-> type ( ) == CUndoCmd::MacroBegin )
			macrolevel++;
		else if (( *it )-> type ( ) == CUndoCmd::MacroEnd )
			macrolevel--;

		if ( !macrolevel )
			sl << ( *it )-> description ( );
		++it;
	}
	return sl;
}

QStringList CUndoStack::undoList ( ) const
{
	QStringList sl;
	QPtrListIterator <CUndoCmd> it = m_current;

	int macrolevel = 0;

	while ( *it ) {
		if (( *it )-> type ( ) == CUndoCmd::MacroBegin )
			macrolevel--;
		else if (( *it )-> type ( ) == CUndoCmd::MacroEnd )
			macrolevel++;

		if ( !macrolevel )
			sl << ( *it )-> description ( );
		--it;
	}
	return sl;
}

void CUndoStack::clear ( )
{
	CUndoEmitter e ( this );

	m_stack. clear ( );

	m_clean_valid = true;
	m_clean = 0;
	m_macro_level = 0;
}

void CUndoStack::redo ( int count )
{
	CUndoEmitter e ( this );

	while ( !m_current. atLast ( ) && ( count > 0 )) {
		QPtrListIterator <CUndoCmd> it = m_current;
		if ( *it )
			++it;
		else
			it. toFirst ( );

		switch (( *it )-> type ( )) {
			case CUndoCmd::MacroBegin:
				m_macro_level++;
				break;

			case CUndoCmd::MacroEnd:
				m_macro_level--;
				if ( m_macro_level == 0 ) {
					count--;
					emit commandExecuted ( );
				}
				break;

			case CUndoCmd::Command:
			default:
				( *it )-> redo ( );
				if ( m_macro_level == 0 ) {
					count--;
					emit commandExecuted ( );
				}
				break;
		}
		if ( *m_current )
			++m_current;
		else
			m_current. toFirst ( );
	}
}

void CUndoStack::undo ( int count )
{
	CUndoEmitter e ( this );

	while (( m_current != 0 ) && ( count > 0 )) {
		switch (( *m_current )-> type ( )) {
			case CUndoCmd::MacroEnd:
				m_macro_level++;
				break;

			case CUndoCmd::MacroBegin:
				m_macro_level--;
				if ( m_macro_level == 0 ) {
					count--;
					emit commandExecuted ( );
				}
				break;

			case CUndoCmd::Command:
			default:
				( *m_current )-> undo ( );
				if ( m_macro_level == 0 ) {
					count--;
					emit commandExecuted ( );
				}
				break;
		}
		--m_current;
	}
}

//----------------------------------------------------------------------------------


CUndoManager::CUndoManager ( )
	: QObject ( 0, "CUndoManager::inst()" )
{ 
	m_limit = 0;
	m_current = 0;

	qApp-> installEventFilter ( this );
}

CUndoManager *CUndoManager::s_inst = 0;

CUndoManager *CUndoManager::inst ( )
{
	if ( !s_inst )
		s_inst = new CUndoManager ( );
	return s_inst;
}

void CUndoManager::disassociateView ( QObject *view )
{
	if ( !view )
		return;

	CUndoStack *stack = m_stacks. find ( view );

	if ( view && stack ) {
		disconnect ( view, 0, this, 0 );
		disconnect ( stack, 0, this, 0 );

		m_stacks. remove ( view );
	}
}

void CUndoManager::associateView ( QObject *view, CUndoStack *stack )
{
	if ( !view || !stack )
		return;

	connect ( view, SIGNAL( destroyed ( QObject * )), this, SLOT( viewDestroyed ( QObject * )));
	connect ( stack, SIGNAL( destroyed ( QObject * )), this, SLOT( stackDestroyed ( QObject * )));

	m_stacks. replace ( view, stack );
}

bool CUndoManager::canRedo ( ) const
{
	return m_current ? m_current-> canRedo ( ) : false;
}

bool CUndoManager::canUndo ( ) const
{
	return m_current ? m_current-> canUndo ( ) : false;
}

void CUndoManager::redo ( int count )
{
	if ( m_current ) {
		CUndoEmitter e ( 0 );

		m_current-> redo ( count );
	}
}

void CUndoManager::undo ( int count )
{
	if ( m_current ) {
		CUndoEmitter e ( 0 );

		m_current-> undo ( count );
	}
}

QAction *CUndoManager::createRedoAction ( QObject *parent, const char *name ) const
{
	QAction *a = new CUndoAction ( CUndoAction::Redo, parent, name );
	a-> setEnabled ( canRedo ( ));

	connect ( this, SIGNAL( redoDescriptionChanged ( const QString & )), a, SLOT( updateDescriptions ( )));
	connect ( this, SIGNAL( canRedoChanged ( bool )), a, SLOT( setEnabled ( bool )));
	connect ( a, SIGNAL( activated ( int )), this, SLOT( redo ( int )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( redo ( )));

	return a;
}

QAction *CUndoManager::createUndoAction ( QObject *parent, const char *name ) const
{
	QAction *a = new CUndoAction ( CUndoAction::Undo, parent, name );
	a-> setEnabled ( canUndo ( ));

	connect ( this, SIGNAL( undoDescriptionChanged ( const QString & )), a, SLOT( updateDescriptions ( )));
	connect ( this, SIGNAL( canUndoChanged ( bool )), a, SLOT( setEnabled ( bool )));
	connect ( a, SIGNAL( activated ( int )), this, SLOT( undo ( int )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( undo ( )));

	return a;
}

CUndoStack *CUndoManager::currentStack ( ) const
{
	return m_current;
}

QString CUndoManager::redoDescription ( ) const
{
	return m_current ? m_current-> redoDescription ( ) : QString ( );
}

QString CUndoManager::redoText ( )
{
	return tr( "Redo" );
}

QStringList CUndoManager::redoList ( ) const
{
	return m_current ? m_current-> redoList ( ) : QStringList ( );
}

QString CUndoManager::undoDescription ( ) const
{
	return m_current ? m_current-> undoDescription ( ) : QString ( );
}

QString CUndoManager::undoText ( )
{
	return tr( "Undo" );
}

QStringList CUndoManager::undoList ( ) const
{
	return m_current ? m_current-> undoList ( ) : QStringList ( );
}

void CUndoManager::setCurrentStack ( CUndoStack *stack )
{
	activateStack ( stack );
}

void CUndoManager::setUndoLimit ( uint i )
{
	m_limit = i;
}

uint CUndoManager::undoLimit ( ) const
{
	return m_limit;
}

void CUndoManager::activateStack ( CUndoStack *stack )
{
	if ( stack == m_current )
		return;

	CUndoEmitter e;

	if ( m_current ) {
		disconnect ( m_current, SIGNAL( canRedoChanged ( bool )), this, SIGNAL( canRedoChanged ( bool )));
		disconnect ( m_current, SIGNAL( canUndoChanged ( bool )), this, SIGNAL( canUndoChanged ( bool )));
		disconnect ( m_current, SIGNAL( cleanChanged ( bool )), this, SIGNAL( cleanChanged ( bool )));
		disconnect ( m_current, SIGNAL( redoDescriptionChanged ( const QString & )), this, SIGNAL( redoDescriptionChanged ( const QString & )));
		disconnect ( m_current, SIGNAL( undoDescriptionChanged ( const QString & )), this, SIGNAL( undoDescriptionChanged ( const QString & )));

		m_current = 0;
	}

	if ( stack ) {
		m_current = stack;

		connect ( m_current, SIGNAL( canRedoChanged ( bool )), this, SIGNAL( canRedoChanged ( bool )));
		connect ( m_current, SIGNAL( canUndoChanged ( bool )), this, SIGNAL( canUndoChanged ( bool )));
		connect ( m_current, SIGNAL( cleanChanged ( bool )), this, SIGNAL( cleanChanged ( bool )));
		connect ( m_current, SIGNAL( redoDescriptionChanged ( const QString & )), this, SIGNAL( redoDescriptionChanged ( const QString & )));
		connect ( m_current, SIGNAL( undoDescriptionChanged ( const QString & )), this, SIGNAL( undoDescriptionChanged ( const QString & )));
	}
}


CUndoStack *CUndoManager::findStackForObject ( QObject *o )
{
	CUndoStack *stack = 0;

	while ( o ) {
		stack = m_stacks. find ( o );
		
		if ( !stack )
			o = o-> parent ( );
		else
			break;
	}

	return stack;
}

bool CUndoManager::eventFilter ( QObject *o, QEvent *e )
{
	if ( e-> type ( ) == QEvent::FocusIn ) {
		CUndoStack *stack = findStackForObject ( o );
		if ( stack )
			activateStack ( stack );
	}
	/*
	else if (( e-> type ( ) == QEvent::FocusOut ) && m_current ) {
		CUndoStack *stack = findStackForObject ( o );

		if ( stack == m_current )
			activateStack ( 0 );
	}
*/
	return QObject::eventFilter ( o, e );
}

void CUndoManager::viewDestroyed ( QObject *view )
{
	CUndoStack *stack = m_stacks. find ( view );

	if ( stack ) {
		CUndoEmitter e;

		m_stacks. remove ( view );
		disconnect ( stack, 0, this, 0 );
		if ( stack == m_current )
			m_current = 0;
	}
}

void CUndoManager::stackDestroyed ( QObject *stack )
{
	QObject *view = 0;

	for ( QPtrDictIterator <CUndoStack> it ( m_stacks ); it. current ( ); ++it ) {
		if ( it. current ( ) == stack ) {
			view = static_cast <QObject *> ( it. currentKey ( ));
			break;
		}
	}

	if ( view ) {
		if ( stack == m_current )
			m_current = 0;

		m_stacks. remove ( view );
		disconnect ( view, 0, this, 0 );
	}
}

// --------------------------------------------------------------------------

namespace {

class MyListBox : public QListBox {
public:
	MyListBox ( QWidget *parent ) : QListBox ( parent ), m_highlight ( -1 ){ }

	virtual QSize sizeHint ( ) const
	{
		(void) QListBox::sizeHint ( );

	    int fw = 2 * style ( ). pixelMetric ( QStyle::PM_DefaultFrameWidth );

		QSize s1 ( contentsWidth ( ), contentsHeight ( ));
		QSize s2 = s1. boundedTo ( QSize ( 300, 300 ));

		if (( s2. height ( ) < s1. height ( )) && ( s1. width ( ) <= s2. width ( )))
			s2. setWidth ( s1. width ( ) + style ( ). pixelMetric ( QStyle::PM_ScrollBarExtent ));

		return s2. expandedTo ( QSize ( 60, 60 )) + QSize ( fw, fw );
	}

public:
	int m_highlight;
};


class MyListBoxItem : public QListBoxText {
public:
	MyListBoxItem ( QListBox *parent, const QString & text = QString::null )
		: QListBoxText ( parent, text )
	{
		setCustomHighlighting ( true ); 
	}

protected:
	virtual void paint ( QPainter *p )
	{
		MyListBox *mlb = static_cast <MyListBox *> ( listBox ( ));
		int i = mlb-> index ( this );

		if ( i <= mlb-> m_highlight ) {
			p-> fillRect ( 0, 0, mlb-> contentsWidth ( ), height ( mlb ), mlb-> colorGroup ( ). brush ( QColorGroup::Highlight ));
		    p-> setPen ( mlb-> colorGroup ( ). highlightedText ( ));
		}

		QListBoxText::paint ( p );
	}
};


class MyLabel : public QLabel {
public:
	MyLabel ( QWidget *parent, const char **strings ) : QLabel ( parent ), m_strings ( strings ) { }

	virtual QSize sizeHint ( ) const
	{
		QSize s = QLabel::sizeHint ( );

		int fw = 2 * style ( ). pixelMetric ( QStyle::PM_DefaultFrameWidth );
		int w = s. width ( ) - 2 * fw;

		const QFontMetrics &fm = fontMetrics ( );

		for ( int i = 0; i < 4; i++ ) {
			QString s = CUndoManager::tr( m_strings [i] );
			if (!( i & 1 ))
				s = s. arg ( 1000 );
			int w2 = fm. width ( s );
			w = QMAX( w, w2 );
		}
		s. setWidth ( w + 2 * fw + 8 );
		return s;
	}
private:
	const char **m_strings;
};

}


const char *CUndoAction::s_strings [] = {
	QT_TRANSLATE_NOOP( "CUndoManager", "Undo %1 Actions" ),
	QT_TRANSLATE_NOOP( "CUndoManager", "Undo Action" ),
	QT_TRANSLATE_NOOP( "CUndoManager", "Redo %1 Actions" ),
	QT_TRANSLATE_NOOP( "CUndoManager", "Redo Action" )
};

CUndoAction::CUndoAction ( Type t, QObject *parent, const char *name )
	: QAction ( parent, name ) 
{ 
	m_type = t;
	m_menu = 0;
	m_list = 0;
	m_label = 0;

	languageChange ( );
}

void CUndoAction::setDescription ( const QString &desc )
{
	m_desc = desc;
	QString str = ( m_type == Undo ) ? CUndoManager::undoText ( ) : CUndoManager::redoText ( );
	if ( !desc. isEmpty ( ))
		str = str +  "  (" + desc + ")";
	
	setText ( str ); 
}

bool CUndoAction::event ( QEvent *e )
{
	if ( e-> type ( ) == QEvent::LanguageChange )
		languageChange ( );

	return QAction::event ( e );
}

void CUndoAction::languageChange ( )
{
	setDescription ( m_desc );

	if ( m_menu )
		m_menu-> resize ( m_menu-> sizeHint ( ));
}


void CUndoAction::addedTo ( QWidget *w, QWidget *cont )
{
	if ( cont-> inherits ( "QToolBar" ) && w-> inherits ( "QToolButton" )) {
		QToolButton *tb = static_cast <QToolButton *> ( w );

		m_menu = new QPopupMenu ( tb );
		tb-> setPopup ( m_menu );
		tb-> setPopupDelay ( 0 );

		m_list = new MyListBox ( m_menu );
		m_list-> setFrameStyle ( QFrame::NoFrame );
		m_list-> setSelectionMode ( QListBox::Multi );
		m_list-> setHScrollBarMode ( QScrollView::AlwaysOff );
		m_list-> setMouseTracking ( true );
		m_menu-> insertItem ( m_list );

		m_label = new MyLabel ( m_menu, s_strings );
		m_label-> setAlignment ( Qt::AlignCenter );
		m_label-> installEventFilter ( this );
		m_label-> setPalette ( QApplication::palette ( m_menu ));
		m_label-> setBackgroundMode ( m_menu-> backgroundMode ( ));
		m_menu-> insertItem ( m_label );

		connect ( m_list, SIGNAL( onItem ( QListBoxItem * )),         this, SLOT( setCurrentItemSlot ( QListBoxItem * )));
		connect ( m_list, SIGNAL( currentChanged ( QListBoxItem * )), this, SLOT( selectRange ( QListBoxItem * )));
		connect ( m_list, SIGNAL( clicked ( QListBoxItem * )),        this, SLOT( itemSelected ( QListBoxItem * )));
		connect ( m_list, SIGNAL( returnPressed ( QListBoxItem * )),  this, SLOT( itemSelected ( QListBoxItem * )));

		connect ( m_menu, SIGNAL( aboutToShow ( )), this, SLOT( fixMenu ( )));
	}
	QAction::addedTo ( w, cont );
}

bool CUndoAction::eventFilter ( QObject *o, QEvent *e )
{
	if (( o == m_label ) && ( e-> type ( ) == QEvent::MouseButtonPress ) && ( static_cast <QMouseEvent *> ( e )-> button ( ) == Qt::LeftButton ))
		m_menu-> close ( );

	return QAction::eventFilter ( o, e );
}

void CUndoAction::setCurrentItemSlot ( QListBoxItem *item )
{
	m_list-> setCurrentItem ( item );
}

void CUndoAction::fixMenu ( )
{
	m_list-> setCurrentItem ( m_list-> firstItem ( ));
	selectRange ( m_list-> firstItem ( ));
}

void CUndoAction::selectRange ( QListBoxItem *item )
{
	if ( item ) {
		int hl = m_list-> index ( item );

		static_cast <MyListBox *> ( m_list )-> m_highlight = hl;

		for ( int i = 0; i < int( m_list-> count ( )); i++ )
			m_list-> setSelected ( i, i <= hl );

		QString s = CUndoManager::tr( s_strings [(( m_type == Undo ) ? 0 : 2 ) + (( hl > 0 ) ? 0 : 1 )] );
		if ( hl > 0 )
			s = s. arg( hl+1 );
		m_label-> setText ( s );
	}
}


void CUndoAction::itemSelected ( QListBoxItem *item )
{
	if ( item ) {
		m_menu-> close ( );
		emit activated ( m_list-> index ( item ) + 1 );
	}
}


void CUndoAction::updateDescriptions ( )
{
	const QObject *s = sender ( );

	QStringList sl;

	if ( s ) {
		CUndoManager *manager = ::qt_cast<CUndoManager *> ( s );
		CUndoStack *stack = ::qt_cast<CUndoStack *> ( s );

		if ( manager )
			sl = (( m_type == Undo ) ? manager-> undoList ( ) : manager-> redoList ( ));
		else if ( stack )
			sl = (( m_type == Undo ) ? stack-> undoList ( ) : stack-> redoList ( ));
	}

	if ( m_list ) {
		m_list-> clear ( );
		for ( QStringList::const_iterator it = sl. begin ( ); it != sl. end ( ); ++it )
			(void) new MyListBoxItem ( m_list, *it );
	}

	setDescription ( sl. isEmpty ( ) ? QString ( ) : sl. front ( ));
}

