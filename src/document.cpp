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
    template <typename Result> static Result returnType(Result (Lot::*)() const);
    typedef decltype(returnType(G)) R;

    static void copy(const Lot &from, Lot &to)
    {
        (to.*S)((from.*G)());
    }

    static bool merge(const Lot &from, Lot &to,
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
    mergeInternal(const Lot &from, Lot &to, Document::MergeMode mergeMode,
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
    mergeInternal(const Lot &from, Lot &to, Document::MergeMode mergeMode,
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
    mergeInternal(const Lot &from, Lot &to, Document::MergeMode mergeMode,
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
                           const QVector<int> &filteredPositions, const LotList &lots)
    : QUndoCommand(genDesc(t == Add, qMax(lots.count(), positions.count())))
    , m_doc(doc)
    , m_positions(positions)
    , m_sortedPositions(sortedPositions)
    , m_filteredPositions(filteredPositions)
    , m_lots(lots)
    , m_type(t)
{
    // for add: specify lots and optionally also positions
    // for remove: specify lots only
}

AddRemoveCmd::~AddRemoveCmd()
{
    if (m_type == Add) {
        if (m_doc) {
            for (const auto lot : qAsConst(m_lots))
                m_doc->m_differenceBase.remove(lot);
        }
        qDeleteAll(m_lots);
    }
}

int AddRemoveCmd::id() const
{
    return CID_AddRemove;
}

void AddRemoveCmd::redo()
{
    if (m_type == Add) {
        // Document::insertLotsDirect() adds all m_lots at the positions given in
        // m_*positions (or append them to the document in case m_*positions is empty)
        m_doc->insertLotsDirect(m_lots, m_positions, m_sortedPositions, m_filteredPositions);
        m_positions.clear();
        m_sortedPositions.clear();
        m_filteredPositions.clear();
        m_type = Remove;
    }
    else {
        // Document::removeLotsDirect() removes all m_lots and records the positions
        // in m_*positions
        m_doc->removeLotsDirect(m_lots, m_positions, m_sortedPositions, m_filteredPositions);
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

ChangeCmd::ChangeCmd(Document *doc, const std::vector<std::pair<Lot *, Lot>> &changes, Document::Field hint)
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
            .arg((m_hint < Document::FieldCount) ? m_doc->headerData(m_hint, Qt::Horizontal).toString()
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
                                           std::make_pair(change.first, Lot()),
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
    m_doc->changeLotsDirect(m_changes);
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


Document::Statistics::Statistics(const Document *doc, const LotList &list, bool ignoreExcluded)
{
    m_lots = 0;
    m_items = 0;
    m_val = m_minval = m_cost = 0.;
    m_weight = .0;
    m_errors = 0;
    m_differences = 0;
    m_incomplete = 0;
    bool weight_missing = false;

    for (const Lot *lot : list) {
        if (ignoreExcluded && (lot->status() == BrickLink::Status::Exclude))
            continue;

        ++m_lots;

        int qty = lot->quantity();
        double price = lot->price();

        m_val += (qty * price);
        m_cost += (qty * lot->cost());

        for (int i = 0; i < 3; i++) {
            if (lot->tierQuantity(i) && !qFuzzyIsNull(lot->tierPrice(i)))
                price = lot->tierPrice(i);
        }
        m_minval += (qty * price * (1.0 - double(lot->sale()) / 100.0));
        m_items += qty;

        if (lot->totalWeight() > 0)
            m_weight += lot->totalWeight();
        else
            weight_missing = true;

        auto flags = doc->lotFlags(lot);
        if (flags.first)
            m_errors += qPopulationCount(flags.first);
        if (flags.second)
            m_differences += qPopulationCount(flags.second);

        if (lot->isIncomplete())
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

Document *Document::createTemporary(const LotList &list, const QVector<int> &fakeIndexes)
{
    auto *doc = new Document(1 /*dummy*/);
    LotList lots;

    // the caller owns the items, so we have to copy here
    for (const Lot *lot : list)
        lots.append(new Lot(*lot));

    doc->setLotsDirect(lots);
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
        } else if (m_undo->cleanIndex() < 0) {
            m_visuallyClean = false;
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
    setLotsDirect(bsx.lots);

    if (!bsx.currencyCode.isEmpty()) {
        if (bsx.currencyCode == "$$$"_l1) // legacy USD
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
    qDeleteAll(m_lots);
    delete m_undo;

    s_documents.removeAll(this);
}

const QVector<Document *> &Document::allDocuments()
{
    return s_documents;
}

const LotList &Document::lots() const
{
    return m_lots;
}

const LotList &Document::sortedLots() const
{
    return m_sortedLots;
}

Document::Statistics Document::statistics(const LotList &list, bool ignoreExcluded) const
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
    removeLots(m_lots);
    return true;
}

void Document::appendLot(Lot *lot)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, { }, { }, { }, { lot }));
}

void Document::appendLots(const LotList &lots)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, { }, { }, { }, lots));
}

void Document::insertLotsAfter(const Lot *afterLot, const LotList &lots)
{
    if (lots.isEmpty())
        return;

    int afterPos = m_lots.indexOf(const_cast<Lot *>(afterLot)) + 1;
    int afterSortedPos = m_sortedLots.indexOf(const_cast<Lot *>(afterLot)) + 1;
    int afterFilteredPos = m_filteredLots.indexOf(const_cast<Lot *>(afterLot)) + 1;

    Q_ASSERT((afterPos > 0) && (afterSortedPos > 0));
    if (afterFilteredPos == 0)
        afterFilteredPos = m_filteredLots.size();

    QVector<int> positions(lots.size());
    std::iota(positions.begin(), positions.end(), afterPos);
    QVector<int> sortedPositions(lots.size());
    std::iota(sortedPositions.begin(), sortedPositions.end(), afterSortedPos);
    QVector<int> filteredPositions(lots.size());
    std::iota(filteredPositions.begin(), filteredPositions.end(), afterFilteredPos);

    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, sortedPositions,
                                  filteredPositions, lots));
}

void Document::removeLot(Lot *lot)
{
    return removeLots({ lot });
}

void Document::removeLots(const LotList &lots)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, { }, { }, { }, lots));
}

void Document::changeLot(Lot *lot, const Lot &value, Document::Field hint)
{
    m_undo->push(new ChangeCmd(this, {{ lot, value }}, hint));
}

void Document::changeLots(const std::vector<std::pair<Lot *, Lot>> &changes, Field hint)
{
    if (!changes.empty())
        m_undo->push(new ChangeCmd(this, changes, hint));
}

void Document::setLotsDirect(const LotList &lots)
{
    if (lots.isEmpty())
        return;
    QVector<int> posDummy, sortedPosDummy, filteredPosDummy;
    insertLotsDirect(lots, posDummy, sortedPosDummy, filteredPosDummy);
}

void Document::insertLotsDirect(const LotList &lots, QVector<int> &positions,
                                 QVector<int> &sortedPositions, QVector<int> &filteredPositions)
{
    auto pos = positions.constBegin();
    auto sortedPos = sortedPositions.constBegin();
    auto filteredPos = filteredPositions.constBegin();
    bool isAppend = positions.isEmpty();

    Q_ASSERT((positions.size() == sortedPositions.size())
             && (positions.size() == filteredPositions.size()));
    Q_ASSERT(isAppend != (positions.size() == lots.size()));

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    for (Lot *lot : qAsConst(lots)) {
        if (!isAppend) {
            m_lots.insert(*pos++, lot);
            m_sortedLots.insert(*sortedPos++, lot);
            m_filteredLots.insert(*filteredPos++, lot);
        } else {
            m_lots.append(lot);
            m_sortedLots.append(lot);
            m_filteredLots.append(lot);
        }

        // this is really a new lot, not just a redo - start with no differences
        if (!m_differenceBase.contains(lot))
            m_differenceBase.insert(lot, *lot);

        updateLotFlags(lot);
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(lot(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    emit lotCountChanged(m_lots.count());
    emitStatisticsChanged();

    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void Document::removeLotsDirect(const LotList &lots, QVector<int> &positions,
                                 QVector<int> &sortedPositions, QVector<int> &filteredPositions)
{
    positions.resize(lots.count());
    sortedPositions.resize(lots.count());
    filteredPositions.resize(lots.count());

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    for (int i = lots.count() - 1; i >= 0; --i) {
        Lot *lot = lots.at(i);
        int idx = m_lots.indexOf(lot);
        int sortIdx = m_sortedLots.indexOf(lot);
        int filterIdx = m_filteredLots.indexOf(lot);
        Q_ASSERT(idx >= 0 && sortIdx >= 0);
        positions[i] = idx;
        sortedPositions[i] = sortIdx;
        filteredPositions[i] = filterIdx;
        m_lots.removeAt(idx);
        m_sortedLots.removeAt(sortIdx);
        m_filteredLots.removeAt(filterIdx);
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(lot(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    emit lotCountChanged(m_lots.count());
    emitStatisticsChanged();

    //TODO: we should remember and re-apply the isSorted/isFiltered state
    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void Document::changeLotsDirect(std::vector<std::pair<Lot *, Lot>> &changes)
{
    Q_ASSERT(!changes.empty());

    for (auto &change : changes) {
        Lot *lot = change.first;
        std::swap(*lot, change.second);

        QModelIndex idx1 = index(lot, 0);
        QModelIndex idx2 = idx1.siblingAtColumn(columnCount() - 1);
        updateLotFlags(lot);
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
            prices = new double[5 * m_lots.count()];

        for (int i = 0; i < m_lots.count(); ++i) {
            Lot *lot = m_lots[i];
            if (createPrices) {
                prices[i * 5] = lot->cost();
                prices[i * 5 + 1] = lot->price();
                prices[i * 5 + 2] = lot->tierPrice(0);
                prices[i * 5 + 3] = lot->tierPrice(1);
                prices[i * 5 + 4] = lot->tierPrice(2);

                lot->setCost(prices[i * 5] * crate);
                lot->setPrice(prices[i * 5 + 1] * crate);
                lot->setTierPrice(0, prices[i * 5 + 2] * crate);
                lot->setTierPrice(1, prices[i * 5 + 3] * crate);
                lot->setTierPrice(2, prices[i * 5 + 4] * crate);
            } else {
                lot->setCost(prices[i * 5]);
                lot->setPrice(prices[i * 5 + 1]);
                lot->setTierPrice(0, prices[i * 5 + 2]);
                lot->setTierPrice(1, prices[i * 5 + 3]);
                lot->setTierPrice(2, prices[i * 5 + 4]);
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

void Document::updateLotFlags(const Lot *lot)
{
    quint64 errors = 0;
    quint64 updated = 0;

    if (!lot->item())
        errors |= (1ULL << PartNo);
    if (lot->price() <= 0)
        errors |= (1ULL << Price);
    if (lot->quantity() <= 0)
        errors |= (1ULL << Quantity);
    if (!lot->color() || (lot->itemType() && ((lot->color()->id() != 0) && !lot->itemType()->hasColors())))
        errors |= (1ULL << Color);
    if (lot->tierQuantity(0) && ((lot->tierPrice(0) <= 0) || (lot->tierPrice(0) >= lot->price())))
        errors |= (1ULL << TierP1);
    if (lot->tierQuantity(1) && ((lot->tierPrice(1) <= 0) || (lot->tierPrice(1) >= lot->tierPrice(0))))
        errors |= (1ULL << TierP2);
    if (lot->tierQuantity(1) && (lot->tierQuantity(1) <= lot->tierQuantity(0)))
        errors |= (1ULL << TierQ2);
    if (lot->tierQuantity(2) && ((lot->tierPrice(2) <= 0) || (lot->tierPrice(2) >= lot->tierPrice(1))))
        errors |= (1ULL << TierP3);
    if (lot->tierQuantity(2) && (lot->tierQuantity(2) <= lot->tierQuantity(1)))
        errors |= (1ULL << TierQ3);

    if (auto base = differenceBaseLot(lot)) {
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
                | (1ULL << YearReleased)
                | (1ULL << Marker);

        for (Field f = Index; f != FieldCount; f = Field(f + 1)) {
            quint64 fmask = (1ULL << f);
            if (fmask & ignoreMask)
                continue;
            if (dataForEditRole(lot, f) != dataForEditRole(base, f))
                updated |= fmask;
        }
    }

    setLotFlags(lot, errors, updated);
}

void Document::resetDifferenceMode()
{
    m_undo->push(new ResetDifferenceModeCmd(this));
}

void Document::resetDifferenceModeDirect(QHash<const Lot *, Lot>
                                         &differenceBase) {
    std::swap(m_differenceBase, differenceBase);

    if (m_differenceBase.isEmpty()) {
        for (const auto *lot : qAsConst(m_lots))
            m_differenceBase.insert(lot, *lot);
    }

    for (const auto *lot : qAsConst(m_lots))
        updateLotFlags(lot);

    emitDataChanged();
}

const Lot *Document::differenceBaseLot(const Lot *lot) const
{
    if (!lot)
        return nullptr;

    auto it = m_differenceBase.constFind(lot);
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

bool Document::canLotsBeMerged(const Lot &lot1, const Lot &lot2)
{
    return ((&lot1 != &lot2)
            && !lot1.isIncomplete()
            && !lot2.isIncomplete()
            && (lot1.item() == lot2.item())
            && (lot1.color() == lot2.color())
            && (lot1.condition() == lot2.condition())
            && (lot1.subCondition() == lot2.subCondition()));
}

bool Document::mergeLotFields(const Lot &from, Lot &to, MergeMode defaultMerge,
                               const QHash<Field, MergeMode> &fieldMerge)
{
    if (!canLotsBeMerged(from, to))
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

    if (FieldOp<&Lot::quantity, &Lot::setQuantity>::merge(from, to, mergeModeFor(Quantity)))
        changed = true;
    if (FieldOp<&Lot::price, &Lot::setPrice>::merge(from, to, mergeModeFor(Price)))
        changed = true;
    if (FieldOp<&Lot::cost, &Lot::setCost>::merge(from, to, mergeModeFor(Cost)))
        changed = true;
    if (FieldOp<&Lot::bulkQuantity, &Lot::setBulkQuantity>::merge(from, to, mergeModeFor(Bulk)))
        changed = true;
    if (FieldOp<&Lot::sale, &Lot::setSale>::merge(from, to, mergeModeFor(Sale)))
        changed = true;
    if (FieldOp<&Lot::comments, &Lot::setComments>::merge(from, to, mergeModeFor(Comments)))
        changed = true;
    if (FieldOp<&Lot::remarks, &Lot::setRemarks>::merge(from, to, mergeModeFor(Remarks)))
        changed = true;
    if (FieldOp<&Lot::tierQuantity0, &Lot::setTierQuantity0>::merge(from, to, mergeModeFor(TierQ1)))
        changed = true;
    if (FieldOp<&Lot::tierPrice0, &Lot::setTierPrice0>::merge(from, to, mergeModeFor(TierP1)))
        changed = true;
    if (FieldOp<&Lot::tierQuantity1, &Lot::setTierQuantity1>::merge(from, to, mergeModeFor(TierQ2)))
        changed = true;
    if (FieldOp<&Lot::tierPrice1, &Lot::setTierPrice1>::merge(from, to, mergeModeFor(TierP2)))
        changed = true;
    if (FieldOp<&Lot::tierQuantity2, &Lot::setTierQuantity2>::merge(from, to, mergeModeFor(TierQ3)))
        changed = true;
    if (FieldOp<&Lot::tierPrice2, &Lot::setTierPrice2>::merge(from, to, mergeModeFor(TierP3)))
        changed = true;
    if (FieldOp<&Lot::retain, &Lot::setRetain>::merge(from, to, mergeModeFor(Retain)))
        changed = true;
    if (FieldOp<&Lot::stockroom, &Lot::setStockroom>::merge(from, to, mergeModeFor(Stockroom)))
        changed = true;
    if (FieldOp<&Lot::reserved, &Lot::setReserved>::merge(from, to, mergeModeFor(Reserved)))
        changed = true;

    //TODO: merge markers

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

QHash<const Lot *, Lot> Document::differenceBase() const
{
    return m_differenceBase;
}

void Document::updateModified()
{
    emit modificationChanged(isModified());
}

void Document::setLotFlagsMask(QPair<quint64, quint64> flagsMask)
{
    m_lotFlagsMask = flagsMask;
    emitStatisticsChanged();
    emitDataChanged();
}

QPair<quint64, quint64> Document::lotFlags(const Lot *lot) const
{
    auto flags = m_lotFlags.value(lot, { });
    flags.first &= m_lotFlagsMask.first;
    flags.second &= m_lotFlagsMask.second;
    return flags;
}

void Document::setLotFlags(const Lot *lot, quint64 errors, quint64 updated)
{
    if (!lot)
        return;

    auto oldFlags = m_lotFlags.value(lot, { });
    if (oldFlags.first != errors || oldFlags.second != updated) {
        if (errors || updated)
            m_lotFlags.insert(lot, qMakePair(errors, updated));
        else
            m_lotFlags.remove(lot);

        emit lotFlagsChanged(lot);
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
        return createIndex(row, column, m_filteredLots.at(row));
    return { };
}

Lot *Document::lot(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<Lot *>(idx.internalPointer()) : nullptr;
}

QModelIndex Document::index(const Lot *lot, int column) const
{
    int row = m_filteredLots.indexOf(const_cast<Lot *>(lot));
    if (row >= 0)
        return createIndex(row, column, const_cast<Lot *>(lot));
    return { };
}

int Document::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_filteredLots.size();
}

int Document::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : FieldCount;
}

Qt::ItemFlags Document::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return { };

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
    Lot *lotp = this->lot(index);
    Lot lot = *lotp;
    Document::Field f = static_cast<Field>(index.column());

    switch (f) {
    case Status      : lot.setStatus(value.value<BrickLink::Status>()); break;
    case Picture     :
    case Description : lot.setItem(value.value<const BrickLink::Item *>()); break;
    case PartNo      : {
        char itid = lot.itemType() ? lot.itemType()->id() : 'P';
        if (auto newItem = BrickLink::core()->item(itid, value.toString().toLatin1()))
            lot.setItem(newItem);
        break;
    }
    case Condition   : lot.setCondition(value.value<BrickLink::Condition>()); break;
    case Color       : lot.setColor(value.value<const BrickLink::Color *>()); break;
    case QuantityDiff: {
        if (auto base = differenceBaseLot(lotp))
            lot.setQuantity(base->quantity() + value.toInt());
        break;
    }
    case Quantity    : lot.setQuantity(value.toInt()); break;
    case PriceDiff   :  {
        if (auto base = differenceBaseLot(lotp))
            lot.setPrice(base->price() + value.toDouble());
        break;
    }
    case Price       : lot.setPrice(value.toDouble()); break;
    case Cost        : lot.setCost(value.toDouble()); break;
    case Bulk        : lot.setBulkQuantity(value.toInt()); break;
    case Sale        : lot.setSale(value.toInt()); break;
    case Comments    : lot.setComments(value.toString()); break;
    case Remarks     : lot.setRemarks(value.toString()); break;
    case TierQ1      : lot.setTierQuantity(0, value.toInt()); break;
    case TierQ2      : lot.setTierQuantity(1, value.toInt()); break;
    case TierQ3      : lot.setTierQuantity(2, value.toInt()); break;
    case TierP1      : lot.setTierPrice(0, value.toDouble()); break;
    case TierP2      : lot.setTierPrice(1, value.toDouble()); break;
    case TierP3      : lot.setTierPrice(2, value.toDouble()); break;
    case Retain      : lot.setRetain(value.toBool()); break;
    case Stockroom   : lot.setStockroom(value.value<BrickLink::Stockroom>()); break;
    case Reserved    : lot.setReserved(value.toString()); break;
    case Weight      : lot.setTotalWeight(value.toDouble()); break;
    case Marker      : lot.setMarkerText(value.toString()); break;
    default          : break;
    }
    if (lot != *lotp) {
        changeLot(lotp, lot, f);
        return true;
    }
    return false;
}


QVariant Document::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        const Lot *lot = this->lot(index);
        auto f = static_cast<Field>(index.column());

        switch (role) {
        case Qt::DisplayRole      : return dataForDisplayRole(lot, f);
        case BaseDisplayRole      : return dataForDisplayRole(differenceBaseLot(lot), f);
        case Qt::TextAlignmentRole: return headerData(index.column(), Qt::Horizontal, role);
        case Qt::EditRole         : return dataForEditRole(lot, f);
        case BaseEditRole         : return dataForEditRole(differenceBaseLot(lot), f);
        case FilterRole           : return dataForFilterRole(lot, f);
        case LotPointerRole       : return QVariant::fromValue(lot);
        case BaseLotPointerRole   : return QVariant::fromValue(differenceBaseLot(lot));
        case ErrorFlagsRole       : return lotFlags(lot).first;
        case DifferenceFlagsRole  : return lotFlags(lot).second;
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
        { Marker,       {  8,   left, QT_TR_NOOP("Marker") } },
    };
    switch (role) {
    case HeaderDefaultWidthRole: return std::get<0>(columnData.value(section));
    case Qt::TextAlignmentRole : return std::get<1>(columnData.value(section));
    case Qt::DisplayRole       : return tr(std::get<2>(columnData.value(section)));
    default                    : return { };
    }
}

QVariant Document::dataForEditRole(const Lot *lot, Field f) const
{
    if (!lot)
        return { };

    switch (f) {
    case Status      : return QVariant::fromValue(lot->status());
    case Picture     :
    case Description : return QVariant::fromValue(lot->item());
    case PartNo      : return lot->itemId();
    case Condition   : return QVariant::fromValue(lot->condition());
    case Color       : return QVariant::fromValue(lot->color());
    case QuantityDiff:  {
        auto base = differenceBaseLot(lot);
        return base ? lot->quantity() - base->quantity() : 0;
    }
    case Quantity    : return lot->quantity();
    case PriceDiff   : {
        auto base = differenceBaseLot(lot);
        return base ? lot->price() - base->price() : 0;
    }
    case Price       : return lot->price();
    case Cost        : return lot->cost();
    case Bulk        : return lot->bulkQuantity();
    case Sale        : return lot->sale();
    case Comments    : return lot->comments();
    case Remarks     : return lot->remarks();
    case TierQ1      : return lot->tierQuantity(0);
    case TierQ2      : return lot->tierQuantity(1);
    case TierQ3      : return lot->tierQuantity(2);
    case TierP1      : return lot->tierPrice(0);
    case TierP2      : return lot->tierPrice(1);
    case TierP3      : return lot->tierPrice(2);
    case Retain      : return lot->retain();
    case Stockroom   : return QVariant::fromValue(lot->stockroom());
    case Reserved    : return lot->reserved();
    case Weight      : return lot->totalWeight();
    case Marker      : return lot->markerText();
    default          : return { };
    }
}

QVariant Document::dataForDisplayRole(const Lot *lot, Field f) const
{
    switch (f) {
    case Index       :
        if (m_fakeIndexes.isEmpty()) {
            return QString::number(m_lots.indexOf(const_cast<Lot *>(lot)) + 1);
        } else {
            auto fi = m_fakeIndexes.at(m_lots.indexOf(const_cast<Lot *>(lot)));
            return fi >= 0 ? QString::number(fi + 1) : QString::fromLatin1("+");
        }
    case LotId       : return lot->lotId();
    case Description : return lot->itemName();
    case Total       : return lot->total();
    case Color       : return lot->colorName();
    case Category    : return lot->categoryName();
    case ItemType    : return lot->itemTypeName();

    case PriceOrig   : {
        auto base = differenceBaseLot(lot);
        return base ? base->price() : 0;
    }
    case QuantityOrig: {
        auto base = differenceBaseLot(lot);
        return base ? base->quantity() : 0;
    }
    case YearReleased: return lot->itemYearReleased();
    default          : return dataForEditRole(lot, f);
    }
}

QVariant Document::dataForFilterRole(const Lot *lot, Field f) const
{
    switch (f) {
    case Picture:
        return lot->lotId();

    case Status:
        switch (lot->status()) {
        case BrickLink::Status::Include: return tr("I", "Filter>Status>Include");
        case BrickLink::Status::Extra  : return tr("X", "Filter>Status>Extra");
        default:
        case BrickLink::Status::Exclude: return tr("E", "Filter>Status>Exclude");
        }
    case Condition:
        return (lot->condition() == BrickLink::Condition::New) ? tr("N", "Filter>Condition>New")
                                                              : tr("U", "Filter>Condition>Used");
    case Stockroom:
        switch (lot->stockroom()) {
        case BrickLink::Stockroom::A: return QStringLiteral("A");
        case BrickLink::Stockroom::B: return QStringLiteral("B");
        case BrickLink::Stockroom::C: return QStringLiteral("C");
        default                     : return tr("N", "Filter>Stockroom>None");
        }
    case Retain:
        return lot->retain() ? tr("Y", "Filter>Retain>Yes")
                             : tr("N", "Filter>Retain>No");
    default: {
        QVariant v = dataForEditRole(lot, f);
        if (v.isNull() || (v.userType() >= QMetaType::User))
            v = dataForDisplayRole(lot, f);
        return v;
    }
    }
}


void Document::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item())
        return;

    for (const auto *lot : qAsConst(m_lots)) {
        if ((pic->item() == lot->item()) && (pic->color() == lot->color())) {
            QModelIndex idx = index(const_cast<Lot *>(lot), Picture);
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
        LotList dummy2;
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
        LotList dummy2;
        filterDirect(filter, filterList, dummy1, dummy2);
        m_nextSortFilterIsDirect = false;
    }
}

void Document::nextSortFilterIsDirect()
{
    m_nextSortFilterIsDirect = true;
}

void Document::sortDirect(int column, Qt::SortOrder order, bool &sorted,
                          LotList &unsortedLots)
{
    bool emitSortOrderChanged = (order != m_sortOrder);
    bool emitSortColumnChanged = (column != m_sortColumn);
    bool wasSorted = isSorted();

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    m_sortColumn = column;
    m_sortOrder = order;

    if (!unsortedLots.isEmpty()) {
        m_isSorted = sorted;
        m_sortedLots = unsortedLots;
        unsortedLots.clear();

    } else {
        unsortedLots = m_sortedLots;
        sorted = m_isSorted;
        m_isSorted = true;
        m_sortedLots = m_lots;

        if (column >= 0) {
            qParallelSort(m_sortedLots.begin(), m_sortedLots.end(),
                          [column, this](const auto *lot1, const auto *lot2) {
                return compare(lot1, lot2, column) < 0;
            });
            if (order == Qt::DescendingOrder)
                std::reverse(m_sortedLots.begin(), m_sortedLots.end());

            // c++17 alternatives, but not supported everywhere yet:
            //std::stable_sort(std::execution::par_unseq, sorted.begin(), sorted.end(), sorter);
            //std::sort(std::execution::par_unseq, sorted.begin(), sorted.end(), sorter);
        }
    }

    // we were filtered before, but we don't want to refilter: the solution is to
    // keep the old filtered lots, but use the order from m_sortedLots
    if (!m_filteredLots.isEmpty()) {
        m_filteredLots = QtConcurrent::blockingFiltered(m_sortedLots, [this](auto *lot) {
            return m_filteredLots.contains(lot);
        });
    } else {
        m_filteredLots = m_sortedLots;
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(lot(idx), idx.column()));
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
                            bool &filtered, LotList &unfilteredLots)
{
    bool emitFilterChanged = (filterList != m_filterList);
    bool wasFiltered = isFiltered();

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    QModelIndexList before = persistentIndexList();

    m_filterString = filterString;
    m_filterList = filterList;

    if (!unfilteredLots.isEmpty()) {
        m_isFiltered = filtered;
        m_filteredLots = unfilteredLots;
        unfilteredLots.clear();

    } else {
        unfilteredLots = m_filteredLots;
        filtered = m_isFiltered;
        m_isFiltered = true;
        m_filteredLots = m_sortedLots;

        if (!filterList.isEmpty()) {
            m_filteredLots = QtConcurrent::blockingFiltered(m_sortedLots, [this](auto *lot) {
                return filterAcceptsLot(lot);
            });
        }
    }

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(lot(idx), idx.column()));
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

    ds << qint32(m_sortedLots.size());
    for (int i = 0; i < m_sortedLots.size(); ++i) {
        auto *lot = m_sortedLots.at(i);
        qint32 row = m_lots.indexOf(lot);
        bool visible = m_filteredLots.contains(lot);

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
    if ((ds.status() != QDataStream::Ok) || (viewSize != m_lots.size()))
        return false;

    LotList sortedLots;
    LotList filteredLots;
    int lotsSize = m_lots.size();
    while (viewSize--) {
        qint32 pos;
        ds >> pos;
        if ((pos < -lotsSize) || (pos >= lotsSize))
            return false;
        bool visible = (pos >= 0);
        pos = visible ? pos : (-pos - 1);
        auto *lot = m_lots.at(pos);
        sortedLots << lot;
        if (visible)
            filteredLots << lot;
    }

    if (ds.status() != QDataStream::Ok)
        return false;

    auto filterList = m_filterParser->parse(filterString);

    bool willBeSorted = true;
    sortDirect(sortColumn, Qt::SortOrder(sortOrder), willBeSorted, sortedLots);
    bool willBeFiltered = true;
    filterDirect(filterString, filterList, willBeFiltered, filteredLots);
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


bool Document::filterAcceptsLot(const Lot *lot) const
{
    if (!lot)
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
            QVariant v = dataForFilterRole(lot, static_cast<Field>(col));
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

int Document::compare(const Lot *i1, const Lot *i2, int sortColumn) const
{
    switch (sortColumn) {
    case Document::Index       : {
        return m_lots.indexOf(const_cast<Lot *>(i1)) - m_lots.indexOf(const_cast<Lot *>(i2));
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
    case Document::PartNo      : return Utility::naturalCompare(QLatin1String(i1->itemId()),
                                                                QLatin1String(i2->itemId()));
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
        auto base1 = differenceBaseLot(i1);
        auto base2 = differenceBaseLot(i2);
        return doubleCompare(base1 ? base1->price() : 0, base2 ? base2->price() : 0);
    }
    case Document::PriceDiff   : {
        auto base1 = differenceBaseLot(i1);
        auto base2 = differenceBaseLot(i2);
        return doubleCompare(base1 ? i1->price() - base1->price() : 0,
                             base2 ? i2->price() - base2->price() : 0);
    }
    case Document::QuantityOrig: {
        auto base1 = differenceBaseLot(i1);
        auto base2 = differenceBaseLot(i2);
        return (base1 ? base1->quantity() : 0) - (base2 ? base2->quantity() : 0);
    }

    case Document::QuantityDiff:  {
        auto base1 = differenceBaseLot(i1);
        auto base2 = differenceBaseLot(i2);
        return (base1 ? i1->quantity() - base1->quantity() : 0)
                - (base2 ? i2->quantity() - base2->quantity() : 0);
    }
    }
    return 0;
}

LotList Document::sortLotList(const LotList &list) const
{
    LotList result(list);
    qParallelSort(result.begin(), result.end(), [this](const auto &i1, const auto &i2) {
        return index(i1).row() < index(i2).row();
    });
    return result;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


const QString DocumentLotsMimeData::s_mimetype = "application/x-bricklink-invlots"_l1;

DocumentLotsMimeData::DocumentLotsMimeData(const LotList &lots)
    : QMimeData()
{
    setLots(lots);
}

void DocumentLotsMimeData::setLots(const LotList &lots)
{
    QByteArray data;
    QString text;

    QDataStream ds(&data, QIODevice::WriteOnly);

    ds << quint32(lots.count());
    for (const Lot *lot : lots) {
        lot->save(ds);
        if (!text.isEmpty())
            text.append("\n"_l1);
        text.append(QLatin1String(lot->itemId()));
    }
    setText(text);
    setData(s_mimetype, data);
}

LotList DocumentLotsMimeData::lots(const QMimeData *md)
{
    LotList lots;

    if (md) {
        QByteArray data = md->data(s_mimetype);
        QDataStream ds(data);

        if (!data.isEmpty()) {
            quint32 count = 0;
            ds >> count;

            for (; count && !ds.atEnd(); count--) {
                if (auto lot = Lot::restore(ds))
                    lots.append(lot);
            }
        }
    }
    return lots;
}

QStringList DocumentLotsMimeData::formats() const
{
    static QStringList sl;

    if (sl.isEmpty())
        sl << s_mimetype << "text/plain"_l1;

    return sl;
}

bool DocumentLotsMimeData::hasFormat(const QString &mimeType) const
{
    return mimeType.compare(s_mimetype) || mimeType.compare("text/plain"_l1);
}


#include "moc_document.cpp"
