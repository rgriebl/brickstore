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
#ifndef __DOCUMENT_H__
#define __DOCUMENT_H__

#include <QAbstractTableModel>
#include <QDomDocument>
#include <QPixmap>
#include <QUuid>

#include "bricklink.h"
#include "filter.h"

class QUndoStack;
class QUndoCommand;
class QItemSelectionModel;
class AddRemoveCmd;
class ChangeCmd;


class Document : public QAbstractTableModel {
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
        Item();
        Item(const BrickLink::InvItem &);
        Item(const Item &);
        virtual ~Item();

        Item &operator = (const Item &);
        bool operator == (const Item &) const;

        quint64 errors() const          { return m_errors; }
        void setErrors(quint64 errors) { m_errors = errors; }

        QImage image() const;
        QPixmap pixmap() const;

    private:
        quint64 m_errors;

        friend class Document;
    };

    typedef QList<Item *>      ItemList;
    typedef int                Position;
    typedef QVector<Position>  PositionVector;

    class Statistics {
    public:
        uint lots() const         { return m_lots; }
        uint items() const        { return m_items; }
        money_t value() const     { return m_val; }
        money_t minValue() const  { return m_minval; }
        double weight() const     { return m_weight; }
        uint errors() const       { return m_errors; }

    private:
        friend class Document;

        Statistics(const Document *doc, const ItemList &list);

        uint m_lots;
        uint m_items;
        money_t m_val;
        money_t m_minval;
        double m_weight;
        uint m_errors;
    };



    // Itemviews API
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    Item *item(const QModelIndex &idx) const;
    QModelIndex index(const Item *i) const;

    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    Qt::ItemFlags flags(const QModelIndex&) const;
    bool setData(const QModelIndex&, const QVariant&, int);
    QVariant dataForEditRole(Item *it, Field f) const;
    QString dataForDisplayRole(Item *it, Field f) const;
    QVariant dataForDecorationRole(Item *it, Field f) const;
    int dataForTextAlignmentRole(Item *it, Field f) const;
    QString dataForToolTipRole(Item *it, Field f) const;
    static QString headerDataForDisplayRole(Field f);
    int headerDataForTextAlignmentRole(Field f) const;
    int headerDataForDefaultWidthRole(Field f) const;

    QItemSelectionModel *selectionModel() const;

public slots:
    void pictureUpdated(BrickLink::Picture *pic);

public:
    Document(bool dont_sort = false);
    virtual ~Document();

    static const QList<Document *> &allDocuments();
    static QList<ItemList> restoreAutosave();

    QString fileName() const;
    QString title() const;
    bool doNotSortItems() const;

    const BrickLink::Order *order() const;

    bool isModified() const;

    const ItemList &items() const;
    const ItemList &selection() const;

    bool clear();

    bool insertItems(const ItemList &positions, const ItemList &list);
    bool removeItems(const ItemList &positions);

    bool insertItem(Item *position, Item *item);
    bool removeItem(Item *position);

    bool changeItem(Item *position, const Item &item);

    void resetDifferences(const ItemList &items);

    Statistics statistics(const ItemList &list) const;

    quint64 errorMask() const;
    void setErrorMask(quint64);

    static Document *fileNew();
    static Document *fileOpen();
    static Document *fileOpen(const QString &name);
    static Document *fileImportBrickLinkInventory(const BrickLink::Item *preselect = 0);
    static QList<Document *> fileImportBrickLinkOrders();
    static Document *fileImportBrickLinkStore();
    static Document *fileImportBrickLinkCart();
    static Document *fileImportBrickLinkXML();
    static Document *fileImportPeeronInventory();
    static Document *fileImportBrikTrakInventory(const QString &fn = QString::null);
    static Document *fileImportLDrawModel();

public slots:
    void setFileName(const QString &str);
    void setTitle(const QString &str);

    void setSelection(const ItemList &);

    void fileSave(const ItemList &itemlist);
    void fileSaveAs(const ItemList &itemlist);
    void fileExportBrickLinkXML(const ItemList &itemlist);
    void fileExportBrickLinkXMLClipboard(const ItemList &itemlist);
    void fileExportBrickLinkUpdateClipboard(const ItemList &itemlist);
    void fileExportBrickLinkInvReqClipboard(const ItemList &itemlist);
    void fileExportBrickLinkWantedListClipboard(const ItemList &itemlist);
    void fileExportBrikTrakInventory(const ItemList &itemlist);

public:
    void beginMacro(const QString &label = QString());
    void endMacro(const QString &label = QString());

    QUndoStack *undoStack() const;

signals:
    void itemsAdded(const Document::ItemList &);
    void itemsAboutToBeRemoved(const Document::ItemList &);
    void itemsRemoved(const Document::ItemList &);
    void itemsChanged(const Document::ItemList &, bool);

    void errorsChanged(Document::Item *);
    void statisticsChanged();
    void fileNameChanged(const QString &);
    void titleChanged(const QString &);
    void modificationChanged(bool);
    void selectionChanged(const Document::ItemList &);

private slots:
    void clean2Modified(bool);
    void selectionHelper();
    void autosave();

private:
    static Document *fileLoadFrom(const QString &s, const char *type, bool import_only = false);
    bool fileSaveTo(const QString &s, const char *type, bool export_only, const ItemList &itemlist);
    void setBrickLinkItems(const BrickLink::InvItemList &bllist, uint multiply = 1);

    void insertItemsDirect(ItemList &items, ItemList &positions);
    void removeItemsDirect(ItemList &items, ItemList &positions);
    void changeItemDirect(Item *position, Item &item);

    friend class AddRemoveCmd;
    friend class ChangeCmd;

    void updateErrors(Item *);

private:
    ItemList         m_items;
    ItemList         m_selection;

    quint64          m_error_mask;
    QString          m_filename;
    QString          m_title;
    bool             m_dont_sort;
    QUuid            m_uuid;  // for autosave
    QTimer           m_autosave_timer;

    QUndoStack *     m_undo;

    BrickLink::Order *m_order;

    QItemSelectionModel *m_selection_model;

    static QList<Document *> s_documents;
};


class DocumentProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    DocumentProxyModel(Document *model);
    ~DocumentProxyModel();

    inline Document::Item *item(const QModelIndex &idx) const  { return static_cast<Document *>(sourceModel())->item(mapToSource(idx)); }

//    using QSortFilterProxyModel::index;
//    const AppearsInItem *appearsIn(const QModelIndex &idx) const;
//    QModelIndex index(const AppearsInItem *const_ai) const;

    void setFilterExpression(const QString &filter);
    QString filterExpression() const;

protected:
//    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
    virtual bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const;
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
    QString         m_filter_expression;
    Filter::Parser *m_parser;
    QList<Filter>   m_filter;
};


QDataStream &operator << (QDataStream &ds, const Document::Item &item);
QDataStream &operator >> (QDataStream &ds, Document::Item &item);

#endif
