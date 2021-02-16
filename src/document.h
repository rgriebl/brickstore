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
    BrickLink::InvItem *item(const QModelIndex &idx) const;
    QModelIndex index(const BrickLink::InvItem *i, int column = 0) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = { }) const override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
                                int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex&) const override;
    bool setData(const QModelIndex&, const QVariant&, int) override;
    QVariant dataForEditRole(const Item *it, Field f) const;
    QString dataForDisplayRole(const BrickLink::InvItem *it, Field f) const;
    QVariant dataForFilterRole(const Item *it, Field f) const;
    QVariant dataForDecorationRole(const Item *it, Field f) const;
    Qt::CheckState dataForCheckStateRole(const Item *it, Field f) const;
    int dataForTextAlignmentRole(const Item *it, Field f) const;
    QString dataForToolTipRole(const Item *it, Field f) const;
    static QString headerDataForDisplayRole(Field f);
    int headerDataForTextAlignmentRole(Field f) const;
    int headerDataForDefaultWidthRole(Field f) const;

    QString subConditionLabel(BrickLink::SubCondition sc) const;

    bool isSorted() const;
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;
    void sort(int column, Qt::SortOrder order) override;

    bool isFiltered() const;
    QString filter() const;
    void setFilter(const QString &filter);

    void nextSortFilterIsDirect(); // hack for the Window c'tor ... find something better
    void reapplySortFilter();

    BrickLink::InvItemList sortItemList(const BrickLink::InvItemList &list) const;

    QString filterToolTip() const;

    bool lastCommandWasSortFilter() const;
    bool lastCommandWasActivateDifferenceMode() const;

public slots:
    void pictureUpdated(BrickLink::Picture *pic);

public:
    Document();
    Document(const BrickLink::InvItemList &items);
    Document(const BrickLink::InvItemList &items, const QString &currencyCode);
    virtual ~Document() override;

    static Document *createTemporary(const BrickLink::InvItemList &list,
                                     const QVector<int> &fakeIndexes = { });

    static const QVector<Document *> &allDocuments();

    QString fileName() const;
    QString title() const;
    QString fileNameOrTitle() const;

    const BrickLink::Order *order() const;

    bool isModified() const;
    void unsetModified(); // only for DocumentIO::fileSaveTo

    const ItemList &items() const;
    bool clear();

    void appendItem(Item *item);
    void insertItemsAfter(Item *afterItem, const BrickLink::InvItemList &items);

    void removeItems(const ItemList &items);
    void removeItem(Item *item);

    void changeItem(Item *item, const Item &value);

    Statistics statistics(const ItemList &list, bool ignoreExcluded = false) const;

    quint64 errorMask() const;
    void setErrorMask(quint64);

    QPair<quint64, quint64> itemFlags(const Item *item) const;

    bool legacyCurrencyCode() const;
    QString currencyCode() const;
    void setCurrencyCode(const QString &code, qreal crate = qreal(1));

    bool hasGuiState() const;
    QDomElement guiState() const;
    void setGuiState(QDomElement dom);
    void clearGuiState();
    void setGuiStateModified(bool modified);

    void setOrder(BrickLink::Order *order);

public slots:
    void setFileName(const QString &str);
    void setTitle(const QString &title);


public:
    void beginMacro(const QString &label = QString());
    void endMacro(const QString &label = QString());

    QUndoStack *undoStack() const;

    bool isDifferenceModeActive() const;
    void setDifferenceModeActive(bool active);
    const Item *differenceBaseItem(const Item *item) const;

    void activateDifferenceModeInternal(const QHash<const Item *, Item> &differenceBase = { }); // only for DocumentIO

    QByteArray saveSortFilterState() const;
    bool restoreSortFilterState(const QByteArray &ba);

signals:
    void itemFlagsChanged(const Document::Item *);
    void statisticsChanged();
    void fileNameChanged(const QString &);
    void titleChanged(const QString &);
    void modificationChanged(bool);
    void currencyCodeChanged(const QString &ccode);
    void differenceModeChanged(bool differenceMode);
    void itemCountChanged(int itemCount);
    void filterChanged(const QString &filter);
    void sortOrderChanged(Qt::SortOrder order);
    void sortColumnChanged(int column);
    void lastCommandWasSortFilterChanged(bool b);

protected:
    bool event(QEvent *e) override;
    virtual bool filterAcceptsItem(const BrickLink::InvItem *item) const;

private:
    Document(int dummy);

    void setFakeIndexes(const QVector<int> &fakeIndexes);

    void setItemsDirect(const ItemList &items);
    void insertItemsDirect(const ItemList &items, QVector<int> &positions, QVector<int> &viewPositions);
    void removeItemsDirect(ItemList &items, QVector<int> &positions, QVector<int> &viewPositions);
    void changeItemDirect(BrickLink::InvItem *item, Item &value);
    void changeCurrencyDirect(const QString &ccode, qreal crate, double *&prices);
    void setDifferenceModeActiveDirect(bool active);
    void sortFilterDirect(int column, Qt::SortOrder order, const QString &filterString,
                          const QVector<Filter> &filterList, BrickLink::InvItemList &unsorted);

    void emitDataChanged(const QModelIndex &tl = { }, const QModelIndex &br = { });
    void emitStatisticsChanged();
    void updateItemFlags(const Item *item);
    void setItemFlags(const Item *item, quint64 errors, quint64 updated);

    void invalidateFilterInternal();
    int compare(const BrickLink::InvItem *i1, const BrickLink::InvItem *i2, int sortColumn) const;
    void languageChange();

    friend class AddRemoveCmd;
    friend class ChangeCmd;
    friend class CurrencyCmd;
    friend class SortFilterCmd;
    friend class DifferenceModeCmd;

private:
    QVector<BrickLink::InvItem *> m_items;
    QHash<const Item *, Item> m_differenceBase;
    QVector<int>     m_fakeIndexes; // for the consolidate dialogs
    QHash<const Item *, QPair<quint64, quint64>> m_itemFlags;

    QVector<BrickLink::InvItem *> m_viewItems;

    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    bool m_nextSortFilterIsDirect = false;
    QScopedPointer<Filter::Parser> m_filterParser;
    QString m_filterString;
    QVector<Filter> m_filterList;

    bool m_differenceModeActive = false;

    QString          m_currencycode;
    quint64          m_error_mask = 0;
    QString          m_filename;
    QString          m_title;

    UndoStack *      m_undo = nullptr;

    BrickLink::Order *m_order = nullptr;

    bool              m_gui_state_modified = false;
    QDomElement       m_gui_state;

    QTimer *          m_delayedEmitOfStatisticsChanged = nullptr;
    QTimer *          m_delayedEmitOfDataChanged = nullptr;
    QPair<QPoint, QPoint> m_nextDataChangedEmit;

    static QVector<Document *> s_documents;
};

Q_DECLARE_METATYPE(Document *)
Q_DECLARE_METATYPE(const Document *)
