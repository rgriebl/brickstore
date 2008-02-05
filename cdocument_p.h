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
#ifndef __CDOCUMENT_P_H__
#define __CDOCUMENT_P_H__

#include "cundo.h"
#include "cdocument.h"


class CAddRemoveCmd : public CUndoCmd {
	Q_OBJECT

public:
	enum Type { Add, Remove };

	CAddRemoveCmd ( Type t, CDocument *doc, const CDocument::ItemList &positions, const CDocument::ItemList &items, bool can_merge = false );
	~CAddRemoveCmd ( );

	virtual void redo ( );
	virtual void undo ( );
	virtual bool mergeMeWith ( CUndoCmd *other );

	static QString genDesc ( bool is_add, uint count );

private:
	CDocument *         m_doc;
	CDocument::ItemList m_positions;
	CDocument::ItemList m_items;
	Type                m_type;
};

class CChangeCmd : public CUndoCmd {
	Q_OBJECT

public:
	CChangeCmd ( CDocument *doc, CDocument::Item *position, const CDocument::Item &item, bool can_merge = false );

	virtual void redo ( );
	virtual void undo ( );
	virtual bool mergeMeWith ( CUndoCmd *other );

private:
	CDocument *      m_doc;
	CDocument::Item *m_position;
	CDocument::Item  m_item;
};

#endif
