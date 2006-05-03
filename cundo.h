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
#ifndef __CUNDO_H__
#define __CUNDO_H__

#include <qobject.h>
#include <qptrlist.h>
#include <qptrdict.h>
#include <qaction.h>

class QPopupMenu;
class CUndoManager;

class CUndoCmd : public QObject {
public:
	enum Type { 
		Command, 
		MacroBegin, 
		MacroEnd 
	};

	CUndoCmd ( Type type, const QString &description = QString( ), bool canMerge = false );
	CUndoCmd ( const QString &description = QString( ), bool canMerge = true );
	virtual ~CUndoCmd ( );

	QString description ( ) const;
	void setDescription ( const QString &s );

	Type type ( ) const;
	bool isCommand ( ) const;
	bool isMacroBegin ( ) const;
	bool isMacroEnd ( ) const;

	virtual void initdo ( );
	virtual void redo ( );
	virtual void undo ( );

	bool canMerge ( ) const;
	void setCanMerge ( bool b );

protected:
	friend class CUndoStack;

	virtual bool mergeMeWith ( CUndoCmd *other );

private:
	QString m_desc;
	Type    m_type;
	bool    m_can_merge;
};


class CUndoStack : public QObject {
	Q_OBJECT

public:
	CUndoStack ( QObject *parent = 0 );

	bool canRedo ( ) const;
	bool canUndo ( ) const;
	QAction *createRedoAction ( QObject *parent, const char *name ) const;
	QAction *createUndoAction ( QObject *parent, const char *name ) const;
	bool isClean ( ) const;
	void push ( CUndoCmd *command );
	QString redoDescription ( ) const;
	QStringList redoList ( ) const;
	void setCurrent ( );
	QString undoDescription ( ) const;
	QStringList undoList ( ) const;

public slots:
	virtual void clear ( );
	void redo ( int count = 1 );
	void setClean ( );
	void undo ( int count = 1 );

signals:
	void canRedoChanged ( bool enabled );
	void canUndoChanged ( bool enabled );
	void cleanChanged ( bool clean );
	void commandExecuted ( );
	void redoDescriptionChanged ( const QString &desc );
	void undoDescriptionChanged ( const QString &desc );

private:
	friend class CUndoManager; 
	friend class CUndoEmitter;

	void currentChanged ( bool force_all = false );

private:
	QPtrList <CUndoCmd> m_stack;
	QPtrListIterator <CUndoCmd> m_current;
	CUndoCmd *m_clean;
	bool m_clean_valid;
	int m_macro_level;
	CUndoCmd *m_last_macro_begin;
};


class CUndoManager : public QObject {
	Q_OBJECT

public:
	static CUndoManager *inst ( );

	bool canRedo ( ) const;
	bool canUndo ( ) const;

	QAction *createRedoAction ( QObject *parent, const char *name = 0 ) const;
	QAction *createUndoAction ( QObject *parent, const char *name = 0 ) const;

	CUndoStack *currentStack ( ) const;
	void setCurrentStack ( CUndoStack *stack );

	QString undoDescription ( ) const;
	QString redoDescription ( ) const;
	QStringList undoList ( ) const;
	QStringList redoList ( ) const;

	uint undoLimit ( ) const;
	void setUndoLimit ( uint i );

	void associateView ( QObject *view, CUndoStack *stack ); 
	void disassociateView ( QObject *view );

public slots:
	void redo ( int count = 1 );
	void undo ( int count = 1 );

signals:
	void canRedoChanged ( bool enabled );
	void canUndoChanged ( bool enabled );
	void cleanChanged ( bool clean );
	void redoDescriptionChanged ( const QString &desc );
	void undoDescriptionChanged ( const QString &desc );

protected:
	virtual bool eventFilter ( QObject *o, QEvent *e );

private slots:
	void viewDestroyed ( QObject *view );
	void stackDestroyed ( QObject *stack );

private:
	CUndoManager ( );
	void activateStack ( CUndoStack *stack );
	CUndoStack *findStackForObject ( QObject *o );
	static QString undoText ( );
	static QString redoText ( );

	friend class CUndoStack;
	friend class CUndoEmitter;
	friend class CUndoAction;

private:
	CUndoStack *          m_current;
	QPtrDict <CUndoStack> m_stacks;
	uint                  m_limit;

	static CUndoManager * s_inst;
};


#endif
