/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include "bricklinkfwd.h"

#include <QDateTime>
#include <QString>
#include <QColor>
#include <QObject>
#include <QImage>
#include <QtXml/QDomDocument>
#include <QLocale>
#include <QHash>
#include <QVector>
#include <QList>
#include <QMap>
#include <QPair>
#include <QUrl>
#include <QMimeData>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QMutex>
#include <QTimer>
#include <QCache>


#include <time.h>

#include "cref.h"
#include "cmoney.h"
#include "cthreadpool.h"
#include "ctransfer.h"

class QIODevice;
class QFile;


namespace BrickLink {

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
        quint64  m_cpart    : 1;
        quint64  m_reserved : 11;
#else
        quint64  m_reserved : 11;
        quint64  m_cpart    : 1;
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
    QString key() const               { return QString::number(m_image.cacheKey()); }

    int cost() const;

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid         : 1;
    int           m_update_status : 7;

    QImage        m_image;

private:
    Picture(const Item *item, const Color *color);

    void load_from_disk();
    void save_to_disk();

    void parse(const char *data, uint size);

    friend class Core;
    friend class PictureLoaderJob;
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

    bool alternate() const             { return m_alternate; }
    void setAlternate(bool a)          { m_alternate = a; }
    uint alternateId() const           { return m_alt_id; }
    void setAlternateId(uint aid)      { m_alt_id = aid; }

    bool counterPart() const           { return m_cpart; }
    void setCounterPart(bool b)        { m_cpart = b; }

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
    bool             m_alternate : 1;
    uint             m_alt_id    : 6;
    bool             m_cpart     : 1;
    int              m_xreserved : 1;

    QString          m_comments;
    QString          m_remarks;
    QString          m_reserved;

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
    Order(const QString &id, OrderType type);

    QString id() const        { return m_id; }
    OrderType type() const    { return m_type; }
    QDateTime date() const    { return m_date; }
    QDateTime statusChange() const  { return m_status_change; }
    //QString buyer() const     { return m_type == Received ? m_other : QString(); }
    //QString seller() const    { return m_type == Placed ? m_other : QString(); }
    QString other() const     { return m_other; }
    money_t shipping() const  { return m_shipping; }
    money_t insurance() const { return m_insurance; }
    money_t delivery() const  { return m_delivery; }
    money_t credit() const    { return m_credit; }
    money_t grandTotal() const{ return m_grand_total; }
    QString status() const    { return m_status; }
    QString payment() const   { return m_payment; }
    QString remarks() const   { return m_remarks; }
    QString address() const   { return m_address; }

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
    void setAddress(const QString &str)       { m_address = str; }

private:
    QString   m_id;
    OrderType m_type;
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
    QString   m_address;
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

// semi-internal class for TextImport -> Core communciation
struct AllTimePriceGuide {
    const BrickLink::Item * item;
    const BrickLink::Color *color;
    struct {
        uint    quantity;
        money_t minPrice;
        money_t avgPrice;
        money_t maxPrice;
    } condition[ConditionCount];
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
    struct btpriceguide_dummy { };
    bool readDB_processLine(btpriceguide_dummy &, uint count, const char **strs);

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

    QHash<const Item *, AppearsIn>   m_appears_in_hash;
    QHash<const Item *, InvItemList> m_consists_of_hash;
    QList<AllTimePriceGuide>         m_alltime_pg_list;

    const ItemType *m_current_item_type;
};


class ColorModel : public QAbstractListModel {
    Q_OBJECT

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;

    QModelIndex index(const Color *color) const;
    const Color *color(const QModelIndex &index) const;

protected:
    ColorModel(QObject *parent = 0);

    QList<const Color *> m_colors;

    friend class Core;
};


class ColorProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    ColorProxyModel(ColorModel *model);

    using QSortFilterProxyModel::index;
    QModelIndex index(const Color *color) const;
    const Color *color(const QModelIndex &index) const;

    void setFilterItemType(const ItemType *it);

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
    virtual void sort(int column, Qt::SortOrder order);

    const ItemType *m_itemtype_filter;
    Qt::SortOrder m_order;
};


class CategoryModel : public QAbstractListModel {
    Q_OBJECT

public:
    static const Category *AllCategories;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;

    QModelIndex index(const Category *category) const;
    const Category *category(const QModelIndex &index) const;

protected:
    CategoryModel(QObject *parent = 0);

    QList<const Category *> m_categories;

    friend class Core;
};


class CategoryProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    CategoryProxyModel(CategoryModel *model);

    void setFilterItemType(const ItemType *it);
    void setFilterAllCategories(bool);

    using QSortFilterProxyModel::index;
    QModelIndex index(const Category *category) const;
    const Category *category(const QModelIndex &index) const;

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    const ItemType *m_itemtype_filter;
    bool m_all_filter;
};


class ItemTypeModel : public QAbstractListModel {
    Q_OBJECT

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;

    QModelIndex index(const ItemType *itemtype) const;
    const ItemType *itemType(const QModelIndex &index) const;

protected:
    ItemTypeModel(QObject *parent = 0);

    QList<const ItemType *> m_itemtypes;

    friend class Core;
};


class ItemTypeProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    ItemTypeProxyModel(ItemTypeModel *model);

    void setFilterWithoutInventory(bool on);

    using QSortFilterProxyModel::index;
    QModelIndex index(const ItemType *itemtype) const;
    const ItemType *itemType(const QModelIndex &index) const;

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    bool m_inv_filter;
};


class ItemModel : public QAbstractTableModel {
    Q_OBJECT

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;

    QModelIndex index(const Item *item) const;
    const Item *item(const QModelIndex &index) const;

    // shortcut for ItemProxyModel::filterAcceptsRow()
    inline const Item *itemAtRow(int row) const { return m_items.at(row); }

    void setItemType(const ItemType *itemtype);

protected slots:
    void pictureUpdated(BrickLink::Picture *);

protected:
    ItemModel(QObject *parent = 0);

    QVector<const Item *> m_items;
    const ItemType *m_itemtype;

    friend class Core;
};

class ItemProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    ItemProxyModel(ItemModel *model);

    void setFilterItemType(const ItemType *it);
    void setFilterCategory(const Category *it);
    void setFilterText(const QString &str);
    void setFilterWithoutInventory(bool on);

    using QSortFilterProxyModel::index;
    QModelIndex index(const Item *item) const;
    const Item *item(const QModelIndex &index) const;

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

protected slots:
    void invalidateFilterSlot();

protected:
    const ItemType *m_itemtype_filter;
    const Category *m_category_filter;
    QRegExp         m_text_filter;
    bool            m_inv_filter;
    QTimer          m_filter_timer;
};


class AppearsInModel : public QAbstractTableModel {
    Q_OBJECT
public:
    AppearsInModel(const Item *item, const Color *color);
    ~AppearsInModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orient, int role) const;

    const AppearsInItem *appearsIn(const QModelIndex &idx) const;
    QModelIndex index(const AppearsInItem *const_ai) const;

protected:
    const Item *        m_item;
    const Color *       m_color;
    AppearsIn     m_appearsin;
    QList<AppearsInItem *> m_items;
};

class AppearsInProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    AppearsInProxyModel(AppearsInModel *model);

    using QSortFilterProxyModel::index;
    const AppearsInItem *appearsIn(const QModelIndex &idx) const;
    QModelIndex index(const AppearsInItem *const_ai) const;

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};


class Core : public QObject {
    Q_OBJECT
public:
    virtual ~Core();

    const QLocale &cLocale() const   { return m_c_locale; }

    QUrl url(UrlList u, const void *opt = 0, const void *opt2 = 0);

    enum DatabaseVersion {
        BrickStore_1_1,
        BrickStore_2_0,

        Default = BrickStore_2_0
    };

    QString defaultDatabaseName(DatabaseVersion version = Default) const;

    QString dataPath() const;
    QString dataPath(const ItemType *) const;
    QString dataPath(const Item *) const;
    QString dataPath(const Item *, const Color *) const;

    const QHash<int, const Color *>    &colors() const;
    const QHash<int, const Category *> &categories() const;
    const QHash<int, const ItemType *> &itemTypes() const;
    const QVector<const Item *>        &items() const;

    ColorModel *colorModel();
    CategoryModel *categoryModel();
    ItemTypeModel *itemTypeModel();
    ItemModel *itemModel();

    const QPixmap *noImage(const QSize &s) const;

    const QPixmap *colorImage(const Color *col, int w, int h) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const char *name) const;
    const Color *colorFromPeeronName(const char *peeron_name) const;
    const Color *colorFromLDrawId(int ldraw_id) const;
    const Category *category(uint id) const;
    const Category *categoryFromName(const char *name, int len = -1) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const char *id) const;

    AllTimePriceGuide allTimePriceGuide(const Item *item, const Color *color) const;

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
    bool readDatabase(const QString &filename = QString());
    bool writeDatabase(const QString &filename, DatabaseVersion version);

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

    void setDatabase_ConsistsOf(const QHash<const Item *, InvItemList> &hash);
    void setDatabase_AppearsIn(const QHash<const Item *, AppearsIn> &hash);
    void setDatabase_Basics(const QHash<int, const Color *> &colors,
                            const QHash<int, const Category *> &categories,
                            const QHash<int, const ItemType *> &item_types,
                            const QVector<const Item *> &items);
    void setDatabase_AllTimePG(const QList<AllTimePriceGuide> &list);

    friend class TextImport;

private slots:
    void pictureJobFinished(CThreadPoolJob *);
    void priceGuideJobFinished(CThreadPoolJob *);

    void pictureLoaded(CThreadPoolJob *);

private:
    QString  m_datadir;
    bool     m_online;
    QLocale  m_c_locale;
    mutable QMutex m_corelock;

    mutable QHash<QString, QPixmap *>  m_noimages;
    mutable QHash<QString, QPixmap *>  m_colimages;

    QHash<int, const Color *>       m_colors;      // id ->Color *
    QHash<int, const Category *>    m_categories;  // id ->Category *
    QHash<int, const ItemType *>    m_item_types;  // id ->ItemType *
    QVector<const Item *>           m_items;       // sorted array of Item *

    ColorModel *   m_color_model;
    CategoryModel *m_category_model;
    ItemTypeModel *m_itemtype_model;
    ItemModel *    m_item_model;

    CTransfer                  m_pg_transfer;
    int                        m_pg_update_iv;
    QCache<quint64, PriceGuide> m_pg_cache;

    CTransfer                 m_pic_transfer;
    int                       m_pic_update_iv;

    CThreadPool               m_pic_diskload;

    QCache<quint64, Picture>  m_pic_cache;

    struct alltimepg_record {
        union {
            quint32 m_id;
            struct {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                quint32 m_item_index  : 20;
                quint32 m_color_id    : 12;
#else
                quint32 m_color_id    : 12;
                quint32 m_item_index  : 20;
#endif
            };
        };
        quint16 m_new_qty;
        quint16 m_used_qty;
#pragma warning(disable:4200)
        float   m_prices[];
#pragma warning(default:4200)
    };

    QVector<char>  m_alltime_pg; // really alltimepg_records
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Color *)
Q_DECLARE_METATYPE(const BrickLink::Category *)
Q_DECLARE_METATYPE(const BrickLink::ItemType *)
Q_DECLARE_METATYPE(const BrickLink::Item *)
Q_DECLARE_METATYPE(const BrickLink::AppearsInItem *)


template<> inline bool qIsDetached<BrickLink::Picture>(BrickLink::Picture &c) { return c.refCount() == 0; }
template<> inline bool qIsDetached<BrickLink::PriceGuide>(BrickLink::PriceGuide &c) { return c.refCount() == 0; }

#endif

