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
#include <utility>

#include <QApplication>
#include <QCursor>
#include <QFileInfo>
#include <QDir>
#include <QStringBuilder>
#include <QtConcurrentFilter>
#include <QtAlgorithms>

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "utility.h"
#include "config.h"
#include "undo.h"
#include "currency.h"
#include "qparallelsort.h"
#include "document.h"
#include "document_p.h"

using namespace std::chrono_literals;


template <auto G, auto S>
struct FieldOp
{
    template <typename Result> static Result returnType(Result (Document::Item::*)() const);
    typedef decltype(returnType(G)) R;

    static void copy(const Document::Item &from, Document::Item &to)
    {
        (to.*S)((from.*G)());
    }

    static bool merge(const Document::Item &from, Document::Item &to,
                      Document::MergeMode mergeMode = Document::MergeMode::Merge,
                      const R defaultValue = { })
    {
        if (mergeMode == Document::MergeMode::Ignore) {
            return false;
        } else if (mergeMode == Document::MergeMode::Copy) {
            copy(from, to);
            return true;
        } else {
            return mergeInternal(from, to, mergeMode, defaultValue);
        }
    }

private:
    template<class Q = R>
    static typename std::enable_if<!std::is_same<Q, double>::value && !std::is_same<Q, QString>::value, bool>::type
    mergeInternal(const Document::Item &from, Document::Item &to, Document::MergeMode mergeMode,
                  const R defaultValue)
    {
        Q_UNUSED(mergeMode)

        if (((from.*G)() != defaultValue) && ((to.*G)() == defaultValue)) {
            copy(from, to);
            return true;
        }
        return false;
    }

    template<class Q = R>
    static typename std::enable_if<std::is_same<Q, double>::value, bool>::type
    mergeInternal(const Document::Item &from, Document::Item &to, Document::MergeMode mergeMode,
                  const R defaultValue)
    {
        if (mergeMode == Document::MergeMode::MergeAverage) { // weighted by quantity
            const int fromQty = from.quantity();
            const int toQty = to.quantity();

            (to.*S)(((to.*G)() * toQty + (from.*G)() * fromQty) / (toQty + fromQty));
            return true;
        } else {
            if (!qFuzzyCompare((from.*G)(), defaultValue) && qFuzzyCompare((to.*G)(), defaultValue)) {
                copy(from, to);
                return true;
            }
        }
        return false;
    }

    template<class Q = R>
    static typename std::enable_if<std::is_same<Q, QString>::value, bool>::type
    mergeInternal(const Document::Item &from, Document::Item &to, Document::MergeMode mergeMode,
                  const R defaultValue)
    {
        if (mergeMode == Document::MergeMode::MergeText) { // add or remove "tags"
            QString f = (from.*G)();
            QString t = (to.*G)();

            if (!f.isEmpty() && !t.isEmpty() && (f != t)) {
                QRegularExpression fromRe { u"\\b" % QRegularExpression::escape(f) % u"\\b" };

                if (!fromRe.match(t).hasMatch()) {
                    QRegularExpression toRe { u"\\b" % QRegularExpression::escape(t) % u"\\b" };

                    if (toRe.match(f).hasMatch())
                        (to.*S)(f);
                    else
                        (to.*S)(t % u" " % f);
                    return true;
                }
            } else if (!f.isEmpty()) {
                (to.*S)(f);
                return true;
            }
        } else {
            if (((from.*G)() != defaultValue) && ((to.*G)() == defaultValue)) {
                copy(from, to);
                return true;
            }
        }
        return false;
    }
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


enum {
    CID_Change,
    CID_AddRemove,
    CID_Currency,
    CID_ResetDifferenceMode,
    CID_Sort,
    CID_Filter,

    // values starting at 0x00010000 are reserved for the view
};


AddRemoveCmd::AddRemoveCmd(Type t, Document *doc, const QVector<int> &positions,
                           const QVector<int> &sortedPositions,
                           const QVector<int> &filteredPositions, const Document::ItemList &items)
    : QUndoCommand(genDesc(t == Add, qMax(items.count(), positions.count())))
    , m_doc(doc)
    , m_positions(positions)
    , m_sortedPositions(sortedPositions)
    , m_filteredPositions(filteredPositions)
    , m_items(items)
    , m_type(t)
{
    // for add: specify items and optionally also positions
    // for remove: specify items only
}

AddRemoveCmd::~AddRemoveCmd()
{
    if (m_type == Add)
        qDeleteAll(m_items);
}

int AddRemoveCmd::id() const
{
    return CID_AddRemove;
}

void AddRemoveCmd::redo()
{
    if (m_type == Add) {
        // Document::insertItemsDirect() adds all m_items at the positions given in
        // m_*positions (or append them to the document in case m_*positions is empty)
        m_doc->insertItemsDirect(m_items, m_positions, m_sortedPositions, m_filteredPositions);
        m_positions.clear();
        m_sortedPositions.clear();
        m_filteredPositions.clear();
        m_type = Remove;
    }
    else {
        // Document::removeItemsDirect() removes all m_items and records the positions
        // in m_*positions
        m_doc->removeItemsDirect(m_items, m_positions, m_sortedPositions, m_filteredPositions);
        m_type = Add;
    }
}

void AddRemoveCmd::undo()
{
    redo();
}

QString AddRemoveCmd::genDesc(bool is_add, int count)
{
    if (is_add)
        return Document::tr("Added %n item(s)", nullptr, count);
    else
        return Document::tr("Removed %n item(s)", nullptr, count);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QTimer *ChangeCmd::s_eventLoopCounter = nullptr;

ChangeCmd::ChangeCmd(Document *doc, const std::vector<std::pair<Document::Item *, Document::Item>> &changes, Document::Field hint)
    : QUndoCommand()
    , m_doc(doc)
    , m_hint(hint)
    , m_changes(changes)
{
    std::sort(m_changes.begin(), m_changes.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });

    if (!s_eventLoopCounter) {
        s_eventLoopCounter = new QTimer(qApp);
        s_eventLoopCounter->setProperty("loopCount", uint(0));
        s_eventLoopCounter->setInterval(0);
        s_eventLoopCounter->setSingleShot(true);
        QObject::connect(s_eventLoopCounter, &QTimer::timeout,
                         s_eventLoopCounter, []() {
            uint lc = s_eventLoopCounter->property("loopCount").toUInt();
            s_eventLoopCounter->setProperty("loopCount", lc + 1);
        });
    }
    m_loopCount = s_eventLoopCounter->property("loopCount").toUInt();
    s_eventLoopCounter->start();

    updateText();
}

void ChangeCmd::updateText()
{
    //: Generic undo/redo text for table edits: %1 == column name (e.g. "Price")
    setText(QCoreApplication::translate("ChangeCmd", "Modified %1 on %Ln item(s)", nullptr,
                                        int(m_changes.size()))
            //: Generic undo/redo text for table edits: if more than one column was edited at once
            .arg((int(m_hint) >= 0) ? m_doc->headerData(m_hint, Qt::Horizontal).toString()
                                    : QCoreApplication::translate("ChangeCmd", "multiple fields")));
}

int ChangeCmd::id() const
{
    return CID_Change;
}

bool ChangeCmd::mergeWith(const QUndoCommand *other)
{
    if (other->id() == id()) {
        auto *otherChange = static_cast<const ChangeCmd *>(other);
        if ((m_loopCount == otherChange->m_loopCount) && (m_hint == otherChange->m_hint)) {
            std::copy_if(otherChange->m_changes.cbegin(), otherChange->m_changes.cend(),
                         std::back_inserter(m_changes), [this](const auto &change) {
                return !std::binary_search(m_changes.begin(), m_changes.end(),
                                           std::make_pair(change.first, Document::Item()),
                                           [](const auto &a, const auto &b) {
                    return a.first < b.first;
                });
            });
            updateText();
            return true;
        }
    }
    return false;
}

void ChangeCmd::redo()
{
    m_doc->changeItemsDirect(m_changes);
}

void ChangeCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


CurrencyCmd::CurrencyCmd(Document *doc, const QString &ccode, qreal crate)
    : QUndoCommand(qApp->translate("CurrencyCmd", "Changed currency"))
    , m_doc(doc)
    , m_ccode(ccode)
    , m_crate(crate)
    , m_prices(nullptr)
{ }

CurrencyCmd::~CurrencyCmd()
{
    delete [] m_prices;
}

int CurrencyCmd::id() const
{
    return CID_Currency;
}

void CurrencyCmd::redo()
{
    Q_ASSERT(!m_prices);

    QString oldccode = m_doc->currencyCode();
    m_doc->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}

void CurrencyCmd::undo()
{
    Q_ASSERT(m_prices);

    QString oldccode = m_doc->currencyCode();
    m_doc->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ResetDifferenceModeCmd::ResetDifferenceModeCmd(Document *doc)
    : QUndoCommand(qApp->translate("ResetDifferenceModeCmd", "Reset difference mode base values"))
    , m_doc(doc)
{ }

int ResetDifferenceModeCmd::id() const
{
    return CID_ResetDifferenceMode;
}

bool ResetDifferenceModeCmd::mergeWith(const QUndoCommand *other)
{
    return (other->id() == id());
}

void ResetDifferenceModeCmd::redo()
{
    m_doc->resetDifferenceModeDirect(m_differenceBase);
}

void ResetDifferenceModeCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SortCmd::SortCmd(Document *doc, int column, Qt::SortOrder order)
    : QUndoCommand(qApp->translate("SortCmd", "Sorted the view"))
    , m_doc(doc)
    , m_created(QDateTime::currentDateTime())
    , m_column(column)
    , m_order(order)
{ }

int SortCmd::id() const
{
    return CID_Sort;
}

bool SortCmd::mergeWith(const QUndoCommand *other)
{
    return (other->id() == id());
}

void SortCmd::redo()
{
    int oldColumn = m_doc->sortColumn();
    Qt::SortOrder oldOrder = m_doc->sortOrder();

    m_doc->sortDirect(m_column, m_order, m_isSorted, m_unsorted);

    m_column = oldColumn;
    m_order = oldOrder;
}

void SortCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


FilterCmd::FilterCmd(Document *doc, const QString &filterString, const QVector<Filter> &filterList)
    : QUndoCommand(qApp->translate("FilterCmd", "Filtered the view"))
    , m_doc(doc)
    , m_created(QDateTime::currentDateTime())
    , m_filterString(filterString)
    , m_filterList(filterList)
{ }

int FilterCmd::id() const
{
    return CID_Filter;
}

bool FilterCmd::mergeWith(const QUndoCommand *other)
{
    return (other->id() == id());
}

void FilterCmd::redo()
{
    QString oldFilterString = m_doc->filter();
    auto oldFilterList = m_doc->m_filterList;

    m_doc->filterDirect(m_filterString, m_filterList, m_isFiltered, m_unfiltered);

    m_filterString = oldFilterString;
    m_filterList = oldFilterList;
}

void FilterCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Document::Statistics::Statistics(const Document *doc, const ItemList &list, bool ignoreExcluded)
{
    m_lots = 0;
    m_items = 0;
    m_val = m_minval = m_cost = 0.;
    m_weight = .0;
    m_errors = 0;
    m_differences = 0;
    m_incomplete = 0;
    bool weight_missing = false;

    for (const Item *item : list) {
        if (ignoreExcluded && (item->status() == BrickLink::Status::Exclude))
            continue;

        ++m_lots;

        int qty = item->quantity();
        double price = item->price();

        m_val += (qty * price);
        m_cost += (qty * item->cost());

        for (int i = 0; i < 3; i++) {
            if (item->tierQuantity(i) && !qFuzzyIsNull(item->tierPrice(i)))
                price = item->tierPrice(i);
        }
        m_minval += (qty * price * (1.0 - double(item->sale()) / 100.0));
        m_items += qty;

        if (item->totalWeight() > 0)
            m_weight += item->totalWeight();
        else
            weight_missing = true;

        auto flags = doc->itemFlags(item);
        if (flags.first)
            m_errors += qPopulationCount(flags.first);
        if (flags.second)
            m_differences += qPopulationCount(flags.second);

        if (item->isIncomplete())
            m_incomplete++;
    }
    if (weight_missing)
        m_weight = qFuzzyIsNull(m_weight) ? -std::numeric_limits<double>::min() : -m_weight;
    m_ccode = doc->currencyCode();
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

QVector<Document *> Document::s_documents;

Document *Document::createTemporary(const BrickLink::InvItemList &list, const QVector<int> &fakeIndexes)
{
    auto *doc = new Document(1 /*dummy*/);
    ItemList items;

    // the caller owns the items, so we have to copy here
    for (const BrickLink::InvItem *item : list)
        items.append(new Item(*item));

    doc->setItemsDirect(items);
    doc->setFakeIndexes(fakeIndexes);
    return doc;
}

Document::Document(int /*is temporary*/)
    : m_filterParser(new Filter::Parser())
    , m_currencycode(Config::inst()->defaultCurrencyCode())
{
    MODELTEST_ATTACH(this)

    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            this, &Document::pictureUpdated);

    languageChange();
}

// the caller owns the items
Document::Document()
    : Document(0)
{
    m_undo = new UndoStack(this);
    connect(m_undo, &QUndoStack::cleanChanged,
            this, [this](bool clean) {
        if (clean) {
            m_firstNonVisualIndex = 0;
            m_visuallyClean = true;
        }
        updateModified();
    });
    connect(m_undo, &QUndoStack::indexChanged,
            this, [this](int index) {
        bool oldVisuallyClean = m_visuallyClean;
        int cleanIndex = m_undo->cleanIndex();

        if ((cleanIndex < 0) || (index < cleanIndex)) {
            m_visuallyClean = false;
        } else if (index == 0 || (index == cleanIndex)) {
            m_firstNonVisualIndex = 0;
            m_visuallyClean = true;
        } else if (m_firstNonVisualIndex && (index < m_firstNonVisualIndex)) {
            m_firstNonVisualIndex = 0;
            m_visuallyClean = true;
        } else {
            const auto *lastCmd = m_undo->command(index - 1);
            if (lastCmd && (lastCmd->id() < CID_Sort)) {
                if (!m_firstNonVisualIndex || (index < m_firstNonVisualIndex)) {
                    m_firstNonVisualIndex = index;
                    m_visuallyClean = false;
                }
            }
        }
        if (m_visuallyClean != oldVisuallyClean)
            updateModified();
    });
    s_documents.append(this);
}

// the caller owns the items
Document::Document(const DocumentIO::BsxContents &bsx, bool forceModified)
    : Document()
{
    // we take ownership of the items
    setItemsDirect(bsx.items);

    if (!bsx.currencyCode.isEmpty()) {
        if (bsx.currencyCode == QLatin1String("$$$")) // legacy USD
            m_currencycode.clear();
        else
            m_currencycode = bsx.currencyCode;
    }

    if (forceModified)
        m_undo->resetClean();

    auto db = bsx.differenceModeBase; // get rid of const
    if (!db.isEmpty())
        resetDifferenceModeDirect(db);
}

Document::~Document()
{
    delete m_order;
    qDeleteAll(m_items);

    s_documents.removeAll(this);
}

const QVector<Document *> &Document::allDocuments()
{
    return s_documents;
}

const Document::ItemList &Document::items() const
{
    return m_items;
}

Document::Statistics Document::statistics(const ItemList &list, bool ignoreExcluded) const
{
    return Statistics(this, list, ignoreExcluded);
}

void Document::beginMacro(const QString &label)
{
    m_undo->beginMacro(label);
}

void Document::endMacro(const QString &label)
{
    m_undo->endMacro(label);
}

QUndoStack *Document::undoStack() const
{
    return m_undo;
}


bool Document::clear()
{
    removeItems(m_items);
    return true;
}

void Document::appendItem(Item *item)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, { }, { }, { }, { item }));
}

void Document::appendItems(const Document::ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, { }, { }, { }, items));
}

void Document::insertItemsAfter(Item *afterItem, const BrickLink::InvItemList &items)
{
    if (items.isEmpty())
        return;

    int afterPos = m_items.indexOf(afterItem) + 1;
    int afterSortedPos = m_sortedItems.indexOf(afterItem) + 1;
    int afterFilteredPos = m_filteredItems.indexOf(afterItem) + 1;

    Q_ASSERT((afterPos > 0) && (afterSortedPos > 0));
    if (afterFilteredPos == 0)
        afterFilteredPos = m_filteredItems.size();

    QVector<int> positions(items.size());
    std::iota(positions.begin(), positions.end(), afterPos);
    QVector<int> sortedPositions(items.size());
    std::iota(sortedPositions.begin(), sortedPositions.end(), afterSortedPos);
    QVector<int> filteredPositions(items.size());
    std::iota(filteredPositions.begin(), filteredPositions.end(), afterFilteredPos);

    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, sortedPositions,
                                  filteredPositions, items));
}

void Document::removeItem(Item *item)
{
    return removeItems({ item });
}

void Document::removeItems(const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, { }, { }, { }, items));
}

void Document::changeItem(Item *item, const Item &value, Document::Field hint)
{
    m_undo->push(new ChangeCmd(this, {{ item, value }}, hint));
}

void Document::changeItems(const std::vector<std::pair<Item *, Item>> &changes, Field hint)
{
    if (!changes.empty())
        m_undo->push(new ChangeCmd(this, changes, hint));
}

void Document::setItemsDirect(const Document::ItemList &items)
{
    if (items.isEmpty())
        return;
    QVector<int> posDummy, sortedPosDummy, filteredPosDummy;
    insertItemsDirect(items, posDummy, sortedPosDummy, filteredPosDummy);
}

void Document::insertItemsDirect(const ItemList &items, QVector<int> &positions,
                                 QVector<int> &sortedPositions, QVector<int> &filteredPositions)
{
    auto pos = positions.constBegin();
    auto sortedPos = sortedPositions.constBegin();
    auto filteredPos = filteredPositions.constBegin();
    bool isAppend = positions.isEmpty();

    Q_ASSERT((positions.size() == sortedPositions.size())
             && (positions.size() == filteredPositions.size()));
    Q_ASSERT(isAppend != (positions.size() == items.size()));

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    for (Item *item : qAsConst(items)) {
        if (!isAppend) {
            m_items.insert(*pos++, item);
            m_sortedItems.insert(*sortedPos++, item);
            m_filteredItems.insert(*filteredPos++, item);
        } else {
            m_items.append(item);
            m_sortedItems.append(item);
            m_filteredItems.append(item);
        }

        // this is really a new item, not just a redo - start with no differences
        if (!m_differenceBase.contains(item))
            m_differenceBase.insert(item, *item);

        updateItemFlags(item);
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(item(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    emit itemCountChanged(m_items.count());
    emitStatisticsChanged();

    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void Document::removeItemsDirect(ItemList &items, QVector<int> &positions,
                                 QVector<int> &sortedPositions, QVector<int> &filteredPositions)
{
    positions.resize(items.count());
    sortedPositions.resize(items.count());
    filteredPositions.resize(items.count());

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    for (int i = items.count() - 1; i >= 0; --i) {
        Item *item = items.at(i);
        int idx = m_items.indexOf(item);
        int sortIdx = m_sortedItems.indexOf(item);
        int filterIdx = m_filteredItems.indexOf(item);
        Q_ASSERT(idx >= 0 && sortIdx >= 0);
        positions[i] = idx;
        sortedPositions[i] = sortIdx;
        filteredPositions[i] = filterIdx;
        m_items.removeAt(idx);
        m_sortedItems.removeAt(sortIdx);
        m_filteredItems.removeAt(filterIdx);
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(item(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    emit itemCountChanged(m_items.count());
    emitStatisticsChanged();

    //TODO: we should remember and re-apply the isSorted/isFiltered state
    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void Document::changeItemsDirect(std::vector<std::pair<Item *, Item>> &changes)
{
    Q_ASSERT(!changes.empty());

    for (auto &change : changes) {
        Item *item = change.first;
        std::swap(*item, change.second);

        QModelIndex idx1 = index(item, 0);
        QModelIndex idx2 = idx1.siblingAtColumn(columnCount() - 1);
        updateItemFlags(item);
        emitDataChanged(idx1, idx2);
    }

    emitStatisticsChanged();

    //TODO: we should remember and re-apply the isSorted/isFiltered state
    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void Document::changeCurrencyDirect(const QString &ccode, qreal crate, double *&prices)
{
    m_currencycode = ccode;

    if (!qFuzzyCompare(crate, qreal(1)) || (ccode != m_currencycode)) {
        bool createPrices = (prices == nullptr);
        if (createPrices)
            prices = new double[5 * m_items.count()];

        for (int i = 0; i < m_items.count(); ++i) {
            Item *item = m_items[i];
            if (createPrices) {
                prices[i * 5] = item->cost();
                prices[i * 5 + 1] = item->price();
                prices[i * 5 + 2] = item->tierPrice(0);
                prices[i * 5 + 3] = item->tierPrice(1);
                prices[i * 5 + 4] = item->tierPrice(2);

                item->setCost(prices[i * 5] * crate);
                item->setPrice(prices[i * 5 + 1] * crate);
                item->setTierPrice(0, prices[i * 5 + 2] * crate);
                item->setTierPrice(1, prices[i * 5 + 3] * crate);
                item->setTierPrice(2, prices[i * 5 + 4] * crate);
            } else {
                item->setCost(prices[i * 5]);
                item->setPrice(prices[i * 5 + 1]);
                item->setTierPrice(0, prices[i * 5 + 2]);
                item->setTierPrice(1, prices[i * 5 + 3]);
                item->setTierPrice(2, prices[i * 5 + 4]);
            }
        }

        if (!createPrices) {
            delete [] prices;
            prices = nullptr;
        }

        emitDataChanged();
        emitStatisticsChanged();

        //TODO: we should remember and re-apply the isSorted/isFiltered state
        if (isSorted())
            emit isSortedChanged(m_isSorted = false);
        if (isFiltered())
            emit isFilteredChanged(m_isFiltered = false);
    }
    emit currencyCodeChanged(currencyCode());
}

void Document::emitDataChanged(const QModelIndex &tl, const QModelIndex &br)
{
    if (!m_delayedEmitOfDataChanged) {
        m_delayedEmitOfDataChanged = new QTimer(this);
        m_delayedEmitOfDataChanged->setSingleShot(true);
        m_delayedEmitOfDataChanged->setInterval(0);

        static auto resetNext = [](decltype(m_nextDataChangedEmit) &next) {
            next = {
                QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()),
                QPoint(std::numeric_limits<int>::min(), std::numeric_limits<int>::min())
            };
        };

        resetNext(m_nextDataChangedEmit);

        connect(m_delayedEmitOfDataChanged, &QTimer::timeout,
                this, [this]() {

            emit dataChanged(index(m_nextDataChangedEmit.first.y(),
                                   m_nextDataChangedEmit.first.x()),
                             index(m_nextDataChangedEmit.second.y(),
                                   m_nextDataChangedEmit.second.x()));

            resetNext(m_nextDataChangedEmit);
        });
    }

    QModelIndex xtl = tl.isValid() ? tl : index(0, 0);
    QModelIndex xbr = br.isValid() ? br : index(rowCount() - 1, columnCount() - 1);

    if (xtl.row() < m_nextDataChangedEmit.first.y())
        m_nextDataChangedEmit.first.setY(xtl.row());
    if (xtl.column() < m_nextDataChangedEmit.first.x())
        m_nextDataChangedEmit.first.setX(xtl.column());
    if (xbr.row() > m_nextDataChangedEmit.second.y())
        m_nextDataChangedEmit.second.setY(xbr.row());
    if (xbr.column() > m_nextDataChangedEmit.second.x())
        m_nextDataChangedEmit.second.setX(xbr.column());

    m_delayedEmitOfDataChanged->start();
}

void Document::emitStatisticsChanged()
{
    if (!m_delayedEmitOfStatisticsChanged) {
        m_delayedEmitOfStatisticsChanged = new QTimer(this);
        m_delayedEmitOfStatisticsChanged->setSingleShot(true);
        m_delayedEmitOfStatisticsChanged->setInterval(0);

        connect(m_delayedEmitOfStatisticsChanged, &QTimer::timeout,
                this, &Document::statisticsChanged);
    }
    m_delayedEmitOfStatisticsChanged->start();
}

void Document::updateItemFlags(const Item *item)
{
    quint64 errors = 0;
    quint64 updated = 0;

    if (item->price() <= 0)
        errors |= (1ULL << Price);
    if (item->quantity() <= 0)
        errors |= (1ULL << Quantity);
    if (item->color() && item->itemType() && ((item->color()->id() != 0) && !item->itemType()->hasColors()))
        errors |= (1ULL << Color);
    if (item->tierQuantity(0) && ((item->tierPrice(0) <= 0) || (item->tierPrice(0) >= item->price())))
        errors |= (1ULL << TierP1);
    if (item->tierQuantity(1) && ((item->tierPrice(1) <= 0) || (item->tierPrice(1) >= item->tierPrice(0))))
        errors |= (1ULL << TierP2);
    if (item->tierQuantity(1) && (item->tierQuantity(1) <= item->tierQuantity(0)))
        errors |= (1ULL << TierQ2);
    if (item->tierQuantity(2) && ((item->tierPrice(2) <= 0) || (item->tierPrice(2) >= item->tierPrice(1))))
        errors |= (1ULL << TierP3);
    if (item->tierQuantity(2) && (item->tierQuantity(2) <= item->tierQuantity(1)))
        errors |= (1ULL << TierQ3);

    if (auto base = differenceBaseItem(item)) {
        static const quint64 ignoreMask = 0ULL
                | (1ULL << Index)
                | (1ULL << Status)
                | (1ULL << Picture)
                | (1ULL << Description)
                | (1ULL << QuantityOrig)
                | (1ULL << QuantityDiff)
                | (1ULL << PriceOrig)
                | (1ULL << PriceDiff)
                | (1ULL << Total)
                | (1ULL << Category)
                | (1ULL << ItemType)
                | (1ULL << LotId)
                | (1ULL << Weight)
                | (1ULL << YearReleased);

        for (Field f = Index; f != FieldCount; f = Field(f + 1)) {
            quint64 fmask = (1ULL << f);
            if (fmask & ignoreMask)
                continue;
            if (dataForEditRole(item, f) != dataForEditRole(base, f))
                updated |= fmask;
        }
    }

    setItemFlags(item, errors, updated);
}

void Document::resetDifferenceMode()
{
    m_undo->push(new ResetDifferenceModeCmd(this));
}

void Document::resetDifferenceModeDirect(QHash<const BrickLink::InvItem *, BrickLink::InvItem>
                                         &differenceBase) {
    std::swap(m_differenceBase, differenceBase);

    if (m_differenceBase.isEmpty()) {
        for (const auto *item : qAsConst(m_items))
            m_differenceBase.insert(item, *item);
    }

    for (const auto *item : qAsConst(m_items))
        updateItemFlags(item);

    emitDataChanged();
}

const Document::Item *Document::differenceBaseItem(const Document::Item *item) const
{
    if (!item)
        return nullptr;

    auto it = m_differenceBase.constFind(item);
    return (it != m_differenceBase.end()) ? &(*it) : nullptr;
}

bool Document::legacyCurrencyCode() const
{
    return m_currencycode.isEmpty();
}

QString Document::currencyCode() const
{
    return m_currencycode.isEmpty() ? QString::fromLatin1("USD") : m_currencycode;
}

void Document::setCurrencyCode(const QString &ccode, qreal crate)
{
    if (ccode != currencyCode())
        m_undo->push(new CurrencyCmd(this, ccode, crate));
}

void Document::setOrder(BrickLink::Order *order)
{
    if (m_order)
        delete m_order;
    m_order = order;
}

bool Document::canItemsBeMerged(const Item &item1, const Item &item2)
{
    return ((&item1 != &item2)
            && !item1.isIncomplete()
            && !item2.isIncomplete()
            && (item1.item() == item2.item())
            && (item1.color() == item2.color())
            && (item1.condition() == item2.condition())
            && (item1.subCondition() == item2.subCondition()));
}

bool Document::mergeItemFields(const Item &from, Item &to, MergeMode defaultMerge,
                               const QHash<Field, MergeMode> &fieldMerge)
{
    if (!canItemsBeMerged(from, to))
        return false;

    if (fieldMerge.isEmpty()) {
        if (defaultMerge == MergeMode::Ignore) {
            return false;
        } else if (defaultMerge == MergeMode::Copy) {
            to = from;
            return true;
        }
    }

    auto mergeModeFor = [&fieldMerge, defaultMerge](Field f) -> MergeMode {
        auto mergeIt = fieldMerge.constFind(f);
        return (mergeIt == fieldMerge.cend()) ? defaultMerge : mergeIt.value();
    };

    bool changed = false;

    if (FieldOp<&Item::quantity, &Item::setQuantity>::merge(from, to, mergeModeFor(Quantity)))
        changed = true;
    if (FieldOp<&Item::price, &Item::setPrice>::merge(from, to, mergeModeFor(Price)))
        changed = true;
    if (FieldOp<&Item::cost, &Item::setCost>::merge(from, to, mergeModeFor(Cost)))
        changed = true;
    if (FieldOp<&Item::bulkQuantity, &Item::setBulkQuantity>::merge(from, to, mergeModeFor(Bulk)))
        changed = true;
    if (FieldOp<&Item::sale, &Item::setSale>::merge(from, to, mergeModeFor(Sale)))
        changed = true;
    if (FieldOp<&Item::comments, &Item::setComments>::merge(from, to, mergeModeFor(Comments)))
        changed = true;
    if (FieldOp<&Item::remarks, &Item::setRemarks>::merge(from, to, mergeModeFor(Remarks)))
        changed = true;
    if (FieldOp<&Item::tierQuantity0, &Item::setTierQuantity0>::merge(from, to, mergeModeFor(TierQ1)))
        changed = true;
    if (FieldOp<&Item::tierPrice0, &Item::setTierPrice0>::merge(from, to, mergeModeFor(TierP1)))
        changed = true;
    if (FieldOp<&Item::tierQuantity1, &Item::setTierQuantity1>::merge(from, to, mergeModeFor(TierQ2)))
        changed = true;
    if (FieldOp<&Item::tierPrice1, &Item::setTierPrice1>::merge(from, to, mergeModeFor(TierP2)))
        changed = true;
    if (FieldOp<&Item::tierQuantity2, &Item::setTierQuantity2>::merge(from, to, mergeModeFor(TierQ3)))
        changed = true;
    if (FieldOp<&Item::tierPrice2, &Item::setTierPrice2>::merge(from, to, mergeModeFor(TierP3)))
        changed = true;
    if (FieldOp<&Item::retain, &Item::setRetain>::merge(from, to, mergeModeFor(Retain)))
        changed = true;
    if (FieldOp<&Item::stockroom, &Item::setStockroom>::merge(from, to, mergeModeFor(Stockroom)))
        changed = true;
    if (FieldOp<&Item::reserved, &Item::setReserved>::merge(from, to, mergeModeFor(Reserved)))
        changed = true;

    return changed;
}

void Document::setFakeIndexes(const QVector<int> &fakeIndexes)
{
    m_fakeIndexes = fakeIndexes;
}

QString Document::fileName() const
{
    return m_filename;
}

void Document::setFileName(const QString &str)
{
    if (str != m_filename) {
        m_filename = str;
        emit fileNameChanged(str);
    }
}

QString Document::fileNameOrTitle() const
{
    QFileInfo fi(m_filename);
    if (fi.exists())
        return QDir::toNativeSeparators(fi.absoluteFilePath());

    if (!m_title.isEmpty())
        return m_title;

    return m_filename;
}

QString Document::title() const
{
    return m_title;
}

void Document::setTitle(const QString &str)
{
    if (str != m_title) {
        m_title = str;
        emit titleChanged(m_title);
    }
}

bool Document::isModified() const
{
    bool modified = !m_undo->isClean();
    if (modified && !Config::inst()->visualChangesMarkModified())
        modified = !m_visuallyClean;

    return modified;
}

void Document::unsetModified()
{
    m_undo->setClean();
    updateModified();
}

QHash<const Document::Item *, Document::Item> Document::differenceBase() const
{
    return m_differenceBase;
}

void Document::updateModified()
{
    emit modificationChanged(isModified());
}

void Document::setItemFlagsMask(QPair<quint64, quint64> flagsMask)
{
    m_itemFlagsMask = flagsMask;
    emitStatisticsChanged();
    emitDataChanged();
}

QPair<quint64, quint64> Document::itemFlags(const Item *item) const
{
    auto flags = m_itemFlags.value(item, { });
    flags.first &= m_itemFlagsMask.first;
    flags.second &= m_itemFlagsMask.second;
    return flags;
}

void Document::setItemFlags(const Item *item, quint64 errors, quint64 updated)
{
    if (!item)
        return;

    auto oldFlags = m_itemFlags.value(item, { });
    if (oldFlags.first != errors || oldFlags.second != updated) {
        if (errors || updated)
            m_itemFlags.insert(item, qMakePair(errors, updated));
        else
            m_itemFlags.remove(item);

        emit itemFlagsChanged(item);
        emitStatisticsChanged();
    }
}

const BrickLink::Order *Document::order() const
{
    return m_order;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
// Itemviews API
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


QModelIndex Document::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid() && row >= 0 && column >= 0 && row < rowCount() && column < columnCount())
        return createIndex(row, column, m_filteredItems.at(row));
    return { };
}

BrickLink::InvItem *Document::item(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<Item *>(idx.internalPointer()) : nullptr;
}

QModelIndex Document::index(const BrickLink::InvItem *item, int column) const
{
    int row = m_filteredItems.indexOf(const_cast<BrickLink::InvItem *>(item));
    if (row >= 0)
        return createIndex(row, column, const_cast<BrickLink::InvItem *>(item));
    return { };
}

int Document::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_filteredItems.size();
}

int Document::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : FieldCount;
}

Qt::ItemFlags Document::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemFlags();

    Qt::ItemFlags ifs = QAbstractItemModel::flags(index);

    switch (index.column()) {
    case Index       :
    case Total       :
    case ItemType    :
    case Category    :
    case YearReleased:
    case PriceOrig   :
    case QuantityOrig:
    case LotId       : break;
    case Retain      : ifs |= Qt::ItemIsUserCheckable; Q_FALLTHROUGH();
    default          : ifs |= Qt::ItemIsEditable; break;
    }
    return ifs;
}

bool Document::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (role != Qt::EditRole))
        return false;
    Item *itemp = this->item(index);
    Item item = *itemp;
    Document::Field f = static_cast<Field>(index.column());

    switch (f) {
    case Status      : item.setStatus(value.value<BrickLink::Status>()); break;
    case Picture     :
    case Description : item.setItem(value.value<const BrickLink::Item *>()); break;
    case PartNo      : {
        char itid = item.itemType() ? item.itemType()->id() : 'P';
        const BrickLink::Item *newitem = BrickLink::core()->item(itid, value.toString().toLatin1().constData());
        if (newitem)
            item.setItem(newitem);
        break;
    }
    case Condition   : item.setCondition(value.value<BrickLink::Condition>()); break;
    case Color       : item.setColor(value.value<const BrickLink::Color *>()); break;
    case QuantityDiff: {
        if (auto base = differenceBaseItem(itemp))
            item.setQuantity(base->quantity() + value.toInt());
        break;
    }
    case Quantity    : item.setQuantity(value.toInt()); break;
    case PriceDiff   :  {
        if (auto base = differenceBaseItem(itemp))
            item.setPrice(base->price() + value.toDouble());
        break;
    }
    case Price       : item.setPrice(value.toDouble()); break;
    case Cost        : item.setCost(value.toDouble()); break;
    case Bulk        : item.setBulkQuantity(value.toInt()); break;
    case Sale        : item.setSale(value.toInt()); break;
    case Comments    : item.setComments(value.toString()); break;
    case Remarks     : item.setRemarks(value.toString()); break;
    case TierQ1      : item.setTierQuantity(0, value.toInt()); break;
    case TierQ2      : item.setTierQuantity(1, value.toInt()); break;
    case TierQ3      : item.setTierQuantity(2, value.toInt()); break;
    case TierP1      : item.setTierPrice(0, value.toDouble()); break;
    case TierP2      : item.setTierPrice(1, value.toDouble()); break;
    case TierP3      : item.setTierPrice(2, value.toDouble()); break;
    case Retain      : item.setRetain(value.toBool()); break;
    case Stockroom   : item.setStockroom(value.value<BrickLink::Stockroom>()); break;
    case Reserved    : item.setReserved(value.toString()); break;
    case Weight      : item.setTotalWeight(value.toDouble()); break;
    default          : break;
    }
    if (item != *itemp) {
        changeItem(itemp, item, f);
        return true;
    }
    return false;
}


QVariant Document::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        const BrickLink::InvItem *it = item(index);
        auto f = static_cast<Field>(index.column());

        switch (role) {
        case Qt::DisplayRole      : return dataForDisplayRole(it, f);
        case BaseDisplayRole      : return dataForDisplayRole(differenceBaseItem(it), f);
        case Qt::DecorationRole   : if (f == Picture) return it->image(); break;
        case Qt::TextAlignmentRole: return headerData(index.column(), Qt::Horizontal, role);
        case Qt::EditRole         : return dataForEditRole(it, f);
        case BaseEditRole         : return dataForEditRole(differenceBaseItem(it), f);
        case FilterRole           : return dataForFilterRole(it, f);
        case ItemPointerRole      : return QVariant::fromValue(it);
        case BaseItemPointerRole  : return QVariant::fromValue(differenceBaseItem(it));
        case ErrorFlagsRole       : return itemFlags(it).first;
        case DifferenceFlagsRole  : return itemFlags(it).second;
        }
    }
    return QVariant();
}

QVariant Document::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return { };

    static constexpr int left   = Qt::AlignLeft    | Qt::AlignVCenter;
    static constexpr int center = Qt::AlignHCenter | Qt::AlignVCenter;
    static constexpr int right  = Qt::AlignRight   | Qt::AlignVCenter;

    // column -> { default width, alignment, title }
    static QHash<int, std::tuple<int, int, const char *>> columnData = {
        { Index,        {  3,  right, QT_TR_NOOP("Index") } },
        { Status,       {  6, center, QT_TR_NOOP("Status") } },
        { Picture,      {-80, center, QT_TR_NOOP("Image") } },
        { PartNo,       { 10,   left, QT_TR_NOOP("Part #") } },
        { Description,  { 28,   left, QT_TR_NOOP("Description") } },
        { Comments,     {  8,   left, QT_TR_NOOP("Comments") } },
        { Remarks,      {  8,   left, QT_TR_NOOP("Remarks") } },
        { QuantityOrig, {  5,  right, QT_TR_NOOP("Qty.Orig") } },
        { QuantityDiff, {  5,  right, QT_TR_NOOP("Qty.Diff") } },
        { Quantity,     {  5,  right, QT_TR_NOOP("Qty.") } },
        { Bulk,         {  5,  right, QT_TR_NOOP("Bulk") } },
        { PriceOrig,    {  8,  right, QT_TR_NOOP("Pr.Orig") } },
        { PriceDiff,    {  8,  right, QT_TR_NOOP("Pr.Diff") } },
        { Cost,         {  8,  right, QT_TR_NOOP("Cost") } },
        { Price,        {  8,  right, QT_TR_NOOP("Price") } },
        { Total,        {  8,  right, QT_TR_NOOP("Total") } },
        { Sale,         {  5,  right, QT_TR_NOOP("Sale") } },
        { Condition,    {  5, center, QT_TR_NOOP("Cond.") } },
        { Color,        { 15,   left, QT_TR_NOOP("Color") } },
        { Category,     { 12,   left, QT_TR_NOOP("Category") } },
        { ItemType,     { 12,   left, QT_TR_NOOP("Item Type") } },
        { TierQ1,       {  5,  right, QT_TR_NOOP("Tier Q1") } },
        { TierP1,       {  8,  right, QT_TR_NOOP("Tier P1") } },
        { TierQ2,       {  5,  right, QT_TR_NOOP("Tier Q2") } },
        { TierP2,       {  8,  right, QT_TR_NOOP("Tier P2") } },
        { TierQ3,       {  5,  right, QT_TR_NOOP("Tier Q3") } },
        { TierP3,       {  8,  right, QT_TR_NOOP("Tier P3") } },
        { LotId,        {  8,  right, QT_TR_NOOP("Lot Id") } },
        { Retain,       {  8, center, QT_TR_NOOP("Retain") } },
        { Stockroom,    {  8, center, QT_TR_NOOP("Stockroom") } },
        { Reserved,     {  8,   left, QT_TR_NOOP("Reserved") } },
        { Weight,       {  8,  right, QT_TR_NOOP("Weight") } },
        { YearReleased, {  5,  right, QT_TR_NOOP("Year") } },
    };
    switch (role) {
    case HeaderDefaultWidthRole: return std::get<0>(columnData.value(section));
    case Qt::TextAlignmentRole : return std::get<1>(columnData.value(section));
    case Qt::DisplayRole       : return tr(std::get<2>(columnData.value(section)));
    default                    : return { };
    }
}

QVariant Document::dataForEditRole(const Item *it, Field f) const
{
    if (!it)
        return { };

    switch (f) {
    case Status      : return QVariant::fromValue(it->status());
    case Picture     :
    case Description : return QVariant::fromValue(it->item());
    case PartNo      : return it->itemId();
    case Condition   : return QVariant::fromValue(it->condition());
    case Color       : return QVariant::fromValue(it->color());
    case QuantityDiff:  {
        auto base = differenceBaseItem(it);
        return base ? it->quantity() - base->quantity() : 0;
    }
    case Quantity    : return it->quantity();
    case PriceDiff   : {
        auto base = differenceBaseItem(it);
        return base ? it->price() - base->price() : 0;
    }
    case Price       : return it->price();
    case Cost        : return it->cost();
    case Bulk        : return it->bulkQuantity();
    case Sale        : return it->sale();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case TierQ1      : return it->tierQuantity(0);
    case TierQ2      : return it->tierQuantity(1);
    case TierQ3      : return it->tierQuantity(2);
    case TierP1      : return it->tierPrice(0);
    case TierP2      : return it->tierPrice(1);
    case TierP3      : return it->tierPrice(2);
    case Retain      : return it->retain();
    case Stockroom   : return QVariant::fromValue(it->stockroom());
    case Reserved    : return it->reserved();
    case Weight      : return it->totalWeight();
    default          : return { };
    }
}

QVariant Document::dataForDisplayRole(const BrickLink::InvItem *it, Field f) const
{
    switch (f) {
    case Index       :
        if (m_fakeIndexes.isEmpty()) {
            return QString::number(m_items.indexOf(const_cast<BrickLink::InvItem *>(it)) + 1);
        } else {
            auto fi = m_fakeIndexes.at(m_items.indexOf(const_cast<BrickLink::InvItem *>(it)));
            return fi >= 0 ? QString::number(fi + 1) : QString::fromLatin1("+");
        }
    case LotId       : return it->lotId();
    case Description : return it->itemName();
    case Total       : return it->total();
    case Color       : return it->colorName();
    case Category    : return it->categoryName();
    case ItemType    : return it->itemTypeName();

    case PriceOrig   : {
        auto base = differenceBaseItem(it);
        return base ? base->price() : 0;
    }
    case QuantityOrig: {
        auto base = differenceBaseItem(it);
        return base ? base->quantity() : 0;
    }
    case YearReleased: return it->itemYearReleased();
    default          : return dataForEditRole(it, f);
    }
}

QVariant Document::dataForFilterRole(const Item *it, Field f) const
{
    switch (f) {
    case Picture:
        return it->itemId();

    case Status:
        switch (it->status()) {
        case BrickLink::Status::Include: return tr("I", "Filter>Status>Include");
        case BrickLink::Status::Extra  : return tr("X", "Filter>Status>Extra");
        default:
        case BrickLink::Status::Exclude: return tr("E", "Filter>Status>Exclude");
        }
    case Condition:
        return (it->condition() == BrickLink::Condition::New) ? tr("N", "Filter>Condition>New")
                                                              : tr("U", "Filter>Condition>Used");
    case Stockroom:
        switch (it->stockroom()) {
        case BrickLink::Stockroom::A: return QString("A");
        case BrickLink::Stockroom::B: return QString("B");
        case BrickLink::Stockroom::C: return QString("C");
        default                     : return tr("N", "Filter>Stockroom>None");
        }
    case Retain:
        return it->retain() ? tr("Y", "Filter>Retain>Yes")
                            : tr("N", "Filter>Retain>No");
    default: {
        QVariant v = dataForEditRole(it, f);
        if (v.isNull() || (v.userType() >= QMetaType::User))
            v = dataForDisplayRole(it, f);
        return v;
    }
    }
}


void Document::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item())
        return;

    for (const auto *it : qAsConst(m_items)) {
        if ((pic->item() == it->item()) && (pic->color() == it->color())) {
            QModelIndex idx = index(const_cast<BrickLink::InvItem *>(it), Picture);
            emitDataChanged(idx, idx);
        }
    }
}

bool Document::isSorted() const
{
    return m_isSorted;
}

int Document::sortColumn() const
{
    return m_sortColumn;
}

Qt::SortOrder Document::sortOrder() const
{
    return m_sortOrder;
}

void Document::sort(int column, Qt::SortOrder order)
{
    if ((column == -1) || ((column == m_sortColumn) && (order == m_sortOrder)))
        return;

    if (m_undo && !m_nextSortFilterIsDirect) {
        m_undo->push(new SortCmd(this, column, order));
    } else {
        bool dummy1;
        BrickLink::InvItemList dummy2;
        sortDirect(column, order, dummy1, dummy2);
        m_nextSortFilterIsDirect = false;
    }
}

bool Document::isFiltered() const
{
    return m_isFiltered;
}

QString Document::filter() const
{
    return m_filterString;
}

void Document::setFilter(const QString &filter)
{
    if (filter == m_filterString)
        return;
    auto filterList = m_filterParser->parse(filter);
    if (filterList == m_filterList)
        return;

    if (m_undo && !m_nextSortFilterIsDirect) {
        m_undo->push(new FilterCmd(this, filter, filterList));
    } else {
        bool dummy1;
        BrickLink::InvItemList dummy2;
        filterDirect(filter, filterList, dummy1, dummy2);
        m_nextSortFilterIsDirect = false;
    }
}

void Document::nextSortFilterIsDirect()
{
    m_nextSortFilterIsDirect = true;
}

void Document::sortDirect(int column, Qt::SortOrder order, bool &sorted,
                          BrickLink::InvItemList &unsortedItems)
{
    bool emitSortOrderChanged = (order != m_sortOrder);
    bool emitSortColumnChanged = (column != m_sortColumn);
    bool wasSorted = isSorted();

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    m_sortColumn = column;
    m_sortOrder = order;

    if (!unsortedItems.isEmpty()) {
        m_isSorted = sorted;
        m_sortedItems = unsortedItems;
        unsortedItems.clear();

    } else {
        unsortedItems = m_sortedItems;
        sorted = m_isSorted;
        m_isSorted = true;
        m_sortedItems = m_items;

        if (column >= 0) {
            qParallelSort(m_sortedItems.begin(), m_sortedItems.end(),
                          [column, this](const auto *item1, const auto *item2) {
                return compare(item1, item2, column) < 0;
            });
            if (order == Qt::DescendingOrder)
                std::reverse(m_sortedItems.begin(), m_sortedItems.end());

            // c++17 alternatives, but not supported everywhere yet:
            //std::stable_sort(std::execution::par_unseq, sorted.begin(), sorted.end(), sorter);
            //std::sort(std::execution::par_unseq, sorted.begin(), sorted.end(), sorter);
        }
    }

    // we were filtered before, but we don't want to refilter: the solution is to
    // keep the old filtered items, but use the order from m_sortedItems
    if (!m_filteredItems.isEmpty()) {
        m_filteredItems = QtConcurrent::blockingFiltered(m_sortedItems, [this](auto *item) {
            return m_filteredItems.contains(item);
        });
    } else {
        m_filteredItems = m_sortedItems;
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(item(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    if (emitSortColumnChanged)
        emit sortColumnChanged(column);
    if (emitSortOrderChanged)
        emit sortOrderChanged(order);

    if (isSorted() != wasSorted)
        emit isSortedChanged(isSorted());
}

void Document::filterDirect(const QString &filterString, const QVector<Filter> &filterList,
                            bool &filtered, BrickLink::InvItemList &unfilteredItems)
{
    bool emitFilterChanged = (filterList != m_filterList);
    bool wasFiltered = isFiltered();

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    m_filterString = filterString;
    m_filterList = filterList;

    if (!unfilteredItems.isEmpty()) {
        m_isFiltered = filtered;
        m_filteredItems = unfilteredItems;
        unfilteredItems.clear();

    } else {
        unfilteredItems = m_filteredItems;
        filtered = m_isFiltered;
        m_isFiltered = true;
        m_filteredItems = m_sortedItems;

        if (!filterList.isEmpty()) {
            m_filteredItems = QtConcurrent::blockingFiltered(m_sortedItems, [this](auto *item) {
                return filterAcceptsItem(item);
            });
        }
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(item(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    if (emitFilterChanged)
        emit filterChanged(filterString);

    if (isFiltered() != wasFiltered)
        emit isFilteredChanged(isFiltered());
}

QByteArray Document::saveSortFilterState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SFST") << qint32(2);

    ds << qint8(m_sortColumn) << qint8(m_sortOrder) << m_filterString;

    ds << qint32(m_sortedItems.size());
    for (int i = 0; i < m_sortedItems.size(); ++i) {
        auto *item = m_sortedItems.at(i);
        qint32 row = m_items.indexOf(item);
        bool visible = m_filteredItems.contains(item);

        ds << (visible ? row : (-row - 1)); // can't have -0
    }

    return ba;
}

bool Document::restoreSortFilterState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SFST") || (version != 2))
        return false;
    qint8 sortColumn, sortOrder;
    QString filterString;
    qint32 viewSize;

    ds >> sortColumn >> sortOrder >> filterString >> viewSize;
    if ((ds.status() != QDataStream::Ok) || (viewSize != m_items.size()))
        return false;

    BrickLink::InvItemList sortedItems;
    BrickLink::InvItemList filteredItems;
    int itemsSize = m_items.size();
    while (viewSize--) {
        qint32 pos;
        ds >> pos;
        if ((pos < -itemsSize) || (pos >= itemsSize))
            return false;
        bool visible = (pos >= 0);
        pos = visible ? pos : (-pos - 1);
        auto *item = m_items.at(pos);
        sortedItems << item;
        if (visible)
            filteredItems << item;
    }

    if (ds.status() != QDataStream::Ok)
        return false;

    auto filterList = m_filterParser->parse(filterString);

    bool willBeSorted = true;
    sortDirect(sortColumn, Qt::SortOrder(sortOrder), willBeSorted, sortedItems);
    bool willBeFiltered = true;
    filterDirect(filterString, filterList, willBeFiltered, filteredItems);
    return true;
}

QString Document::filterToolTip() const
{
    return m_filterParser->toolTip();
}

void Document::reSort()
{
    if (!isSorted())
        m_undo->push(new SortCmd(this, m_sortColumn, m_sortOrder));
}

void Document::reFilter()
{
    if (!isFiltered())
        m_undo->push(new FilterCmd(this, m_filterString, m_filterList));
}


bool Document::filterAcceptsItem(const BrickLink::InvItem *item) const
{
    if (!item)
        return false;
    else if (m_filterList.isEmpty())
        return true;

    bool result = false;
    Filter::Combination nextcomb = Filter::Or;

    for (const Filter &f : m_filterList) {
        int firstcol = f.field();
        int lastcol = firstcol;
        if (firstcol < 0) {
            firstcol = 0;
            lastcol = columnCount() - 1;
        }

        bool localresult = false;
        for (int col = firstcol; col <= lastcol && !localresult; ++col) {
            QVariant v = dataForFilterRole(item, static_cast<Field>(col));
            localresult = f.matches(v);
        }
        if (nextcomb == Filter::And)
            result = result && localresult;
        else
            result = result || localresult;

        nextcomb = f.combination();
    }
    return result;
}

bool Document::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QAbstractTableModel::event(e);
}

void Document::languageChange()
{
    m_filterParser->setStandardCombinationTokens(Filter::And | Filter::Or);
    m_filterParser->setStandardComparisonTokens(Filter::Matches | Filter::DoesNotMatch |
                                          Filter::Is | Filter::IsNot |
                                          Filter::Less | Filter::LessEqual |
                                          Filter::Greater | Filter::GreaterEqual |
                                          Filter::StartsWith | Filter::DoesNotStartWith |
                                          Filter::EndsWith | Filter::DoesNotEndWith);

    QMultiMap<int, QString> fields;
    QString str;
    for (int i = 0; i < columnCount(); ++i) {
        str = headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!str.isEmpty())
            fields.insert(i, str);
    }
    fields.insert(-1, tr("Any"));

    m_filterParser->setFieldTokens(fields);
}

static inline int boolCompare(bool b1, bool b2)
{
    return (b1 ? 1 : 0) - (b2 ? 1 : 0);
}

static inline int uintCompare(uint u1, uint u2)
{
    return (u1 == u2) ? 0 : ((u1 < u2) ? -1 : 1);
}

static inline int doubleCompare(double d1, double d2)
{
    return qFuzzyCompare(d1, d2) ? 0 : ((d1 < d2) ? -1 : 1);
}

int Document::compare(const BrickLink::InvItem *i1, const BrickLink::InvItem *i2, int sortColumn) const
{
    switch (sortColumn) {
    case Document::Index       : {
        return m_items.indexOf(const_cast<BrickLink::InvItem *>(i1)) - m_items.indexOf(const_cast<BrickLink::InvItem *>(i2));
    }
    case Document::Status      : {
        if (i1->counterPart() != i2->counterPart())
            return boolCompare(i1->counterPart(), i2->counterPart());
        else if (i1->alternateId() != i2->alternateId())
            return uintCompare(i1->alternateId(), i2->alternateId());
        else if (i1->alternate() != i2->alternate())
            return boolCompare(i1->alternate(), i2->alternate());
        else
            return int(i1->status()) - int(i2->status());
    }
    case Document::Picture     :
    case Document::PartNo      : return Utility::naturalCompare(i1->itemId(),
                                                                i2->itemId());
    case Document::Description : return Utility::naturalCompare(i1->itemName(),
                                                                i2->itemName());

    case Document::Color       : return i1->colorName().localeAwareCompare(i2->colorName());
    case Document::Category    : return i1->categoryName().localeAwareCompare(i2->categoryName());
    case Document::ItemType    : return i1->itemTypeName().localeAwareCompare(i2->itemTypeName());

    case Document::Comments    : return i1->comments().localeAwareCompare(i2->comments());
    case Document::Remarks     : return i1->remarks().localeAwareCompare(i2->remarks());

    case Document::LotId       : return uintCompare(i1->lotId(), i2->lotId());
    case Document::Quantity    : return i1->quantity() - i2->quantity();
    case Document::Bulk        : return i1->bulkQuantity() - i2->bulkQuantity();
    case Document::Price       : return doubleCompare(i1->price(), i2->price());
    case Document::Total       : return doubleCompare(i1->total(), i2->total());
    case Document::Sale        : return i1->sale() - i2->sale();
    case Document::Condition   : {
        if (i1->condition() == i2->condition())
            return int(i1->subCondition()) - int(i2->subCondition());
        else
            return int(i1->condition()) - int(i2->condition());
    }
    case Document::TierQ1      : return i1->tierQuantity(0) - i2->tierQuantity(0);
    case Document::TierQ2      : return i1->tierQuantity(1) - i2->tierQuantity(1);
    case Document::TierQ3      : return i1->tierQuantity(2) - i2->tierQuantity(2);
    case Document::TierP1      : return doubleCompare(i1->tierPrice(0), i2->tierPrice(0));
    case Document::TierP2      : return doubleCompare(i1->tierPrice(1), i2->tierPrice(1));
    case Document::TierP3      : return doubleCompare(i1->tierPrice(2), i2->tierPrice(2));
    case Document::Retain      : return boolCompare(i1->retain(), i2->retain());
    case Document::Stockroom   : return int(i1->stockroom()) - int(i2->stockroom());
    case Document::Reserved    : return i1->reserved().compare(i2->reserved());
    case Document::Weight      : return doubleCompare(i1->totalWeight(), i2->totalWeight());
    case Document::YearReleased: return i1->itemYearReleased() - i2->itemYearReleased();
    case Document::PriceOrig   : {
        auto base1 = differenceBaseItem(i1);
        auto base2 = differenceBaseItem(i2);
        return doubleCompare(base1 ? base1->price() : 0, base2 ? base2->price() : 0);
    }
    case Document::PriceDiff   : {
        auto base1 = differenceBaseItem(i1);
        auto base2 = differenceBaseItem(i2);
        return doubleCompare(base1 ? i1->price() - base1->price() : 0,
                             base2 ? i2->price() - base2->price() : 0);
    }
    case Document::QuantityOrig: {
        auto base1 = differenceBaseItem(i1);
        auto base2 = differenceBaseItem(i2);
        return (base1 ? base1->quantity() : 0) - (base2 ? base2->quantity() : 0);
    }

    case Document::QuantityDiff:  {
        auto base1 = differenceBaseItem(i1);
        auto base2 = differenceBaseItem(i2);
        return (base1 ? i1->quantity() - base1->quantity() : 0)
                - (base2 ? i2->quantity() - base2->quantity() : 0);
    }
    }
    return 0;
}

BrickLink::InvItemList Document::sortItemList(const BrickLink::InvItemList &list) const
{
    BrickLink::InvItemList result(list);
    qParallelSort(result.begin(), result.end(), [this](const auto &i1, const auto &i2) {
        return index(i1).row() < index(i2).row();
    });
    return result;
}


#include "moc_document.cpp"
