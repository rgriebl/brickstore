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
#ifndef __BL_DATABASE_H__
#define __BL_DATABASE_H__

#include <qdatetime.h>
#include <qstring.h>
#include <qcstring.h>
#include <qptrvector.h>
#include <qptrlist.h>
#include <qdict.h>
#include <qintdict.h>
#include <qasciicache.h>
#include <qcolor.h>
#include <qobject.h>
#include <qimage.h>
#include <qdragobject.h>
#include <qdom.h>
#include <qlocale.h>
#include <qvaluevector.h>
#include <qmap.h>
#include <qpair.h>

#include "cref.h"
#include "cmoney.h"
#include "ctransfer.h"

class QIODevice;
template <typename T> class QDict;

class BrickLink : public QObject {
	Q_OBJECT
public:
	class Picture;
	class Category;
	class Color;
	class TextImport;

	class ItemType {
	public:
		char id ( ) const                 { return m_id; }
		const char *name ( ) const        { return m_name; }

		const QPtrList<BrickLink::Category> &categories ( ) const { return m_categories; }
		bool hasInventories ( ) const     { return m_has_inventories; }
		bool hasColors ( ) const          { return m_has_colors; }
		bool hasYearReleased ( ) const    { return m_has_year; }
		bool hasWeight ( ) const          { return m_has_weight; }
		char pictureId ( ) const          { return m_picture_id; }
		QSize imageSize ( ) const;

		~ItemType ( );

	private:
		char     m_id;
		char     m_picture_id;

		bool     m_has_inventories : 1;
		bool     m_has_colors      : 1;
		bool     m_has_weight      : 1;
		bool     m_has_year        : 1;

		char  *  m_name;

		QPtrList <Category> m_categories;

	private:
		ItemType ( );

		friend class BrickLink;
		friend class BrickLink::TextImport;
		friend QDataStream &operator << ( QDataStream &ds, const BrickLink::ItemType *itt );
		friend QDataStream &operator >> ( QDataStream &ds, BrickLink::ItemType *itt );
	};


	class Category {
	public:
		uint id ( ) const           { return m_id; }
		const char *name ( ) const  { return m_name; }

		~Category ( );

	private:
		uint     m_id;
		char *   m_name;

	private:
		Category ( );

		friend class BrickLink;
		friend class BrickLink::TextImport;
		friend QDataStream &operator << ( QDataStream &ds, const BrickLink::Category *cat );
		friend QDataStream &operator >> ( QDataStream &ds, BrickLink::Category *cat );
	};

	class Color {
	public:
		uint id ( ) const           { return m_id; }
		const char *name ( ) const  { return m_name; }
		QColor color ( ) const      { return m_color; }
		
		const char *peeronName ( ) const { return m_peeron_name; }
		int ldrawId ( ) const            { return m_ldraw_id; }

		bool isTransparent ( ) const { return m_is_trans; }
		bool isGlitter ( ) const     { return m_is_glitter; }
		bool isSpeckle ( ) const     { return m_is_speckle; }
		bool isMetallic ( ) const    { return m_is_metallic; }
		bool isChrome ( ) const      { return m_is_chrome; }

		~Color ( );

	private:
		uint   m_id;
		char * m_name;
		char * m_peeron_name;
		int    m_ldraw_id;
		QColor m_color;
		
		bool   m_is_trans    : 1;
		bool   m_is_glitter  : 1;
		bool   m_is_speckle  : 1;
		bool   m_is_metallic : 1;
		bool   m_is_chrome   : 1;

	private:
		Color ( );

		friend class BrickLink;
		friend class BrickLink::TextImport;
		friend QDataStream &operator << ( QDataStream &ds, const BrickLink::Color *col );
		friend QDataStream &operator >> ( QDataStream &ds, BrickLink::Color *col );
	};

	class Item {
	public:
		const char *id ( ) const                 { return m_id; }
		const char *name ( ) const               { return m_name; }
		const ItemType *itemType ( ) const       { return m_item_type; }
		const Category *category ( ) const       { return m_categories [0]; }
		const Category **allCategories ( ) const { return m_categories; }
		bool hasCategory ( const Category *cat ) const;
		QDateTime inventoryUpdated ( ) const     { return m_inv_updated; }
		const Color *defaultColor ( ) const      { return m_color; }
		double weight ( ) const                  { return m_weight; }
		int yearReleased ( ) const               { return m_year ? m_year + 1900 : 0; }

		~Item ( );

		typedef QValueVector <QPair <int, const Item *> >   AppearsInMapVector ;
		typedef QMap <const Color *, AppearsInMapVector>    AppearsInMap;

		AppearsInMap appearsIn ( const Color *color = 0 ) const;

	private:
		char *            m_id;
		char *            m_name;
		const ItemType *  m_item_type;
		const Category ** m_categories;
		const Color *     m_color;
		QDateTime         m_inv_updated;
		float             m_weight;
		Q_UINT32          m_index : 24;
		Q_UINT32          m_year  : 8;

		mutable Q_UINT32 *m_appears_in;

	private:
		Item ( );

		void setAppearsIn ( const AppearsInMap &map ) const;

		struct appears_in_record {
			Q_UINT32  m12  : 12;
			Q_UINT32  m20  : 20;
		};

		static Item *parse ( const BrickLink *bl, uint count, const char **strs, void *itemtype );

		static int compare ( const BrickLink::Item **a, const BrickLink::Item **b );

		friend class BrickLink;
		friend class BrickLink::TextImport;
		friend QDataStream &operator << ( QDataStream &ds, const BrickLink::Item *item );
		friend QDataStream &operator >> ( QDataStream &ds, BrickLink::Item *item );
	};


	enum Condition { New, Used, ConditionCount };

	enum UpdateStatus { Ok, Updating, UpdateFailed };

	class Picture : public CRef {
	public:
		const Item *item ( ) const          { return m_item; }
		const Color *color ( ) const        { return m_color; }

		void update ( bool high_priority = false );
		QDateTime lastUpdate ( ) const      { return m_fetched; }

		bool valid ( ) const                { return m_valid; }
		int updateStatus ( ) const          { return m_update_status; }

		const QPixmap pixmap ( ) const;

		virtual ~Picture ( );

		const QImage image ( ) const        { return m_image; }
		const char *key ( ) const           { return m_key; }

	private:
		const Item *  m_item;
		const Color * m_color;

		QDateTime     m_fetched;

		bool          m_valid         : 1;
		int           m_update_status : 7;

		char *        m_key;
		QImage        m_image;

	private:
		Picture ( const Item *item, const Color *color, const char *key );

		void load_from_disk ( );
		void save_to_disk ( );

		void parse ( const char *data, uint size );

		friend class BrickLink;
	};

	class InvItem {
	public:
		InvItem ( const Color *color, const Item *item );
		~InvItem ( );

		const Item *item ( ) const          { return m_item; }
		void setItem ( const Item *i )      { /*can be 0*/ m_item = i; }
		const Category *category ( ) const  { return m_item ? m_item-> category ( ) : 0; }
		const ItemType *itemType ( ) const  { return m_item ? m_item-> itemType ( ) : 0; }
		const Color *color ( ) const        { return m_color; }
		void setColor ( const Color *c )    { m_color = c; }

		enum Status    { Include, Exclude, Extra, Unknown };

		Status status ( ) const             { return m_status; }
		void setStatus ( Status s )         { m_status = s; }
		Condition condition ( ) const       { return m_condition; }
		void setCondition ( Condition c )   { m_condition = c; }
		QString comments ( ) const          { return m_comments; }
		void setComments ( const QString &n ) { m_comments = n; }
		QString remarks ( ) const           { return m_remarks; }
		void setRemarks ( const QString &r ){ m_remarks = r; }

		QCString customPictureUrl ( ) const { return m_custom_picture_url; }
		void setCustomPictureUrl ( const char *url ) { m_custom_picture_url = url; }

		int quantity ( ) const              { return m_quantity; }
		void setQuantity ( int q )          { m_quantity = q; }
		int origQuantity ( ) const          { return m_orig_quantity; }
		void setOrigQuantity ( int q )      { m_orig_quantity = q; }
		int bulkQuantity ( ) const          { return m_bulk_quantity; }
		void setBulkQuantity ( int q )      { m_bulk_quantity = QMAX( 1, q ); }
		int tierQuantity ( uint i ) const   { return m_tier_quantity [i < 3 ? i : 0]; }
		void setTierQuantity ( uint i, int q ) { m_tier_quantity [i < 3 ? i : 0] = q; }
		money_t price ( ) const             { return m_price; }
		void setPrice ( money_t p )         { m_price = p; }
		money_t origPrice ( ) const         { return m_orig_price; }
		void setOrigPrice ( money_t p )     { m_orig_price = p; }
		money_t tierPrice ( uint i ) const  { return m_tier_price [i < 3 ? i : 0]; }
		bool setTierPrice ( uint i, money_t p ) { if ( p < 0 ) return false; m_tier_price [i < 3 ? i : 0] = p; return true; }
		int sale ( ) const                  { return m_sale; }
		void setSale ( int s )              { m_sale = QMAX( -99, QMIN( 100, s )); }
		money_t total ( ) const             { return m_price * m_quantity; }

		uint lotId ( ) const                { return m_lot_id; }
		void setLotId ( uint lid )          { m_lot_id = lid; }

		bool retain ( ) const               { return m_retain; }
		void setRetain ( bool r )           { m_retain = r; }
		bool stockroom ( ) const            { return m_stockroom; }
		void setStockroom ( bool s )        { m_stockroom = s; }

		double weight ( ) const             { return m_weight ? m_weight : m_item-> weight ( ) * m_quantity; }
		void setWeight ( double w )         { m_weight = (float) w; }

		QString reserved ( ) const            { return m_reserved; }
		void setReserved ( const QString &r ) { m_reserved = r; }

		Picture *customPicture ( ) const    { return m_custom_picture; }
		
		struct Incomplete {
			QString m_item_id;
			QString m_item_name;
			QString m_category_id;
			QString m_category_name;
			QString m_itemtype_id;
			QString m_itemtype_name;
			QString m_color_id;
			QString m_color_name;
		};
		
		const Incomplete *isIncomplete ( ) const  { return m_incomplete; }
		void setIncomplete ( Incomplete *inc )    { delete m_incomplete; m_incomplete = inc; }
		
		bool mergeFrom ( const InvItem *merge, bool prefer_from = false );

		typedef void *Diff; // opaque handle

		Diff *createDiff ( const InvItem *diffto ) const;
		bool applyDiff ( Diff *diff );

	private:
		const Item *     m_item;
		const Color *    m_color;

		Incomplete *     m_incomplete;

		Status           m_status    : 3;
		Condition        m_condition : 2;
		bool             m_retain    : 1;
		bool             m_stockroom : 1;

		QString          m_comments;
		QString          m_remarks;
		QString          m_reserved;

		QCString         m_custom_picture_url;
		Picture  *       m_custom_picture;

		int              m_quantity;
		int              m_bulk_quantity;
		int              m_tier_quantity [3];
		int              m_sale;

		money_t          m_price;
		money_t          m_tier_price [3];

		float            m_weight;
		uint             m_lot_id;

		money_t          m_orig_price;
		int              m_orig_quantity;

		friend QDataStream &operator << ( QDataStream &ds, const BrickLink::InvItem &ii );
		friend QDataStream &operator >> ( QDataStream &ds, BrickLink::InvItem &ii );

		friend class BrickLink;
	};

	class InvItemDrag : public QDragObject {
	public:
		InvItemDrag ( const QPtrList<BrickLink::InvItem> &items, QWidget *dragsource = 0, const char *name = 0 );
		InvItemDrag ( QWidget *dragSource = 0, const char *name = 0 );

		void setItems ( const QPtrList<BrickLink::InvItem> &items );
		
		virtual const char *format ( int i ) const;
		virtual QByteArray encodedData ( const char * ) const;

		static bool canDecode ( QMimeSource * );
		static bool decode( QMimeSource *, QPtrList <BrickLink::InvItem> &items );

	private:
		static const char *s_mimetype;

		QByteArray m_data;
		QCString   m_text;
	};

	class Inventory : public CRef {
	public:
		const Item *item ( ) const          { return m_item; }

		void update ( );
		QDateTime lastUpdate ( ) const      { return m_fetched; }

		bool valid ( ) const                { return m_valid; }
		int updateStatus ( ) const          { return m_update_status; }

		QPtrList<InvItem> &inventory ( )    { return m_list; }

		virtual bool isOrder ( ) const      { return false; }

		virtual ~Inventory ( );
		
	private:
		const Item *      m_item;
		QDateTime         m_fetched;

		bool              m_valid         : 1;
		int               m_update_status : 7;
		bool              m_from_peeron   : 1;

		QPtrList<InvItem> m_list;

	protected:
		Inventory ( );

	private:
		Inventory ( const Item *item, bool from_peeron = false );
		void load_from_disk ( );

		friend class BrickLink;
	};

	class Order : public Inventory {
	public:
		enum Type { Received, Placed };

		virtual bool isOrder ( ) const  { return true; }

		QString id ( ) const        { return m_id; }
		Type type ( ) const         { return m_type; }
		QDateTime date ( ) const    { return m_date; }
		QDateTime statusChange ( )  { return m_status_change; }
		QString buyer ( ) const     { return m_buyer; }
		money_t shipping ( ) const  { return m_shipping; }
		money_t insurance ( ) const { return m_insurance; }
		money_t delivery ( ) const  { return m_delivery; }
		money_t credit ( ) const    { return m_credit; }
		money_t grandTotal ( ) const{ return m_grand_total; }
		QString status ( ) const    { return m_status; }
		QString payment ( ) const   { return m_payment; }
		QString remarks ( ) const   { return m_remarks; }

		virtual ~Order ( );

	private:
		QString   m_id;
		Type      m_type;
		QDateTime m_date;
		QDateTime m_status_change;
		QString   m_buyer;
		money_t   m_shipping;
		money_t   m_insurance;
		money_t   m_delivery;
		money_t   m_credit;
		money_t   m_grand_total;
		QString   m_status;
		QString   m_payment;
		QString   m_remarks;

	private:
		Order ( const QString &id, Type type );

		friend class BrickLink;
	};

	class PriceGuide : public CRef {
	public:
		const Item *item ( ) const          { return m_item; }
		const Color *color ( ) const        { return m_color; }

		void update ( bool high_priority = false );
		QDateTime lastUpdate ( ) const      { return m_fetched; }

		enum Time      { AllTime, PastSix, Current, TimeCount };
		enum Price     { Lowest, Average, WAverage, Highest, PriceCount };

		bool valid ( ) const                { return m_valid; }
		int updateStatus ( ) const          { return m_update_status; }

		int quantity  ( Time t, Condition c ) const           { return m_quantities [t < TimeCount ? t : 0][c < ConditionCount ? c : 0]; }
		int lots      ( Time t, Condition c ) const           { return m_lots [t < TimeCount ? t : 0][c < ConditionCount ? c : 0]; }
		money_t price ( Time t, Condition c, Price p ) const  { return m_prices [t < TimeCount ? t : 0][c < ConditionCount ? c : 0][p < PriceCount ? p : 0]; }

		virtual ~PriceGuide ( );

	private:
		const Item *  m_item;
		const Color * m_color;

		QDateTime     m_fetched;

		bool          m_valid         : 1;
		int           m_update_status : 7;

		int           m_quantities [TimeCount][ConditionCount];
		int           m_lots       [TimeCount][ConditionCount];
		money_t       m_prices     [TimeCount][ConditionCount][PriceCount];

	private:
		PriceGuide ( const Item *item, const Color *color );

		void load_from_disk ( );
		void save_to_disk ( );

		bool parse ( const char *data, uint size );

		friend class BrickLink;
	};

	class TextImport {
	public:
		TextImport ( );

		bool import ( const QString &path );

		const QIntDict<Color>    &colors ( ) const      { return m_colors; }
		const QIntDict<Category> &categories ( ) const  { return m_categories; }
		const QIntDict<ItemType> &itemTypes ( ) const   { return m_item_types; }
		const QPtrVector<Item>   &items ( ) const       { return m_items; }
		
	private:
		template <typename T> T *parse ( uint count, const char **strs, T *gcc_dummy );
		Category *parse ( uint count, const char **strs, Category * );
		Color *parse ( uint count, const char **strs, Color * );
		ItemType *parse ( uint count, const char **strs, ItemType * );
		Item *parse ( uint count, const char **strs, Item * );

		template <typename C> bool readDB ( const QString &name, C &container );
		template <typename T> bool readDB_processLine ( QIntDict<T> &d, uint tokencount, const char **tokens );
		template <typename T> bool readDB_processLine ( QPtrVector<T> &v, uint tokencount, const char **tokens );

		struct btinvlist_dummy { };
		bool readDB_processLine ( btinvlist_dummy &, uint count, const char **strs );

		bool readColorGuide ( const QString &name );
		bool readPeeronColors ( const QString &name );

		const BrickLink::Category *findCategoryByName ( const char *name, int len = -1 );
		const BrickLink::Item *findItem ( char type, const char *id );


	private:
		QIntDict <Color>    m_colors;
		QIntDict <ItemType> m_item_types;
		QIntDict <Category> m_categories;
		QPtrVector <Item>   m_items;

		const ItemType *m_current_item_type;
	};


public:
	virtual ~BrickLink ( );
	static BrickLink *inst ( const QString &datadir, QString *errstring );
	static BrickLink *inst ( );

	const QLocale &cLocale ( ) const   { return m_c_locale; }

	enum UrlList {
		URL_InventoryRequest,
		URL_WantedListUpload,
		URL_InventoryUpload,
		URL_InventoryUpdate,
		URL_CatalogInfo,
		URL_PriceGuideInfo,
		URL_ColorChangeLog,
		URL_ItemChangeLog,
		URL_LotsForSale,
		URL_AppearsInSets,
		URL_PeeronInfo
	};

	QCString url ( UrlList u, const void *opt = 0, const void *opt2 = 0 );

	QString dataPath ( ) const;
	QString dataPath ( const ItemType * ) const;
	QString dataPath ( const Item * ) const;
	QString dataPath ( const Item *, const Color * ) const;

	const QIntDict<Color>    &colors ( ) const;
	const QIntDict<Category> &categories ( ) const;
	const QIntDict<ItemType> &itemTypes ( ) const;
	const QPtrVector<Item>   &items ( ) const;

	const QPixmap *noImage ( const QSize &s );

	QImage colorImage ( const BrickLink::Color *col, int w, int h ) const;

	const Color *color ( uint id ) const;
	const Color *colorFromPeeronName ( const char *peeron_name ) const;
	const Color *colorFromLDrawId ( int ldraw_id ) const;
	const Category *category ( uint id ) const;
	const Category *category ( const char *name, int len = -1 ) const;
	const ItemType *itemType ( char id ) const;
	const Item *item ( char tid, const char *id ) const;

	PriceGuide *priceGuide ( const InvItem *invitem, bool high_priority = false );
	PriceGuide *priceGuide ( const Item *item, const Color *color, bool high_priority = false );

	QPtrList<InvItem> *load ( QIODevice *f, uint *invalid_items = 0 );
	QPtrList<InvItem> *loadXML ( QIODevice *f, uint *invalid_items = 0 );
	QPtrList<InvItem> *loadBTI ( QIODevice *f, uint *invalid_items = 0 );

	Inventory *inventory ( const Item *item );
	Inventory *inventoryFromPeeron ( const Item *item );
	Order *order ( const QString &orderid, Order::Type ordertype );
	Inventory *storeInventory ( );

	Picture *picture ( const InvItem *invitem, bool high_priority = false );
	Picture *picture ( const Item *item, const Color *color, bool high_priority = false );
	Picture *largePicture ( const Item *item, bool high_priority = false );

	enum ItemListXMLHint { 
		XMLHint_MassUpload, 
		XMLHint_MassUpdate, 
		XMLHint_Inventory, 
		XMLHint_Order, 
		XMLHint_WantedList, 
		XMLHint_BrikTrak,
		XMLHint_BrickStore
	};

	QPtrList<InvItem> *parseItemListXML ( QDomElement root, ItemListXMLHint hint, uint *invalid_items = 0 );
	QDomElement createItemListXML ( QDomDocument doc, ItemListXMLHint hint, const QPtrList<InvItem> *items, QMap <QString, QString> *extra = 0 );

	bool parseLDrawModel ( QFile &file, QPtrList <InvItem> &items, uint *invalid_items = 0 );

	bool onlineStatus ( ) const;

	void setDatabase_AppearsIn ( const QMap<const Item *, Item::AppearsInMap> &map );
	void setDatabase_Basics ( const QIntDict<Color> &colors, 
								const QIntDict<Category> &categories,
								const QIntDict<ItemType> &item_types,
								const QPtrVector<Item> &items );

public slots:
	bool readDatabase ( const QString &fname = QString ( ));
	bool writeDatabase ( const QString &fname = QString ( ));

	void updatePriceGuide ( PriceGuide *pg, bool high_priority = false );
	void updateInventory ( Inventory *inv );
	void updatePicture ( Picture *pic, bool high_priority = false );

	void setOnlineStatus ( bool on );
	void setUpdateIntervals ( int pic, int pg );
	void setHttpProxy ( bool enable, const QString &name, int port );

	void cancelInventoryTransfers ( );
	void cancelPictureTransfers ( );
	void cancelPriceGuideTransfers ( );

signals:
	void priceGuideUpdated ( BrickLink::PriceGuide *pg );
	void inventoryUpdated ( BrickLink::Inventory *inv );
	void pictureUpdated ( BrickLink::Picture *inv );

	void priceGuideProgress ( int, int );
	void inventoryProgress ( int, int );
	void pictureProgress ( int, int );

private:
	BrickLink ( const QString &datadir );
	static BrickLink *s_inst;

	bool updateNeeded ( const QDateTime &last, int iv );

	bool parseLDrawModelInternal ( QFile &file, const QString &model_name, QPtrList <InvItem> &items, uint *invalid_items, QDict <InvItem> &mergehash );

	void pictureIdleLoader2 ( );

private slots:
	void pictureIdleLoader ( );

	void pictureJobFinished ( CTransfer::Job * );
	void inventoryJobFinished ( CTransfer::Job * );
	void priceGuideJobFinished ( CTransfer::Job * );

private:
	QString  m_datadir;
	bool     m_online;
	QLocale  m_c_locale;

	QDict <QPixmap>  m_noimages;

	struct dummy1 {
		QIntDict<Color>           colors;      // id -> Color *
		QIntDict<Category>        categories;  // id -> Category *
		QIntDict<ItemType>        item_types;  // id -> ItemType *
		QPtrVector<Item>          items;       // sorted array of Item *
	} m_databases;

	struct dummy2 {
		CTransfer *               transfer;
		int                       update_iv;

		CAsciiRefCache<PriceGuide, 503, 500> cache;
	} m_price_guides;

	struct dummy3 {
		CTransfer *               transfer;
		int                       update_iv;

		QPtrList <Picture>        diskload;

		CAsciiRefCache<Picture, 503, 500>    cache;
	} m_pictures;

	struct dummy4 {
		CTransfer *               transfer;
		int                       update_iv;

		CAsciiRefCache<Inventory, 29, 30>  cache;
	} m_inventories;
};

QDataStream &operator << ( QDataStream &ds, const BrickLink::InvItem &ii );
QDataStream &operator >> ( QDataStream &ds, BrickLink::InvItem &ii );

QDataStream &operator << ( QDataStream &ds, const BrickLink::Item *item );
QDataStream &operator >> ( QDataStream &ds, BrickLink::Item *item );

QDataStream &operator << ( QDataStream &ds, const BrickLink::ItemType *itt );
QDataStream &operator >> ( QDataStream &ds, BrickLink::ItemType *itt );

QDataStream &operator << ( QDataStream &ds, const BrickLink::Category *cat );
QDataStream &operator >> ( QDataStream &ds, BrickLink::Category *cat );

QDataStream &operator << ( QDataStream &ds, const BrickLink::Color *col );
QDataStream &operator >> ( QDataStream &ds, BrickLink::Color *col );


//} // namespace BrickLink

#endif

