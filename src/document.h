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
#include <QPixmap>
#include <QUuid>
#include <QElapsedTimer>
#include <QMimeData>


#include "bricklink/bricklinkfwd.h"
#include "lot.h"
#include "documentio.h"
#include "utility/filter.h"

QT_FORWARD_DECLARE_CLASS(QUndoStack)
class UndoStack;
QT_FORWARD_DECLARE_CLASS(QUndoCommand)
class AddRemoveCmd;
class ChangeCmd;


class Document : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(bool isSorted READ isSorted NOTIFY isSortedChanged)
    Q_PROPERTY(bool isFiltered READ isFiltered NOTIFY isFilteredChanged)

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
        Marker,

        FieldCount
    };
    Q_ENUM(Field)

    enum ExtraRoles {
        FilterRole = Qt::UserRole,
        BaseDisplayRole, // like Qt::DisplayRole, but for differenceBase
        BaseEditRole,    // like Qt::EditRole, but for differenceBase
        LotPointerRole,
        BaseLotPointerRole,
        ErrorFlagsRole,
        DifferenceFlagsRole,

        HeaderDefaultWidthRole,
        HeaderValueModelRole,
        HeaderFilterableRole,
    };

    enum class MergeMode {
        Ignore       = 0,
        Copy         = 1,
        Merge        = 2,
        MergeText    = 4,
        MergeAverage = 8,
    };
    Q_DECLARE_FLAGS(MergeModes, MergeMode)
    Q_FLAG(MergeMode)


    class Statistics
    {
    public:
        int lots() const             { return m_lots; }
        int items() const            { return m_items; }
        double value() const         { return m_val; }
        double minValue() const      { return m_minval; }
        double cost() const          { return m_cost; }
        double weight() const        { return m_weight; }
        int errors() const           { return m_errors; }
        int differences() const      { return m_differences; }
        int incomplete() const       { return m_incomplete; }
        QString currencyCode() const { return m_ccode; }

    private:
        Statistics(const Document *doc, const LotList &list, bool ignoreExcluded);

        int m_lots;
        int m_items;
        double m_val;
        double m_minval;
        double m_cost;
        double m_weight;
        int m_errors;
        int m_differences;
        int m_incomplete;
        QString m_ccode;

        friend class Document;
    };

    // Itemviews API
    Lot *lot(const QModelIndex &idx) const;
    QModelIndex index(const Lot *i, int column = 0) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = { }) const override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
                                int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex&) const override;
    bool setData(const QModelIndex&, const QVariant&, int) override;
    QVariant dataForEditRole(const Lot *lot, Field f) const;
    QVariant dataForDisplayRole(const Lot *lot, Field f) const;
    QVariant dataForFilterRole(const Lot *lot, Field f) const;

    bool isSorted() const;
    QVector<QPair<int, Qt::SortOrder>> sortColumns() const;
    void sort(int column, Qt::SortOrder order) override;
    void sort(const QVector<QPair<int, Qt::SortOrder>> &columns);

    bool isFiltered() const;
    const QVector<Filter> &filter() const;
    void setFilter(const QVector<Filter> &filterList);

    void nextSortFilterIsDirect(); // hack for the Window c'tor ... find something better
    void reSort();
    void reFilter();

    inline int lotCount() const { return rowCount(); }
    LotList sortLotList(const LotList &list) const;

    QString filterToolTip() const;

    void resetDifferenceMode(const LotList &lotList);

public slots:
    void pictureUpdated(BrickLink::Picture *pic);

public:
    Document();
    Document(const DocumentIO::BsxContents &bsx, bool forceModified = false);
    virtual ~Document() override;

    static Document *createTemporary(const LotList &list,
                                     const QVector<int> &fakeIndexes = { });

    static const QVector<Document *> &allDocuments();

    QString fileName() const;
    QString title() const;
    QString fileNameOrTitle() const;

    const BrickLink::Order *order() const;

    bool isModified() const;
    void unsetModified(); // only for DocumentIO::fileSaveTo
    QHash<const Lot *, Lot> differenceBase() const; // only for DocumentIO::fileSaveTo

    const LotList &lots() const;
    const LotList &sortedLots() const;
    const LotList &filteredLots() const;
    bool clear();

    void appendLot(Lot *lot);
    void appendLots(const LotList &lots);
    void insertLotsAfter(const Lot *afterLot, const LotList &lots);

    void removeLots(const LotList &lots);
    void removeLot(Lot *lot);

    void changeLot(Lot *lot, const Lot &value, Document::Field hint = Document::FieldCount);
    void changeLots(const std::vector<std::pair<Lot *, Lot>> &changes,
                     Document::Field hint = Document::FieldCount);

    Statistics statistics(const LotList &list, bool ignoreExcluded) const;

    void setLotFlagsMask(QPair<quint64, quint64> flagsMask);

    QPair<quint64, quint64> lotFlags(const Lot *lot) const;

    bool legacyCurrencyCode() const;
    QString currencyCode() const;
    void setCurrencyCode(const QString &code, qreal crate = qreal(1));

    void setOrder(BrickLink::Order *order);

    static bool canLotsBeMerged(const Lot &lot1, const Lot &lot2);
    static bool mergeLotFields(const Lot &from, Lot &to, MergeMode defaultMerge,
                                const QHash<Field, MergeMode> &fieldMerge = { });

    Filter::Parser *filterParser();

public slots:
    void setFileName(const QString &str);
    void setTitle(const QString &title);


public:
    void beginMacro(const QString &label = QString());
    void endMacro(const QString &label = QString());

    QUndoStack *undoStack() const;

    const Lot *differenceBaseLot(const Lot *lot) const;

    QByteArray saveSortFilterState() const;
    bool restoreSortFilterState(const QByteArray &ba);

signals:
    void lotFlagsChanged(const Lot *);
    void statisticsChanged();
    void fileNameChanged(const QString &);
    void titleChanged(const QString &);
    void modificationChanged(bool);
    void currencyCodeChanged(const QString &ccode);
    void lotCountChanged(int lotCount);
    void filterChanged(const QVector<Filter> &filter);
    void sortColumnsChanged(const QVector<QPair<int, Qt::SortOrder>> &columns);
    void lastCommandWasVisualChanged(bool b);
    void isSortedChanged(bool b);
    void isFilteredChanged(bool b);

protected:
    bool event(QEvent *e) override;
    virtual bool filterAcceptsLot(const Lot *lot) const;

private:
    Document(int dummy);

    void setFakeIndexes(const QVector<int> &fakeIndexes);

    void setLotsDirect(const LotList &lots);
    void insertLotsDirect(const LotList &lots, QVector<int> &positions, QVector<int> &sortedPositions, QVector<int> &filteredPositions);
    void removeLotsDirect(const LotList &lots, QVector<int> &positions, QVector<int> &sortedPositions, QVector<int> &filteredPositions);
    void changeLotsDirect(std::vector<std::pair<Lot *, Lot> > &changes);
    void changeCurrencyDirect(const QString &ccode, qreal crate, double *&prices);
    void resetDifferenceModeDirect(QHash<const Lot *, Lot>
                                   &differenceBase);
    void filterDirect(const QVector<Filter> &filterList, bool &filtered,
                      LotList &unfiltered);
    void sortDirect(const QVector<QPair<int, Qt::SortOrder>> &columns, bool &sorted, LotList &unsorted);

    void emitDataChanged(const QModelIndex &tl = { }, const QModelIndex &br = { });
    void emitStatisticsChanged();
    void updateLotFlags(const Lot *lot);
    void setLotFlags(const Lot *lot, quint64 errors, quint64 updated);

    void updateModified();

    void languageChange();

    void initializeColumns();

    friend class AddRemoveCmd;
    friend class ChangeCmd;
    friend class CurrencyCmd;
    friend class SortCmd;
    friend class FilterCmd;
    friend class ResetDifferenceModeCmd;

private:
    struct Column {
        int defaultWidth = 8;
        int alignment = Qt::AlignLeft;
        bool editable = true;
        bool checkable = false;
        bool filterable = true;
        const char *title;
        std::function<QAbstractItemModel *()> valueModelFn = { };
        std::function<QVariant(const Lot *)> dataFn = { };
        std::function<void(Lot *, const QVariant &v)> setDataFn = { };
        std::function<QVariant(const Lot *)> displayFn = { };
        std::function<QVariant(const Lot *)> filterFn = { };
        std::function<int(const Lot *, const Lot *)> compareFn;
    };
    QHash<int, Column> m_columns;

    QVector<Lot *> m_lots;
    QVector<Lot *> m_sortedLots;
    QVector<Lot *> m_filteredLots;

    QHash<const Lot *, Lot> m_differenceBase;
    QVector<int>     m_fakeIndexes; // for the consolidate dialogs
    QHash<const Lot *, QPair<quint64, quint64>> m_lotFlags;


    QVector<QPair<int, Qt::SortOrder>> m_sortColumns = { { -1, Qt::AscendingOrder } };
    QScopedPointer<Filter::Parser> m_filterParser;
    QVector<Filter> m_filter;

    bool m_isSorted = false;   // freshly sorted, no changes
    bool m_isFiltered = false; // freshly filtered, no changes
    bool m_nextSortFilterIsDirect = false;

    QString          m_currencycode;
    QPair<quint64, quint64> m_lotFlagsMask = { 0, 0 };
    QString          m_filename;
    QString          m_title;

    UndoStack *      m_undo = nullptr;
    int m_firstNonVisualIndex = 0;
    bool m_visuallyClean = true;

    BrickLink::Order *m_order = nullptr;

    QTimer *          m_delayedEmitOfStatisticsChanged = nullptr;
    QTimer *          m_delayedEmitOfDataChanged = nullptr;
    QPair<QPoint, QPoint> m_nextDataChangedEmit;

    static QVector<Document *> s_documents;
};

class DocumentLotsMimeData : public QMimeData
{
    Q_OBJECT
public:
    DocumentLotsMimeData(const LotList &lots);

    QStringList formats() const override;
    bool hasFormat(const QString &mimeType) const override;

    void setLots(const LotList &lots);
    static LotList lots(const QMimeData *md);

private:
    static const QString s_mimetype;
};


Q_DECLARE_OPERATORS_FOR_FLAGS(Document::MergeModes)

Q_DECLARE_METATYPE(Document *)
Q_DECLARE_METATYPE(Document::Field)
Q_DECLARE_METATYPE(const Document *)
Q_DECLARE_METATYPE(const Lot *)
