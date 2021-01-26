/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#pragma once

#include <QAbstractTableModel>
#include <QDomDocument>
#include <QPixmap>
#include <QUuid>
#include <QElapsedTimer>

#include "bricklink.h"
#include "filter.h"

QT_FORWARD_DECLARE_CLASS(QUndoStack)
class UndoStack;
QT_FORWARD_DECLARE_CLASS(QUndoCommand)
class AddRemoveCmd;
class ChangeCmd;


class Document : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Field {
        Index = 0,
        Status,
        Picture,
        PartNo,
        Description,
        Condition,
        Color,
        QuantityOrig,
        QuantityDiff,
        Quantity,
        PriceOrig,
        PriceDiff,
        Price,
        Total,
        Cost,
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

        FieldCount
    };

    enum ExtraRoles {
        FilterRole = Qt::UserRole
    };

    typedef BrickLink::InvItem Item;
    typedef QVector<BrickLink::InvItem *> ItemList;

    class Statistics
    {
    public:
        Statistics(const Document *doc, const ItemList &list, bool ignoreExcluded = false);

        int lots() const             { return m_lots; }
        int items() const            { return m_items; }
        double value() const         { return m_val; }
        double minValue() const      { return m_minval; }
        double cost() const          { return m_cost; }
        double weight() const        { return m_weight; }
        int errors() const           { return m_errors; }
        int incomplete() const       { return m_incomplete; }
        QString currencyCode() const { return m_ccode; }

    private:
        int m_lots;
        int m_items;
        double m_val;
        double m_minval;
        double m_cost;
        double m_weight;
        int m_errors;
        int m_incomplete;
        QString m_ccode;
    };



    // Itemviews API
    Item *item(const QModelIndex &idx) const;
    QModelIndex index(const Item *i, int column = 0) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    Qt::ItemFlags flags(const QModelIndex&) const;
    bool setData(const QModelIndex&, const QVariant&, int);
    QVariant dataForEditRole(Item *it, Field f) const;
    QString dataForDisplayRole(Item *it, Field f, int row) const;
    QVariant dataForFilterRole(Item *it, Field f, int row) const;
    QVariant dataForDecorationRole(Item *it, Field f) const;
    Qt::CheckState dataForCheckStateRole(Item *it, Field f) const;
    int dataForTextAlignmentRole(Item *it, Field f) const;
    QString dataForToolTipRole(Item *it, Field f, int row) const;
    static QString headerDataForDisplayRole(Field f);
    int headerDataForTextAlignmentRole(Field f) const;
    int headerDataForDefaultWidthRole(Field f) const;

    QString subConditionLabel(BrickLink::SubCondition sc) const;

public slots:
    void pictureUpdated(BrickLink::Picture *pic);

public:
    Document(const BrickLink::InvItemList &items = { }, const QString &currencyCode = { });
    virtual ~Document();

    static Document *createTemporary(const BrickLink::InvItemList &list,
                                     const QVector<int> &fakeIndexes = { });

    static const QVector<Document *> &allDocuments();

    QString fileName() const;
    QString title() const;
    QString fileNameOrTitle() const;

    const BrickLink::Order *order() const;

    bool isModified() const;

    const ItemList &items() const;
    bool clear();

    bool insertItems(const QVector<int> &positions, const ItemList &items);
    bool insertItem(int position, Item *item);
    bool appendItem(Item *item);

    bool removeItems(const ItemList &items);
    bool removeItem(Item *item);

    bool changeItem(int position, const Item &value);
    bool changeItem(Item *item, const Item &value);

    int positionOf(Item *item) const;
    const Item *itemAt(int position) const;

    void resetDifferences(const ItemList &items);

    Statistics statistics(const ItemList &list) const;

    quint64 errorMask() const;
    void setErrorMask(quint64);

    quint64 itemErrors(const Item *item) const;
    void setItemErrors(Item *item, quint64 errors);

    QString currencyCode() const;
    void setCurrencyCode(const QString &code, qreal crate = qreal(1));

    bool hasGuiState() const;
    QDomElement guiState() const;
    void setGuiState(QDomElement dom);
    void clearGuiState();

    void setGuiStateModified(bool modified);

    static Document *fileNew();
    static Document *fileOpen();
    static Document *fileOpen(const QString &name);
    static Document *fileImportBrickLinkInventory(const BrickLink::Item *preselect = nullptr,
                                                  int quantity = 1,
                                                  BrickLink::Condition condition = BrickLink::Condition::New);
    static QVector<Document *> fileImportBrickLinkOrders();
    static Document *fileImportBrickLinkStore();
    static Document *fileImportBrickLinkCart();
    static Document *fileImportBrickLinkXML();
    static Document *fileImportLDrawModel();

public slots:
    void setFileName(const QString &str);
    void setTitle(const QString &title);

    void fileSave();
    void fileSaveAs();
    void fileExportBrickLinkXML(const Document::ItemList &itemlist);
    void fileExportBrickLinkXMLClipboard(const Document::ItemList &itemlist);
    void fileExportBrickLinkUpdateClipboard(const Document::ItemList &itemlist);
    void fileExportBrickLinkInvReqClipboard(const Document::ItemList &itemlist);
    void fileExportBrickLinkWantedListClipboard(const Document::ItemList &itemlist);

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
    void currencyCodeChanged(const QString &ccode);

private:
    Document(int dummy);
    static Document *fileLoadFrom(const QString &s, const char *type, bool import_only = false);
    bool fileSaveTo(const QString &s, const char *type, bool export_only, const ItemList &itemlist);
    void setBrickLinkItems(const BrickLink::InvItemList &bllist);
    void setFakeIndexes(const QVector<int> &fakeIndexes);

    void insertItemsDirect(ItemList &items, QVector<int> &positions);
    void removeItemsDirect(ItemList &items, QVector<int> &positions);
    void changeItemDirect(int position, Item &item);
    void changeCurrencyDirect(const QString &ccode, qreal crate, double *&prices);

    void emitStatisticsChanged();

    friend class AddRemoveCmd;
    friend class ChangeCmd;
    friend class CurrencyCmd;

    void updateErrors(Item *);

private:
    ItemList         m_items;
    QVector<int>     m_fakeIndexes; // for the consolidate dialogs
    QHash<const Item *, quint64> m_errors;

    QString          m_currencycode;
    quint64          m_error_mask = 0;
    QString          m_filename;
    QString          m_title;

    UndoStack *      m_undo = nullptr;

    BrickLink::Order *m_order = nullptr;

    bool              m_gui_state_modified = false;
    QDomElement       m_gui_state;

    QElapsedTimer     m_lastEmitOfStatisticsChanged;

    static QVector<Document *> s_documents;
};


class DocumentProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    DocumentProxyModel(Document *model, QObject *parent = nullptr);
    ~DocumentProxyModel();

    inline Document::Item *item(const QModelIndex &idx) const  { return static_cast<Document *>(sourceModel())->item(mapToSource(idx)); }
    inline QModelIndex index(const Document::Item *i) const  { return mapFromSource(static_cast<Document *>(sourceModel())->index(i)); }
    using QSortFilterProxyModel::index;

    Document::ItemList sortItemList(const Document::ItemList &list) const;

    void setFilterExpression(const QString &filter);
    QString filterExpression() const;
    QString filterToolTip() const;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

signals:
    void titleChanged(const QString &title);
    void fileNameChanged(const QString &fileName);
    void filterExpressionChanged(const QString &filterExpression);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool event(QEvent *e) override;

private:
    void languageChange();
    int compare(const Document::Item *i1, const Document::Item *i2, int sortColumn) const;

    QString         m_filter_expression;
    Filter::Parser *m_parser;
    QVector<Filter> m_filter;

    int             m_lastSortColumn[2];

    friend class SortItemListCompare;
};

Q_DECLARE_METATYPE(Document *)
Q_DECLARE_METATYPE(const Document *)
