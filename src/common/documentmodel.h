// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>

#include <QAbstractTableModel>
#include <QPixmap>
#include <QUuid>
#include <QTimer>
#include <QElapsedTimer>
#include <QMimeData>

#include "bricklink/global.h"
#include "bricklink/lot.h"
#include "bricklink/io.h"
#include "documentio.h"
#include "common/filter.h"

QT_FORWARD_DECLARE_CLASS(QUndoStack)
class UndoStack;
QT_FORWARD_DECLARE_CLASS(QUndoCommand)
class AddRemoveCmd;
class ChangeCmd;

using BrickLink::Lot;
using BrickLink::LotList;

class DocumentStatistics
{
    Q_GADGET
    Q_DECLARE_TR_FUNCTIONS(DocumentStatistics)
    Q_PROPERTY(int lots READ lots)
    Q_PROPERTY(int items READ items)
    Q_PROPERTY(double value READ value)
    Q_PROPERTY(double minValue READ minValue)
    Q_PROPERTY(double cost READ cost)
    Q_PROPERTY(double weight READ weight)
    Q_PROPERTY(int errors READ errors)
    Q_PROPERTY(int differences READ differences)
    Q_PROPERTY(int incomplete READ incomplete)
    Q_PROPERTY(QString currencyCode READ currencyCode)

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

    DocumentStatistics() = default;

    Q_INVOKABLE QString asHtmlTable() const;

private:
    DocumentStatistics(const DocumentModel *model, const LotList &list, bool ignoreExcluded,
                       bool ignorePriceAndQuantityErrors = false);

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

    friend class DocumentModel;
};


class DocumentModel : public QAbstractTableModel
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
        TotalWeight,
        YearReleased,
        Marker,
        DateAdded,
        DateLastSold,
        Weight,
        AlternateIds,

        FieldCount
    };
    Q_ENUM(Field)

    enum ExtraRoles {
        FilterRole = Qt::UserRole,
        BaseDisplayRole, // like Qt::DisplayRole, but for differenceBase
        BaseToolTipRole, // like Qt::ToolTipRole, but for differenceBase
        BaseEditRole,    // like Qt::EditRole, but for differenceBase
        LotPointerRole,
        BaseLotPointerRole,
        ErrorFlagsRole,
        DifferenceFlagsRole,
        DocumentFieldRole,
        NullValueRole,

        HeaderDefaultWidthRole,
        HeaderValueModelRole,
        HeaderFilterableRole,
        HeaderNullValueRole,
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

    using FieldMergeModes = QHash<Field, MergeMode>;

    struct Consolidate
    {
        explicit Consolidate(const LotList &lots)
            : lots(lots)
        { }

        BrickLink::LotList lots;          // in
        int destinationIndex = -1;        // out (-1 for "just add")
        bool doNotDeleteEmpty = false;    // out (not used for addItem mode)
        FieldMergeModes fieldMergeModes;  // out
    };

    using ConsolidateFunction = QCoro::Task<bool> (DocumentModel *, QVector<Consolidate> &, bool /*add item*/);

    enum class AddLotMode {
        AddAsNew,
        ConsolidateWithExisting,
        ConsolidateInteractive,
    };
    QCoro::Task<int> addLots(BrickLink::LotList &&lots, AddLotMode addLotMode = AddLotMode::AddAsNew);
    QCoro::Task<> consolidateLots(BrickLink::LotList lots);

    static void setConsolidateFunction(const std::function<ConsolidateFunction> &consolidateFunction);

    static MergeModes possibleMergeModesForField(Field field);
    static FieldMergeModes createFieldMergeModes(MergeMode mergeMode = MergeMode::Ignore);
    static bool canLotsBeMerged(const Lot &lot1, const Lot &lot2);
    static bool mergeLotFields(const Lot &from, Lot &to, const FieldMergeModes &fieldMergeModes);

    static constexpr int maxQuantity = 9999999;
    static constexpr double maxPrice = 99999;

    static double maxLocalPrice(const QString &currencyCode);

    // Itemviews API
    Lot *lot(const QModelIndex &idx) const;
    QModelIndex index(const Lot *i, int column = 0) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = { }) const override;

   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex&) const override;
    bool setData(const QModelIndex&, const QVariant&, int) override;
    QString dataForDisplayRole(const Lot *lot, Field f, bool asToolTip) const;
    QVariant dataForEditRole(const Lot *lot, Field f) const;
    QVariant dataForFilterRole(const Lot *lot, Field f) const;
    QHash<int, QByteArray> roleNames() const override;

    bool isSorted() const;
    QVector<QPair<int, Qt::SortOrder>> sortColumns() const;
    void multiSort(const QVector<QPair<int, Qt::SortOrder>> &columns);

    bool isFiltered() const;
    const QVector<Filter> &filter() const;
    void setFilter(const QVector<Filter> &filterList);

    void reSort();
    void reFilter();

    inline int lotCount() const { return rowCount(); }
    LotList sortLotList(const LotList &list) const;

    QString filterToolTip() const;

    void resetDifferenceMode(const LotList &lotList);

public slots:
    void pictureUpdated(BrickLink::Picture *pic);

public:
    DocumentModel();
    DocumentModel(BrickLink::IO::ParseResult &&pr, bool forceModified = false);

    static DocumentModel *createTemporary(const LotList &list,
                                     const QVector<int> &fakeIndexes = { });

    ~DocumentModel() override;

    static const QVector<DocumentModel *> &allDocuments();

    int fixedLotCount() const    { return m_fixedLotCount; }
    int invalidLotCount() const  { return m_invalidLotCount; }

    bool isModified() const;
    bool canBeSaved() const;
    void unsetModified(); // only for DocumentIO::fileSaveTo
    QHash<const Lot *, Lot> differenceBase() const; // only for DocumentIO::fileSaveTo

    const LotList &lots() const;
    const LotList &sortedLots() const;
    const LotList &filteredLots() const;
    bool clear();

    void appendLot(Lot * &&lot);
    void appendLots(BrickLink::LotList &&lots);
    void insertLotsAfter(const LotList &afterLots, BrickLink::LotList &&lots);
    void insertLotsAfter(const Lot *afterLot, BrickLink::LotList &&lots);

    void removeLots(const LotList &lots);
    void removeLot(Lot *lot);

    void changeLot(Lot *lot, const Lot &value, DocumentModel::Field hint = DocumentModel::FieldCount);
    void changeLots(const std::vector<std::pair<Lot *, Lot>> &changes,
                     DocumentModel::Field hint = DocumentModel::FieldCount);

    DocumentStatistics statistics(const LotList &list, bool ignoreExcluded,
                                  bool ignorePriceAndQuantityErrors = false) const;

    void setLotFlagsMask(QPair<quint64, quint64> flagsMask);

    QPair<quint64, quint64> lotFlags(const Lot *lot) const;

    bool legacyCurrencyCode() const;
    QString currencyCode() const;
    void setCurrencyCode(const QString &code, double crate = 1.);

    void adjustLotCurrencyToModel(LotList &lots, const QString &fromCurrency) const;

    Filter::Parser *filterParser();

public:
    void beginMacro(const QString &label = QString());
    void endMacro(const QString &label = QString());

    QUndoStack *undoStack() const;

    enum ApplyToResult {
        LotChanged = 1,
        LotDidNotChange = 0,
    };

    void applyTo(const LotList &lots, const std::function<ApplyToResult(const Lot &, Lot &)> &callback,
                 const QString &actionText = { });

    const Lot *differenceBaseLot(const Lot *lot) const;

    QByteArray saveSortFilterState() const;
    bool restoreSortFilterState(const QByteArray &ba);

    QStringList hasDifferenceUpdateWarnings(const LotList &lots);

    void sortDirectForDocument(const QVector<QPair<int, Qt::SortOrder>> &columns);

signals:
    void lotFlagsChanged(const BrickLink::Lot *);
    void statisticsChanged();
    void modificationChanged(bool);
    void currencyCodeChanged(const QString &ccode);
    void lotCountChanged(int lotCount);
    void filteredLotCountChanged(int filteredLotCount);
    void filterChanged(const QVector<Filter> &filter);
    void sortColumnsChanged(const QVector<QPair<int, Qt::SortOrder>> &columns);
    void lastCommandWasVisualChanged(bool b);
    void isSortedChanged(bool b);
    void isFilteredChanged(bool b);

protected:
    bool event(QEvent *e) override;
    virtual bool filterAcceptsLot(const Lot *lot) const;

private:
    DocumentModel(int dummy);

    void setFakeIndexes(const QVector<int> &fakeIndexes);
    void rebuildLotIndex();
    void rebuildFilteredLotIndex();

    void setLotsDirect(const LotList &lots);
    void insertLotsDirect(const LotList &lots, QVector<int> &positions, QVector<int> &sortedPositions, QVector<int> &filteredPositions);
    void removeLotsDirect(const LotList &lots, QVector<int> &positions, QVector<int> &sortedPositions, QVector<int> &filteredPositions);
    void changeLotsDirect(std::vector<std::pair<Lot *, Lot> > &changes);
    void changeCurrencyDirect(const QString &ccode, double crate, double *&prices);
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
        enum class Type { String, Integer, NonLocalizedInteger, Currency, Date, Weight, Enum, Special };
        Type type = Type::String;
        int defaultWidth = 8;
        int alignment = Qt::AlignLeft;
        bool editable = true;
        bool checkable = false;
        bool filterable = true;
        int integerNullValue = 0;
        const char *title;
        using EnumDefinition = std::tuple<qint64, QString, QString>;  // value, tooltip, filter
        std::vector<EnumDefinition> enumValues = { };
        std::function<QAbstractItemModel *()> valueModelFn = { };
        std::function<QVariant(const Lot *)> dataFn = { };
        std::function<QVariant(const Lot *)> auxDataFn = { };
        std::function<void(Lot *, const QVariant &v)> setDataFn = { };
        std::function<QString(const Lot *, bool asToolTip)> displayFn = { };
        std::function<std::partial_ordering(const Lot *, const Lot *)> compareFn = { };
    };
    QHash<int, Column> m_columns;

    QVector<Lot *> m_lots;
    QVector<Lot *> m_sortedLots;
    QVector<Lot *> m_filteredLots;

    mutable QHash<const Lot *, int> m_lotIndex;
    mutable QHash<const Lot *, int> m_filteredLotIndex;

    QHash<const Lot *, Lot> m_differenceBase;
    QVector<int>     m_fakeIndexes; // for the consolidate dialogs
    QHash<const Lot *, QPair<quint64, quint64>> m_lotFlags;

    QVector<QPair<int, Qt::SortOrder>> m_sortColumns = { { -1, Qt::AscendingOrder } };
    std::unique_ptr<Filter::Parser> m_filterParser;
    QVector<Filter> m_filter;

    bool m_isSorted = false;   // freshly sorted, no changes
    bool m_isFiltered = false; // freshly filtered, no changes

    QString          m_currencycode;
    QPair<quint64, quint64> m_lotFlagsMask = { 0, 0 };

    int m_fixedLotCount = 0;    // on load
    int m_invalidLotCount = 0;  // on load

    UndoStack *      m_undo = nullptr;
    int m_firstNonVisualIndex = 0;
    bool m_visuallyClean = true;

    QTimer *          m_delayedEmitOfStatisticsChanged = nullptr;
    QTimer *          m_delayedEmitOfDataChanged = nullptr;
    QPair<QPoint, QPoint> m_nextDataChangedEmit;

    static std::function<ConsolidateFunction> s_consolidateFunction;
};

class DocumentLotsMimeData : public QMimeData
{
    Q_OBJECT
public:
    DocumentLotsMimeData(const LotList &lots, const QString &currencyCode);

    QStringList formats() const override;
    bool hasFormat(const QString &mimeType) const override;

    static std::tuple<BrickLink::LotList, QString> lots(const QMimeData *md);

private:
    void setLots(const LotList &lots, const QString &currencyCode);

    static const QString s_mimetype;
};


Q_DECLARE_OPERATORS_FOR_FLAGS(DocumentModel::MergeModes)

Q_DECLARE_METATYPE(DocumentStatistics)
Q_DECLARE_METATYPE(DocumentModel *)
Q_DECLARE_METATYPE(DocumentModel::Field)
Q_DECLARE_METATYPE(const DocumentModel *)
