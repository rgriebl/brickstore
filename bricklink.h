/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __BRICKLINK_H__
#define __BRICKLINK_H__

#include <QtGlobal>
#include <QDateTime>
#include <QString>
#include <qcolor.h>
#include <qobject.h>
#include <qimage.h>
//#include <qdragobject.h>
#include <QtXml/QDomDocument>
#include <qlocale.h>
#include <QHash>
#include <QVector>
#include <QList>
#include <QMap>
#include <QPair>
#include <QUrl>
#include <QMimeData>
#include <QAbstractListModel>

#include <time.h>

#include "cref.h"
#include "cmoney.h"
#include "ctransfer.h"


class QIODevice;
class QFile;
class QAbstractItemModel;
//template <typename T> class QDict;


namespace BrickLink {
class Picture;
class Category;
class Color;
class TextImport;
class InvItem;
class Item;
class ItemType;


QDataStream &operator << (QDataStream &ds, const InvItem &ii);
QDataStream &operator >> (QDataStream &ds, InvItem &ii);

QDataStream &operator << (QDataStream &ds, const Item *item);
QDataStream &operator >> (QDataStream &ds, Item *item);

QDataStream &operator << (QDataStream &ds, const ItemType *itt);
QDataStream &operator >> (QDataStream &ds, ItemType *itt);

QDataStream &operator << (QDataStream &ds, const Category *cat);
QDataStream &operator >> (QDataStream &ds, Category *cat);

QDataStream &operator << (QDataStream &ds, const Color *col);
QDataStream &operator >> (QDataStream &ds, Color *col);

typedef QList<InvItem *> InvItemList;

enum Time      { AllTime, PastSix, Current, TimeCount };
enum Price     { Lowest, Average, WAverage, Highest, PriceCount };
enum Condition { New, Used, ConditionCount };

enum Status    { Include, Exclude, Extra, Unknown };

enum UpdateStatus { Ok, Updating, UpdateFailed };

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
    URL_PeeronInfo,
    URL_StoreItemDetail
};

enum ItemListXMLHint {
    XMLHint_MassUpload,
    XMLHint_MassUpdate,
    XMLHint_Inventory,
    XMLHint_Order,
    XMLHint_WantedList,
    XMLHint_BrikTrak,
    XMLHint_BrickStore
};

class ItemType {
public:
    char id() const                 { return m_id; }
    const char *name() const        { return m_name; }

    const Category **categories() const { return m_categories; }
    bool hasInventories() const     { return m_has_inventories; }
    bool hasColors() const          { return m_has_colors; }
    bool hasYearReleased() const    { return m_has_year; }
    bool hasWeight() const          { return m_has_weight; }
    char pictureId() const          { return m_picture_id; }
    QSize pictureSize() const;

    ~ItemType();

private:
    char     m_id;
    char     m_picture_id;

    bool     m_has_inventories : 1;
    bool     m_has_colors      : 1;
    bool     m_has_weight      : 1;
    bool     m_has_year        : 1;

    char  *  m_name;

    const Category **m_categories;

private:
    ItemType();

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator << (QDataStream &ds, const ItemType *itt);
    friend QDataStream &operator >> (QDataStream &ds, ItemType *itt);
};


class Category {
public:
    uint id() const           { return m_id; }
    const char *name() const  { return m_name; }

    ~Category();

private:
    uint     m_id;
    char *   m_name;

private:
    Category();

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator << (QDataStream &ds, const Category *cat);
    friend QDataStream &operator >> (QDataStream &ds, Category *cat);
};

class Color {
public:
    uint id() const           { return m_id; }
    const char *name() const  { return m_name; }
    QColor color() const      { return m_color; }

    const char *peeronName() const { return m_peeron_name; }
    int ldrawId() const            { return m_ldraw_id; }

    QString categoryName() const   { return m_category_name; }

    enum {
        Solid        = 0x00,
        Transparent  = 0x01,
        Glitter      = 0x02,
        Speckle      = 0x04,
        Metallic     = 0x08,
        Chrome       = 0x10,
        Pearl        = 0x20,
        Milky        = 0x40
    };

    bool isTransparent() const { return m_type & Transparent; }
    bool isGlitter() const     { return m_type & Glitter; }
    bool isSpeckle() const     { return m_type & Speckle; }
    bool isMetallic() const    { return m_type & Metallic; }
    bool isChrome() const      { return m_type & Chrome; }
    bool isPearl() const       { return m_type & Pearl; }
    bool isMilky() const       { return m_type & Milky; }
    ~Color();

private:
    uint    m_id;
    char *  m_name;
    char *  m_peeron_name;
    int     m_ldraw_id;
    QColor  m_color;
    QString m_category_name;
    uint    m_type;

private:
    Color();

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator << (QDataStream &ds, const Color *col);
    friend QDataStream &operator >> (QDataStream &ds, Color *col);
};

class Item {
public:
    const char *id() const                 { return m_id; }
    const char *name() const               { return m_name; }
    const ItemType *itemType() const       { return m_item_type; }
    const Category *category() const       { return m_categories [0]; }
    const Category **allCategories() const { return m_categories; }
    bool hasCategory(const Category *cat) const;
    bool hasInventory() const              { return (m_last_inv_update >= 0); }
    QDateTime inventoryUpdated() const     { QDateTime dt; if (m_last_inv_update >= 0) dt.setTime_t (m_last_inv_update); return dt; }
    const Color *defaultColor() const      { return m_color; }
    double weight() const                  { return m_weight; }
    int yearReleased() const               { return m_year ? m_year + 1900 : 0; }

    ~Item();

    typedef QPair <int, const Item *>              AppearsInItem;
    typedef QVector <AppearsInItem>                AppearsInColor;
    typedef QHash <const Color *, AppearsInColor>  AppearsIn;

    AppearsIn appearsIn(const Color *color = 0) const;
    InvItemList  consistsOf() const;

    uint index() const { return m_index; }   // only for internal use (picture/priceguide hashes)

private:
    char *            m_id;
    char *            m_name;
    const ItemType *  m_item_type;
    const Category ** m_categories;
    const Color *     m_color;
    time_t            m_last_inv_update;
    float             m_weight;
    quint32           m_index : 24;
    quint32           m_year  : 8;

    mutable quint32 * m_appears_in;
    mutable quint64 * m_consists_of;

private:
    Item();

    void setAppearsIn(const AppearsIn &hash) const;
    void setConsistsOf(const InvItemList &items) const;

    struct appears_in_record {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint32  m12  : 12;
        quint32  m20  : 20;
#else
        quint32  m20  : 20;
        quint32  m12  : 12;
#endif
    };

    struct consists_of_record {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint64  m_qty      : 12;
        quint64  m_index    : 20;
        quint64  m_color    : 12;
        quint64  m_extra    : 1;
        quint64  m_isalt    : 1;
        quint64  m_altid    : 6;
        quint64  m_reserved : 12;
#else
        quint64  m_reserved : 12;
        quint64  m_altid    : 6;
        quint64  m_isalt    : 1;
        quint64  m_extra    : 1;
        quint64  m_color    : 12;
        quint64  m_index    : 20;
        quint64  m_qty      : 12;
#endif
    };

    static Item *parse(const Core *bl, uint count, const char **strs, void *itemtype);

    static int compare(const Item **a, const Item **b);

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator << (QDataStream &ds, const Item *item);
    friend QDataStream &operator >> (QDataStream &ds, Item *item);
};


class Picture : public CRef {
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool high_priority = false);
    QDateTime lastUpdate() const      { return m_fetched; }

    bool valid() const                { return m_valid; }
    int updateStatus() const          { return m_update_status; }

    const QPixmap pixmap() const;

    virtual ~Picture();

    const QImage image() const        { return m_image; }
    const char *key() const           { return m_key; }

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid         : 1;
    int           m_update_status : 7;

    char *        m_key;
    QImage        m_image;

private:
    Picture(const Item *item, const Color *color, const char *key);

    void load_from_disk();
    void save_to_disk();

    void parse(const char *data, uint size);

    friend class Core;
};

class InvItem {
public:
    InvItem(const Color *color = 0, const Item *item = 0);
    InvItem(const InvItem &copy);
    ~InvItem();

    InvItem &operator = (const InvItem &copy);
    bool operator == (const InvItem &cmp) const;

    const Item *item() const           { return m_item; }
    void setItem(const Item *i)        { /*can be 0*/ m_item = i; }
    const Category *category() const   { return m_item ? m_item->category() : 0; }
    const ItemType *itemType() const   { return m_item ? m_item->itemType() : 0; }
    const Color *color() const         { return m_color; }
    void setColor(const Color *c)      { m_color = c; }

    Status status() const              { return m_status; }
    void setStatus(Status s)           { m_status = s; }
    Condition condition() const        { return m_condition; }
    void setCondition(Condition c)     { m_condition = c; }
    QString comments() const           { return m_comments; }
    void setComments(const QString &n) { m_comments = n; }
    QString remarks() const            { return m_remarks; }
    void setRemarks(const QString &r)  { m_remarks = r; }

    QString customPictureUrl() const   { return m_custom_picture_url; }
    void setCustomPictureUrl(const QString &url) { m_custom_picture_url = url; }

    int quantity() const               { return m_quantity; }
    void setQuantity(int q)            { m_quantity = q; }
    int origQuantity() const           { return m_orig_quantity; }
    void setOrigQuantity(int q)        { m_orig_quantity = q; }
    int bulkQuantity() const           { return m_bulk_quantity; }
    void setBulkQuantity(int q)        { m_bulk_quantity = qMax(1, q); }
    int tierQuantity(uint i) const     { return m_tier_quantity [i < 3 ? i : 0]; }
    void setTierQuantity(uint i, int q){ m_tier_quantity [i < 3 ? i : 0] = q; }
    money_t price() const              { return m_price; }
    void setPrice(money_t p)           { m_price = p; }
    money_t origPrice() const          { return m_orig_price; }
    void setOrigPrice(money_t p)       { m_orig_price = p; }
    money_t tierPrice(uint i) const    { return m_tier_price [i < 3 ? i : 0]; }
    bool setTierPrice(uint i, money_t p){ if (p < 0) return false; m_tier_price [i < 3 ? i : 0] = p; return true; }
    int sale() const                   { return m_sale; }
    void setSale(int s)                { m_sale = qMax(-99, qMin(100, s)); }
    money_t total() const              { return m_price * m_quantity; }

    uint lotId() const                 { return m_lot_id; }
    void setLotId(uint lid)            { m_lot_id = lid; }

    bool retain() const                { return m_retain; }
    void setRetain(bool r)             { m_retain = r; }
    bool stockroom() const             { return m_stockroom; }
    void setStockroom(bool s)          { m_stockroom = s; }

    double weight() const              { return m_weight ? m_weight : m_item->weight() * m_quantity; }
    void setWeight(double w)           { m_weight = (float) w; }

    QString reserved() const           { return m_reserved; }
    void setReserved(const QString &r) { m_reserved = r; }

    Picture *customPicture() const     { return m_custom_picture; }

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

    const Incomplete *isIncomplete() const { return m_incomplete; }
    void setIncomplete(Incomplete *inc)    { delete m_incomplete; m_incomplete = inc; }

    bool mergeFrom(const InvItem &merge, bool prefer_from = false);

    typedef void *Diff; // opaque handle

    Diff *createDiff(const InvItem &diffto) const;
    bool applyDiff(Diff *diff);

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

    QString          m_custom_picture_url;
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

    friend QDataStream &operator << (QDataStream &ds, const InvItem &ii);
    friend QDataStream &operator >> (QDataStream &ds, InvItem &ii);

    friend class Core;
};

class InvItemMimeData : public QMimeData {
public:
    InvItemMimeData(const InvItemList &items);

    virtual QStringList formats() const;
    virtual bool hasFormat(const QString & mimeType) const;

    void setItems(const InvItemList &items);
    static InvItemList items(const QMimeData *md);

private:
    static const char *s_mimetype;
};

class Order {
public:
    enum Type { Received, Placed, Any };

    Order(const QString &id, Type type);

    QString id() const        { return m_id; }
    Type type() const         { return m_type; }
    QDateTime date() const    { return m_date; }
    QDateTime statusChange() const  { return m_status_change; }
    QString buyer() const     { return m_type == Received ? m_other : QString(); }
    QString seller() const    { return m_type == Placed ? m_other : QString(); }
    money_t shipping() const  { return m_shipping; }
    money_t insurance() const { return m_insurance; }
    money_t delivery() const  { return m_delivery; }
    money_t credit() const    { return m_credit; }
    money_t grandTotal() const{ return m_grand_total; }
    QString status() const    { return m_status; }
    QString payment() const   { return m_payment; }
    QString remarks() const   { return m_remarks; }

    void setId(const QString &id)             { m_id = id; }
    void setDate(const QDateTime &dt)         { m_date = dt; }
    void setStatusChange(const QDateTime &dt) { m_status_change = dt; }
    void setBuyer(const QString &str)         { m_other = str; m_type = Received; }
    void setSeller(const QString &str)        { m_other = str; m_type = Placed; }
    void setShipping(const money_t &m)        { m_shipping = m; }
    void setInsurance(const money_t &m)       { m_insurance = m; }
    void setDelivery(const money_t &m)        { m_delivery = m; }
    void setCredit(const money_t &m)          { m_credit = m; }
    void setGrandTotal(const money_t &m)      { m_grand_total = m; }
    void setStatus(const QString &str)        { m_status = str; }
    void setPayment(const QString &str)       { m_payment = str; }
    void setRemarks(const QString &str)       { m_remarks = str; }

private:
    QString   m_id;
    Type      m_type;
    QDateTime m_date;
    QDateTime m_status_change;
    QString   m_other;
    money_t   m_shipping;
    money_t   m_insurance;
    money_t   m_delivery;
    money_t   m_credit;
    money_t   m_grand_total;
    QString   m_status;
    QString   m_payment;
    QString   m_remarks;
};

class PriceGuide : public CRef {
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool high_priority = false);
    QDateTime lastUpdate() const      { return m_fetched; }

    bool valid() const                { return m_valid; }
    int updateStatus() const          { return m_update_status; }

    int quantity(Time t, Condition c) const           { return m_quantities [t < TimeCount ? t : 0][c < ConditionCount ? c : 0]; }
    int lots(Time t, Condition c) const               { return m_lots [t < TimeCount ? t : 0][c < ConditionCount ? c : 0]; }
    money_t price(Time t, Condition c, Price p) const { return m_prices [t < TimeCount ? t : 0][c < ConditionCount ? c : 0][p < PriceCount ? p : 0]; }

    virtual ~PriceGuide();

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
    PriceGuide(const Item *item, const Color *color);

    void load_from_disk();
    void save_to_disk();

    bool parse(const char *data, uint size);

    friend class Core;
};

class TextImport {
public:
    TextImport();

    bool import(const QString &path);
    void exportTo(Core *);

    bool importInventories(const QString &path, QVector<const Item *> &items);
    void exportInventoriesTo(Core *);

    const QHash<int, const Color *>    &colors() const      { return m_colors; }
    const QHash<int, const Category *> &categories() const  { return m_categories; }
    const QHash<int, const ItemType *> &itemTypes() const   { return m_item_types; }
    const QVector<const Item *>        &items() const       { return m_items; }

private:
    template <typename T> T *parse(uint count, const char **strs);    // , T *gcc_dummy );
//  Category *parse ( uint count, const char **strs, Category * );
//  Color *parse ( uint count, const char **strs, Color * );
//  ItemType *parse ( uint count, const char **strs, ItemType * );
//  Item *parse ( uint count, const char **strs, Item * );

    template <typename C> bool readDB(const QString &name, C &container);
    template <typename T> bool readDB_processLine(QHash<int, const T *> &d, uint tokencount, const char **tokens);
    template <typename T> bool readDB_processLine(QVector<const T *> &v, uint tokencount, const char **tokens);

    struct btinvlist_dummy { };
    bool readDB_processLine(btinvlist_dummy &, uint count, const char **strs);

    bool readColorGuide(const QString &name);
    bool readPeeronColors(const QString &name);

    bool readInventory(const QString &path, const Item *item);

    const Category *findCategoryByName(const char *name, int len = -1);
    const Item *findItem(char type, const char *id);
    void appendCategoryToItemType(const Category *cat, ItemType *itt);

private:
    QHash<int, const Color *>    m_colors;
    QHash<int, const ItemType *> m_item_types;
    QHash<int, const Category *> m_categories;
    QVector<const Item *>        m_items;

    QHash<const Item *, Item::AppearsIn> m_appears_in_hash;
    QHash<const Item *, InvItemList>     m_consists_of_hash;

    const ItemType *m_current_item_type;
};


enum ModelRoles {
    RoleBase = 0x05c136c8,  // printf "0x%08x\n" $(($RANDOM*$RANDOM))

    ColorPointerRole,
    CategoryPointerRole,
    ItemTypePointerRole,
    ItemPointerRole,

    RoleMax
};

class ColorModel : public QAbstractListModel {
    Q_OBJECT

public:
    ColorModel(const Core *core);

    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex index(const Color *color) const;
    const Color *color(const QModelIndex &index) const;

    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;
    virtual void sort(int column, Qt::SortOrder so);

    void setItemTypeFilter(const ItemType *it);
    void clearItemTypeFilter();

protected:
    void rebuildColorList();

    class Compare {
    public:
        Compare(bool asc);
        bool operator()(const Color *c1, const Color *c2);

    protected:
        bool m_asc;
    };

    QList<const Color *> m_colors;
    const ItemType *m_filter;
    Qt::SortOrder m_sorted;
};


class CategoryModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Feature {
        Default = 0,

        IncludeAllCategoriesItem,
    };
    Q_DECLARE_FLAGS(Features, Feature)

    static const Category *AllCategories;

    CategoryModel(Features f, const Core *core);

    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex index(const Category *category) const;
    const Category *category(const QModelIndex &index) const;

    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;
    virtual void sort(int column, Qt::SortOrder so);

    void setItemTypeFilter(const ItemType *it);
    void clearItemTypeFilter();

protected:
    void rebuildCategoryList();

    class Compare {
    public:
        Compare(bool asc);
        bool operator()(const Category *c1, const Category *c2);

    protected:
        bool m_asc;
    };

    QList<const Category *> m_categories;
    const ItemType *m_filter;
    Qt::SortOrder   m_sorted : 8;
    bool            m_hasall : 1;
};


class ItemTypeModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Feature {
        Default = 0,

        ExcludeWithoutInventory
    };
    Q_DECLARE_FLAGS(Features, Feature)

    ItemTypeModel(Features sub, const Core *core);

    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex index(const ItemType *itemtype) const;
    const ItemType *itemType(const QModelIndex &index) const;

    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;
    virtual void sort(int column, Qt::SortOrder so);

protected:
    class Compare {
    public:
        Compare(bool asc);
        bool operator()(const ItemType *c1, const ItemType *c2);

    protected:
        bool m_asc;
    };

    QList<const ItemType *> m_itemtypes;
};


class ItemModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Feature {
        Default = 0,
    };
    Q_DECLARE_FLAGS(Features, Feature)

    ItemModel(Features f, const Core *core);

    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex index(const Item *item) const;
    const Item *item(const QModelIndex &index) const;

    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;
    virtual void sort(int column, Qt::SortOrder so);

    void setItemTypeFilter(const ItemType *it);
    void setCategoryFilter(const Category *cat);
    void setTextFilter(const QString &regex);

    void clearItemTypeFilter();
    void clearCategoryFilter();
    void clearTextFilter();

    void setExcludeWithoutInventoryFilter(bool on);

protected:
    void rebuildItemList();

    class Compare {
    public:
        Compare(bool asc, bool on_id);
        bool operator()(const Item *i1, const Item *i2);

    protected:
        bool m_asc;
        bool m_id;
    };

    QVector<const Item *> m_items;
    const ItemType *m_type_filter;
    const Category *m_cat_filter;
    QString         m_text_filter;
    Qt::SortOrder   m_sorted : 8;
    int             m_sortcol : 8;
    bool            m_inv_filter : 1;
};




class Core : public QObject {
    Q_OBJECT
public:
    virtual ~Core();

    const QLocale &cLocale() const   { return m_c_locale; }

    QUrl url(UrlList u, const void *opt = 0, const void *opt2 = 0);

    QString dataPath() const;
    QString dataPath(const ItemType *) const;
    QString dataPath(const Item *) const;
    QString dataPath(const Item *, const Color *) const;

    QString defaultDatabaseName() const;

    const QHash<int, const Color *>    &colors() const;
    const QHash<int, const Category *> &categories() const;
    const QHash<int, const ItemType *> &itemTypes() const;
    const QVector<const Item *>        &items() const;



    ColorModel *colorModel() const;
    CategoryModel *categoryModel(CategoryModel::Features f) const;
    ItemTypeModel *itemTypeModel(ItemTypeModel::Features f) const;
    ItemModel *itemModel(ItemModel::Features f) const;

    const QPixmap *noImage(const QSize &s);

    QImage colorImage(const Color *col, int w, int h) const;

    const Color *color(uint id) const;
    const Color *colorFromPeeronName(const char *peeron_name) const;
    const Color *colorFromLDrawId(int ldraw_id) const;
    const Category *category(uint id) const;
    const Category *category(const char *name, int len = -1) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const char *id) const;

    PriceGuide *priceGuide(const Item *item, const Color *color, bool high_priority = false);

    InvItemList *load(QIODevice *f, uint *invalid_items = 0);
    InvItemList *loadXML(QIODevice *f, uint *invalid_items = 0);
    InvItemList *loadBTI(QIODevice *f, uint *invalid_items = 0);

    QSize pictureSize(const ItemType *itt) const;
    Picture *picture(const Item *item, const Color *color, bool high_priority = false);
    Picture *largePicture(const Item *item, bool high_priority = false);

    InvItemList *parseItemListXML(QDomElement root, ItemListXMLHint hint, uint *invalid_items = 0);
    QDomElement createItemListXML(QDomDocument doc, ItemListXMLHint hint, const InvItemList *items, QMap <QString, QString> *extra = 0);

    bool parseLDrawModel(QFile &file, InvItemList &items, uint *invalid_items = 0);

    bool onlineStatus() const;

public slots:
    bool readDatabase(const QString &fname = QString());
    bool writeDatabase(const QString &fname = QString());

    void updatePriceGuide(PriceGuide *pg, bool high_priority = false);
    void updatePicture(Picture *pic, bool high_priority = false);

    void setOnlineStatus(bool on);
    void setUpdateIntervals(int pic, int pg);
    void setHttpProxy(const QNetworkProxy &proxy);

    void cancelPictureTransfers();
    void cancelPriceGuideTransfers();

signals:
    void priceGuideUpdated(BrickLink::PriceGuide *pg);
    void pictureUpdated(BrickLink::Picture *inv);

    void priceGuideProgress(int, int);
    void pictureProgress(int, int);

private:
    Core(const QString &datadir);

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);

private:
    bool updateNeeded(const QDateTime &last, int iv);
    bool parseLDrawModelInternal(QFile &file, const QString &model_name, InvItemList &items, uint *invalid_items, QHash<QString, InvItem *> &mergehash);
    void pictureIdleLoader2();

    void setDatabase_ConsistsOf(const QHash<const Item *, InvItemList> &hash);
    void setDatabase_AppearsIn(const QHash<const Item *, Item::AppearsIn> &hash);
    void setDatabase_Basics(const QHash<int, const Color *> &colors,
                            const QHash<int, const Category *> &categories,
                            const QHash<int, const ItemType *> &item_types,
                            const QVector<const Item *> &items);

    friend class TextImport;

private slots:
    void pictureIdleLoader();

    void pictureJobFinished(CTransferJob *);
    void priceGuideJobFinished(CTransferJob *);

private:
    QString  m_datadir;
    bool     m_online;
    QLocale  m_c_locale;

    QHash<QString, QPixmap *>  m_noimages;

    struct dummy1 {
        QHash<int, const Color *>       colors;      // id ->Color *
        QHash<int, const Category *>    categories;  // id ->Category *
        QHash<int, const ItemType *>    item_types;  // id ->ItemType *
        QVector<const Item *>           items;       // sorted array of Item *
    } m_databases;

    struct dummy2 {
        CTransfer *               transfer;
        int                       update_iv;

        CAsciiRefCache<PriceGuide, 500> cache;
    } m_price_guides;

    struct dummy3 {
        CTransfer *               transfer;
        int                       update_iv;

        QList <Picture *>         diskload;

        CAsciiRefCache<Picture, 500>    cache;
    } m_pictures;
};

inline Core *core() { return Core::inst(); }
inline Core *inst() { return core(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace BrickLink


Q_DECLARE_METATYPE(const BrickLink::Color *)
Q_DECLARE_METATYPE(const BrickLink::Category *)
Q_DECLARE_METATYPE(const BrickLink::ItemType *)

Q_DECLARE_OPERATORS_FOR_FLAGS(BrickLink::CategoryModel::Features)
Q_DECLARE_OPERATORS_FOR_FLAGS(BrickLink::ItemTypeModel::Features)


#endif

