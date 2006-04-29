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
#ifndef __CDOCUMENT_H__
#define __CDOCUMENT_H__

#include <qobject.h>
#include <qdom.h>

#include "bricklink.h"

class CUndoStack;
class CUndoCmd;
class CAddRemoveCmd;
class CChangeCmd;


class IDocumentView {
public:
	virtual ~IDocumentView() { }

	virtual QDomElement createGuiStateXML ( QDomDocument doc ) = 0;
	virtual bool parseGuiStateXML ( QDomElement root ) = 0;
};

class CDocument : public QObject {
	Q_OBJECT

public:
	enum Field {
		Status = 0,
		Picture,
		PartNo,
		Description,
		Condition,
		Color,
		Quantity,
		Price,
		Total,
		Bulk,
		Sale,
		Comments,
		Remarks,
		Category,
		ItemType,
		TierQ1,
		TierP1,
		TierQ2,
		TierP2,
		TierQ3,
		TierP3,
		LotId,
		Retain,
		Stockroom,
		Reserved,
		Weight,
		YearReleased,

		QuantityOrig,
		QuantityDiff,
		PriceOrig,
		PriceDiff,

		FieldCount,
	};

	class Item : public BrickLink::InvItem {
	public:
		Item ( );
		Item ( const BrickLink::InvItem & );
		Item ( const Item & );

		Item &operator = ( const Item & );
		bool operator == ( const Item & ) const;

		Q_UINT64 errors ( ) const          { return m_errors; }
		void setErrors ( Q_UINT64 errors ) { m_errors = errors; }
	private:
		Q_UINT64 m_errors;
	};

	typedef QValueList<Item *>      ItemList;
	typedef int                     Position;
	typedef QValueVector<Position>  PositionVector;

	class Statistics {
	public:
		uint lots ( ) const         { return m_lots; }
		uint items ( ) const        { return m_items; }
		money_t value ( ) const     { return m_val; }
		money_t minValue ( ) const  { return m_minval; }
		double weight ( ) const     { return m_weight; }
		uint errors ( ) const       { return m_errors; }

	private:
		friend class CDocument;

		Statistics ( const CDocument *doc, const ItemList &list );

		uint m_lots;
		uint m_items;
		money_t m_val;
		money_t m_minval;
		double m_weight;
		uint m_errors;
	};


public:
	CDocument ( );
	virtual ~CDocument ( );

	static const QValueList<CDocument *> &allDocuments ( );

	void addView ( QWidget *view, IDocumentView *docview = 0 );

	QString fileName ( ) const;
	QString title ( ) const;

	const BrickLink::Order *order ( ) const;

	bool isModified ( ) const;

	const ItemList &items ( ) const;
	const ItemList &selection ( ) const;

	bool clear ( );

	bool insertItems ( const ItemList &positions, const ItemList &list );
	bool removeItems ( const ItemList &positions );

	bool insertItem ( Item *position, Item *item );
	bool removeItem ( Item *position );

	bool changeItem ( Item *position, const Item &item );

	void resetDifferences ( const ItemList &items );

	Statistics statistics ( const ItemList &list ) const;

	Q_UINT64 errorMask ( ) const;
	void setErrorMask ( Q_UINT64 );

	static CDocument *fileNew ( );
	static CDocument *fileOpen ( );
	static CDocument *fileOpen ( const QString &name );
	static CDocument *fileImportBrickLinkInventory ( const BrickLink::Item *preselect = 0 );
	static CDocument *fileImportBrickLinkOrder ( );
	static CDocument *fileImportBrickLinkStore ( );
	static CDocument *fileImportBrickLinkXML ( );
	static CDocument *fileImportPeeronInventory ( ); 
	static CDocument *fileImportBrikTrakInventory ( const QString &fn = QString::null );
	static CDocument *fileImportLDrawModel ( );

public slots:
	void setFileName ( const QString &str );
	void setTitle ( const QString &str );

	void setSelection ( const ItemList & );

	void fileSave ( const ItemList &itemlist );
	void fileSaveAs ( const ItemList &itemlist );
	void fileExportBrickLinkXML ( const ItemList &itemlist );
	void fileExportBrickLinkXMLClipboard ( const ItemList &itemlist );
	void fileExportBrickLinkUpdateClipboard ( const ItemList &itemlist );
	void fileExportBrickLinkInvReqClipboard ( const ItemList &itemlist );
	void fileExportBrickLinkWantedListClipboard ( const ItemList &itemlist );
	void fileExportBrikTrakInventory ( const ItemList &itemlist );

public:
	CUndoCmd *macroBegin ( const QString &label = QString ( ));
	void      macroEnd ( CUndoCmd *, const QString &label = QString ( ));

signals:
	void itemsAdded ( const CDocument::ItemList & );
	void itemsAboutToBeRemoved ( const CDocument::ItemList & );
	void itemsRemoved ( const CDocument::ItemList & );
	void itemsChanged ( const CDocument::ItemList &, bool );

	void errorsChanged ( CDocument::Item * );
	void statisticsChanged ( );
	void fileNameChanged ( const QString & );
	void titleChanged ( const QString & );
	void modificationChanged ( bool );
	void selectionChanged ( const CDocument::ItemList & );

private slots:
	void clean2Modified ( bool );

private:
	static CDocument *fileLoadFrom ( const QString &s, const char *type, bool import_only = false );
	bool fileSaveTo ( const QString &s, const char *type, bool export_only, const ItemList &itemlist );
	void setBrickLinkItems ( const BrickLink::InvItemList &bllist, uint multiply = 1 );

	void insertItemsDirect ( ItemList &items, ItemList &positions );
	void removeItemsDirect ( ItemList &items, ItemList &positions );
	void changeItemDirect ( Item *position, Item &item );

	friend class CAddRemoveCmd;
	friend class CChangeCmd;

	void updateErrors ( Item * );

private:
	ItemList         m_items;
	ItemList         m_selection;

	Q_UINT64         m_error_mask;
	QString          m_filename;
	QString          m_title;

	CUndoStack *     m_undo;

	BrickLink::Order *m_order;

	QValueList<IDocumentView *> m_views;
	QDomElement  m_gui_state;

	static QValueList<CDocument *> s_documents;
};

#endif
