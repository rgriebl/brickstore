// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QStringListModel>
#include <QTimer>
#include <QtAlgorithms>
#include <QtConcurrentFilter>

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "utility/utility.h"
#include "common/currency.h"
#include "common/undo.h"
#include "utility/qparallelsort.h"
#include "bricklink/core.h"
#include "bricklink/model.h"
#include "bricklink/picture.h"
#include "config.h"
#include "documentmodel.h"
#include "documentmodel_p.h"

using namespace std::chrono_literals;


template <auto G, auto S>
struct FieldOp
{
    template <typename Result> static Result returnType(Result (Lot::*)() const);
    using R = decltype(returnType(G));

    static void copy(const Lot &from, Lot &to)
    {
        (to.*S)((from.*G)());
    }

    static bool merge(const Lot &from, Lot &to,
                      DocumentModel::MergeMode mergeMode = DocumentModel::MergeMode::Merge,
                      const R defaultValue = { })
    {
        if (mergeMode == DocumentModel::MergeMode::Ignore) {
            return false;
        } else if (mergeMode == DocumentModel::MergeMode::Copy) {
            copy(from, to);
            return true;
        } else {
            return mergeInternal(from, to, mergeMode, defaultValue);
        }
    }

private:
    template<class Q = R>
    static typename std::enable_if<!std::is_same<Q, double>::value && !std::is_same<Q, QString>::value, bool>::type
    mergeInternal(const Lot &from, Lot &to, DocumentModel::MergeMode mergeMode,
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
    mergeInternal(const Lot &from, Lot &to, DocumentModel::MergeMode mergeMode,
                  const R defaultValue)
    {
        if (mergeMode == DocumentModel::MergeMode::MergeAverage) { // weighted by quantity
            int fromQty = std::abs(from.quantity());
            int toQty = std::abs(to.quantity());

            if ((fromQty == 0) && (toQty == 0))
                fromQty = toQty = 1;

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
    mergeInternal(const Lot &from, Lot &to, DocumentModel::MergeMode mergeMode,
                  const R defaultValue)
    {
        if (mergeMode == DocumentModel::MergeMode::MergeText) { // add or remove "tags"
            QString f = (from.*G)();
            QString t = (to.*G)();

            if (!f.isEmpty() && !t.isEmpty() && (f != t)) {
                QRegularExpression fromRe { u"\\b" + QRegularExpression::escape(f) + u"\\b" };

                if (!fromRe.match(t).hasMatch()) {
                    QRegularExpression toRe { u"\\b" + QRegularExpression::escape(t) + u"\\b" };

                    if (toRe.match(f).hasMatch())
                        (to.*S)(f);
                    else
                        (to.*S)(t + u" " + f);
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


AddRemoveCmd::AddRemoveCmd(Type t, DocumentModel *model, const QVector<int> &positions,
                           const QVector<int> &sortedPositions,
                           const QVector<int> &filteredPositions, const LotList &lots)
    : QUndoCommand(genDesc(t == Add, int(std::max(lots.count(), positions.count()))))
    , m_model(model)
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
        if (m_model) {
            for (const auto lot : std::as_const(m_lots))
                m_model->m_differenceBase.remove(lot);
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
        // DocumentModel::insertLotsDirect() adds all m_lots at the positions given in
        // m_*positions (or append them to the document in case m_*positions is empty)
        m_model->insertLotsDirect(m_lots, m_positions, m_sortedPositions, m_filteredPositions);
        m_positions.clear();
        m_sortedPositions.clear();
        m_filteredPositions.clear();
        m_type = Remove;
    }
    else {
        // DocumentModel::removeLotsDirect() removes all m_lots and records the positions
        // in m_*positions
        m_model->removeLotsDirect(m_lots, m_positions, m_sortedPositions, m_filteredPositions);
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
        return DocumentModel::tr("Added %n item(s)", nullptr, count);
    else
        return DocumentModel::tr("Removed %n item(s)", nullptr, count);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QTimer *ChangeCmd::s_eventLoopCounter = nullptr;

ChangeCmd::ChangeCmd(DocumentModel *model, const std::vector<std::pair<Lot *, Lot>> &changes, DocumentModel::Field hint)
    : QUndoCommand()
    , m_model(model)
    , m_hint(hint)
    , m_changes(changes)
{
    std::sort(m_changes.begin(), m_changes.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });

    if (!s_eventLoopCounter) {
        s_eventLoopCounter = new QTimer(QCoreApplication::instance());
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
            .arg((m_hint < DocumentModel::FieldCount) ? m_model->headerData(m_hint, Qt::Horizontal).toString()
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
                                           std::make_pair(change.first, Lot { }),
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
    m_model->changeLotsDirect(m_changes);
}

void ChangeCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


CurrencyCmd::CurrencyCmd(DocumentModel *model, const QString &ccode, double crate)
    : QUndoCommand(QCoreApplication::translate("CurrencyCmd", "Changed currency"))
    , m_model(model)
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

    QString oldccode = m_model->currencyCode();
    m_model->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}

void CurrencyCmd::undo()
{
    Q_ASSERT(m_prices);

    QString oldccode = m_model->currencyCode();
    m_model->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ResetDifferenceModeCmd::ResetDifferenceModeCmd(DocumentModel *model, const LotList &lots)
    : QUndoCommand(QCoreApplication::translate("ResetDifferenceModeCmd", "Reset difference mode base values"))
    , m_model(model)
    , m_differenceBase(model->m_differenceBase)
{
    for (const Lot *lot : lots)
        m_differenceBase.insert(lot, *lot);
}

int ResetDifferenceModeCmd::id() const
{
    return CID_ResetDifferenceMode;
}

void ResetDifferenceModeCmd::redo()
{
    m_model->resetDifferenceModeDirect(m_differenceBase);
}

void ResetDifferenceModeCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SortCmd::SortCmd(DocumentModel *model, const QVector<QPair<int, Qt::SortOrder>> &columns)
    : QUndoCommand(QCoreApplication::translate("SortCmd", "Sorted the view"))
    , m_model(model)
    , m_created(QDateTime::currentDateTime())
    , m_columns(columns)
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
    auto oldColumns = m_model->sortColumns();

    m_model->sortDirect(m_columns, m_isSorted, m_unsorted);

    m_columns = oldColumns;
}

void SortCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


FilterCmd::FilterCmd(DocumentModel *model, const QVector<Filter> &filterList)
    : QUndoCommand(QCoreApplication::translate("FilterCmd", "Filtered the view"))
    , m_model(model)
    , m_created(QDateTime::currentDateTime())
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
    auto oldFilterList = m_model->m_filter;

    m_model->filterDirect(m_filterList, m_isFiltered, m_unfiltered);

    m_filterList = oldFilterList;
}

void FilterCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


DocumentStatistics::DocumentStatistics(const DocumentModel *model, const LotList &list,
                                       bool ignoreExcluded, bool ignorePriceAndQuantityErrors)
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

        auto flags = model->lotFlags(lot);
        if (flags.first) {
            if (ignorePriceAndQuantityErrors)
                flags.first &= ((1ULL << DocumentModel::PartNo) | (1ULL << DocumentModel::Color));
            m_errors += qPopulationCount(flags.first);
        }
        if (flags.second)
            m_differences += qPopulationCount(flags.second);

        if (lot->isIncomplete())
            m_incomplete++;
    }
    if (weight_missing)
        m_weight = qFuzzyIsNull(m_weight) ? -std::numeric_limits<double>::min() : -m_weight;
    m_ccode = model->currencyCode();
}

QString DocumentStatistics::asHtmlTable() const
{
    QLocale loc;
    QString wgtstr;
    QString minvalstr;
    QString valstr = Currency::toDisplayString(value());
    bool hasMinValue = !qFuzzyCompare(value(), minValue());

    if (hasMinValue)
        minvalstr = Currency::toDisplayString(minValue());

    QString coststr = Currency::toDisplayString(cost());
    QString profitstr;
    if (!qFuzzyIsNull(cost())) {
        int percent = int(std::round(value() / cost() * 100. - 100.));
        profitstr = (percent > 0 ? u"(+" : u"(") + loc.toString(percent) + u" %)";
    }


    if (qFuzzyCompare(weight(), -std::numeric_limits<double>::min())) {
        wgtstr = u"-"_qs;
    } else {
        double weight = m_weight;

        if (weight < 0) {
            weight = -weight;
            wgtstr = tr("min.") + u' ';
        }

        wgtstr += Utility::weightToString(weight, Config::inst()->measurementSystem(), true, true);
    }

    static const char *fmt =
            "<table cellspacing=6>"
            "<tr><td>&nbsp;&nbsp;%2 </td><td colspan=2 align=right>&nbsp;&nbsp;%3</td></tr>"
            "<tr><td>&nbsp;&nbsp;%4 </td><td colspan=2 align=right>&nbsp;&nbsp;%5</td></tr>"
            "<tr></tr>"
            "<tr><td>&nbsp;&nbsp;%6 </td><td>&nbsp;&nbsp;%7</td><td align=right>%8</td></tr>"
            "<tr><td>&nbsp;&nbsp;%9 </td><td>&nbsp;&nbsp;%10</td><td align=right>%11</td></tr>"
            "<tr><td>&nbsp;&nbsp;%12 </td><td>&nbsp;&nbsp;%13</td><td align=right>%14</td><td>&nbsp;&nbsp;%15</td></tr>"
            "<tr></tr>"
            "<tr><td>&nbsp;&nbsp;%16 </td><td colspan=2 align=right>&nbsp;&nbsp;%17</td></tr>"
            "</table>";

    return QString::fromLatin1(fmt).arg(
                tr("Lots:"), loc.toString(lots()),
                tr("Items:"), loc.toString(items()),
                tr("Total:"), m_ccode, valstr).arg(
                hasMinValue ? tr("Total (min.):") : QString(), hasMinValue ? m_ccode : QString(), minvalstr,
                tr("Cost:"), m_ccode, coststr, profitstr,
                tr("Weight:"), wgtstr);
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

std::function<DocumentModel::ConsolidateFunction> DocumentModel::s_consolidateFunction;


DocumentModel *DocumentModel::createTemporary(const LotList &list, const QVector<int> &fakeIndexes)
{
    auto *model = new DocumentModel(1 /*dummy*/);
    LotList lots;
    lots.reserve(list.size());

    // the caller owns the items, so we have to copy here
    for (const Lot *lot : list)
        lots.append(new Lot(*lot));

    model->setLotsDirect(lots);
    model->setFakeIndexes(fakeIndexes);
    return model;
}

DocumentModel::DocumentModel(int /*is temporary*/)
    : m_filterParser(new Filter::Parser())
    , m_currencycode(Config::inst()->defaultCurrencyCode())
{
    initializeColumns();

    MODELTEST_ATTACH(this)

    connect(BrickLink::core()->pictureCache(), &BrickLink::PictureCache::pictureUpdated,
            this, &DocumentModel::pictureUpdated);

    languageChange();
}

DocumentModel::DocumentModel()
    : DocumentModel(0)
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
}

// the caller owns the items
DocumentModel::DocumentModel(BrickLink::IO::ParseResult &&pr, bool forceModified)
    : DocumentModel()
{
    m_fixedLotCount = pr.fixedLotCount();
    m_invalidLotCount = pr.invalidLotCount();

    // we take ownership of the items
    setLotsDirect(pr.takeLots());

    if (!pr.currencyCode().isEmpty()) {
        if (pr.currencyCode() == u"$$$"_qs) // legacy USD
            m_currencycode.clear();
        else
            m_currencycode = pr.currencyCode();
    }

    if (forceModified)
        m_undo->resetClean();

    auto db = pr.differenceModeBase(); // get rid of const
    if (!db.isEmpty())
        resetDifferenceModeDirect(db);
}

DocumentModel::~DocumentModel()
{
    qDeleteAll(m_lots);
    m_lots.clear();
    blockSignals(true);
    delete m_undo;

    //qWarning() << "~" << this;
}

QCoro::Task<int> DocumentModel::addLots(BrickLink::LotList &&lotsRef, AddLotMode addLotMode)
{
    if (!s_consolidateFunction)
        co_return -1;

    if (lotsRef.empty())
        co_return -1;

    // We own the items now, but we have to move them into a local variable, because

    // the lotsRef reference might go out of scope when we co_await later
    LotList lots(lotsRef);
    lotsRef.clear();

    if (addLotMode == AddLotMode::ConsolidateWithExisting) {
        if (lots.size() != 1) {
            qDeleteAll(lots);
            co_return -1;
        }
    }

    // the first lot in c.lots is either an existing lot for merges or nullptr for straight adds
    QVector<Consolidate> interactiveConsolidateList;
    QVector<Consolidate> quietConsolidateList;

    // We cannot just concatenate both lists afterwards, because we would loose the the original
    // order. Instead we remember here from which list to pick the next item when iterating
    enum : char { CT_Quiet = 1, CT_Interactive, CT_Ignore };
    QByteArray consolidationType(lots.size(), CT_Quiet);

    // If we add 2 of the same, we would end up in a bad situation, if we handled each lot
    // consolidation individually: both would have the same "mergeLot", but if the first
    // wants to consolidate to the new one and the second on to the existing, the existing lot
    // would have been deleted by the time we resolve the second consolidation.
    // The old system popped up the consolidation dialog for each single lot while looping, so
    // it didn't have to deal with this problem.
    QHash<BrickLink::Lot *, qsizetype> mergedLots;
    int mergedCount = 0;

    for (int i = 0; i < lots.size(); ++i) {
        Lot *lot = lots.at(i);

        if (addLotMode != AddLotMode::AddAsNew) {
            Lot *mergeLot = nullptr;

            for (int j = int(m_sortedLots.count()) - 1; j >= 0; --j) {
                Lot *otherLot = m_sortedLots.at(j);
                if (canLotsBeMerged(*lot, *otherLot)) {
                    mergeLot = otherLot;
                    break;
                }
            }
            if (!mergeLot) {  // record "lot" to be added
                Consolidate c({ nullptr, lot });
                quietConsolidateList.append(c);  // record "lot" to be added
            } else {
                auto mergedIndex = mergedLots.value(mergeLot, -1);

                if (addLotMode == AddLotMode::ConsolidateWithExisting) {
                    // record "lot" be quantity merged, destination == mergeLot
                    if (mergedIndex == -1) {
                        Consolidate c({ mergeLot, lot });
                        c.destinationIndex = 0;
                        quietConsolidateList.append(c);
                        mergedLots.insert(mergeLot, quietConsolidateList.size() - 1);
                    } else {
                        Consolidate &c = quietConsolidateList[mergedIndex];
                        c.lots.append(lot);
                        ++mergedCount;
                        consolidationType[i] = CT_Ignore;
                    }
                } else { // AddLotMode::ConsolidateInteractive
                    if (mergedIndex == -1) {
                        Consolidate c({ mergeLot, lot });
                        interactiveConsolidateList.append(c);
                        mergedLots.insert(mergeLot, interactiveConsolidateList.size() - 1);
                        consolidationType[i] = CT_Interactive;
                    } else {
                        Consolidate &c = interactiveConsolidateList[mergedIndex];
                        c.lots.append(lot);
                        ++mergedCount;
                        consolidationType[i] = CT_Ignore;
                    }
                }
            }
        } else {
            Consolidate c({ nullptr, lot });
            quietConsolidateList.append(c);  // record "lot" to be added
        }
    }

    Q_ASSERT((interactiveConsolidateList.size() + quietConsolidateList.size() + mergedCount) == lots.size());
    Q_ASSERT(consolidationType.count(CT_Interactive) == interactiveConsolidateList.size());
    Q_ASSERT(consolidationType.count(CT_Quiet) == quietConsolidateList.size());

    if (!interactiveConsolidateList.isEmpty()) {
        if (!co_await s_consolidateFunction(this, interactiveConsolidateList, true /*add items*/)) {
            qDeleteAll(lots);
            co_return -1;
        }
    }

    Lot *lastAdded = nullptr;
    int addCount = 0;
    int consolidateCount = 0;

    auto quietIt = quietConsolidateList.cbegin();
    auto interactiveIt = interactiveConsolidateList.cbegin();

    beginMacro();

    for (int i = 0; i < lots.size(); ++i) {
        auto ct = consolidationType.at(i);
        if (ct == CT_Ignore)
            continue;
        auto it = (ct == CT_Interactive) ? interactiveIt++ : quietIt++;
        const Consolidate &consolidate = *it;
        BrickLink::LotList newLotPtrs = consolidate.lots.mid(1);

        Q_ASSERT(!newLotPtrs.isEmpty());

        if (consolidate.destinationIndex < 0) { // just add all the new lots
            for (auto *newLotPtr : std::as_const(newLotPtrs))
                newLotPtr->setDateAdded(QDateTime::currentDateTimeUtc());
            lastAdded = newLotPtrs.constLast();
            appendLots(std::move(newLotPtrs));  // pass on ownership
            ++addCount;
        } else {
            Q_ASSERT(consolidate.lots.size() >= 2);
            Q_ASSERT(consolidate.destinationIndex < consolidate.lots.size());

            auto *existingLotPtr = consolidate.lots.at(0);
            Q_ASSERT(existingLotPtr);

            if (consolidate.destinationIndex == 0) {
                // merge all new lots into existing, delete all new
                Lot consolidatedLot = *existingLotPtr;
                for (auto *newLotPtr : std::as_const(newLotPtrs)) {
                    mergeLotFields(*newLotPtr, consolidatedLot, consolidate.fieldMergeModes);
                    consolidatedLot.setQuantity(consolidatedLot.quantity() + newLotPtr->quantity());
                }
                changeLot(existingLotPtr, consolidatedLot);
                qDeleteAll(newLotPtrs); // we own it, but we don't need it anymore
                ++consolidateCount;
            } else if (consolidate.destinationIndex > 0) {
                // merge existing into new, merge all other new ones too, add one new,
                // delete the others, remove existing
                auto *newLotPtr = consolidate.lots[consolidate.destinationIndex];
                for (auto *lotPtr : consolidate.lots) {
                    if (newLotPtr == lotPtr)
                        continue;
                    mergeLotFields(*lotPtr, *newLotPtr, consolidate.fieldMergeModes);
                    newLotPtr->setQuantity(newLotPtr->quantity() + lotPtr->quantity());
                }
                newLotPtr->setDateAdded(QDateTime::currentDateTimeUtc());

                newLotPtrs.removeOne(newLotPtr);
                qDeleteAll(newLotPtrs);

                appendLot(std::move(newLotPtr)); // pass on ownership
                removeLot(existingLotPtr);
                ++consolidateCount;
            }
        }
    }

    Q_ASSERT(quietIt == quietConsolidateList.cend());
    Q_ASSERT(interactiveIt == interactiveConsolidateList.cend());

    endMacro(tr("Added %1, consolidated %2 items").arg(addCount).arg(consolidateCount));

    lots.clear();

    co_return lastAdded ? index(lastAdded).row() : -1;
}


QCoro::Task<> DocumentModel::consolidateLots(BrickLink::LotList lots)
{
    if (!s_consolidateFunction)
        co_return;

    if (lots.count() < 2)
        co_return;

    QVector<Consolidate> consolidateList;
    LotList sourceLots = lots;

    for (int i = 0; i < sourceLots.count(); ++i) {
        Lot *lot = sourceLots.at(i);
        LotList mergeLots;

        for (int j = i + 1; j < sourceLots.count(); ++j) {
            Lot *otherLot = sourceLots.at(j);
            if (canLotsBeMerged(*lot, *otherLot))
                mergeLots << sourceLots.takeAt(j--);  // clazy:exclude=reserve-candidates
        }
        if (mergeLots.isEmpty())
            continue;

        mergeLots.prepend(sourceLots.at(i));
        consolidateList.emplace_back(mergeLots);
    }

    if (consolidateList.isEmpty())
        co_return;

    if (!co_await s_consolidateFunction(this, consolidateList, false /*add items*/))
        co_return;

    bool startedMacro = false;
    int consolidateCount = 0;

    for (int cli = 0; cli < consolidateList.count(); ++cli) {
        const Consolidate &consolidate = consolidateList.at(cli);
        int destinationIndex = consolidate.destinationIndex;

        if ((destinationIndex < 0) || (destinationIndex >= consolidate.lots.count()))
            continue;

        if (!startedMacro) {
            beginMacro();
            startedMacro = true;
        }

        Lot *destinationLotPtr = consolidate.lots.at(destinationIndex);
        Lot consolidatedLot = *destinationLotPtr;

        for (Lot *lotPtr : consolidate.lots) {
            if (lotPtr != destinationLotPtr) {
                mergeLotFields(*lotPtr, consolidatedLot, consolidate.fieldMergeModes);
                consolidatedLot.setQuantity(consolidatedLot.quantity() + lotPtr->quantity());

                if (consolidate.doNotDeleteEmpty) {
                    Lot zeroedLot = *lotPtr;
                    zeroedLot.setQuantity(0);
                    changeLot(lotPtr, zeroedLot);
                } else {
                    removeLot(lotPtr);
                }
            }
        }
        changeLot(destinationLotPtr, consolidatedLot);

        ++consolidateCount;
    }
    if (startedMacro)
        endMacro(tr("Consolidated %n item(s)", nullptr, consolidateCount));

}

DocumentModel::MergeModes DocumentModel::possibleMergeModesForField(Field field)
{
    // make sure to keep this in sync with mergeLotFields() below and class SelectMergeMode
    static const QHash<Field, MergeModes> possibleModes = {
        { Price,     MergeMode::Copy | MergeMode::Merge | MergeMode::MergeAverage },
        { Cost,      MergeMode::Copy | MergeMode::Merge | MergeMode::MergeAverage },
        { TierP1,    MergeMode::Copy | MergeMode::Merge | MergeMode::MergeAverage },
        { TierP2,    MergeMode::Copy | MergeMode::Merge | MergeMode::MergeAverage },
        { TierP3,    MergeMode::Copy | MergeMode::Merge | MergeMode::MergeAverage },
        { Quantity,  MergeMode::Copy | MergeMode::Merge },
        { Bulk,      MergeMode::Copy | MergeMode::Merge },
        { Sale,      MergeMode::Copy | MergeMode::Merge },
        { Comments,  MergeMode::Copy | MergeMode::Merge | MergeMode::MergeText },
        { Remarks,   MergeMode::Copy | MergeMode::Merge | MergeMode::MergeText },
        { Reserved,  MergeMode::Copy | MergeMode::Merge },
        { Retain,    MergeMode::Copy },
        { Stockroom, MergeMode::Copy },
    };
    return possibleModes.value(field, MergeMode::Ignore);
}

DocumentModel::FieldMergeModes DocumentModel::createFieldMergeModes(MergeMode mergeMode)
{
    FieldMergeModes fmms;

    if (mergeMode != MergeMode::Ignore) {
        for (int i = 0; i < FieldCount; ++i) {
            auto field = static_cast<Field>(i);
            auto possibleModes = possibleMergeModesForField(field);
            if (possibleModes != MergeMode::Ignore) {
                MergeMode mode = mergeMode;
                // clang-tidy complains about the loop running until the enum overflows,
                // but this cannot happen, as we made sure that possibleModes is not Ignore
                while ((mode != MergeMode::Ignore) && !possibleModes.testFlag(mode))
                    mode = static_cast<MergeMode>(int(mode) >> 1);

                if (mode != MergeMode::Ignore)
                    fmms.insert(field, mode);
            }
        }
    }
    return fmms;
}

bool DocumentModel::canLotsBeMerged(const Lot &lot1, const Lot &lot2)
{
    return ((&lot1 != &lot2)
            && !lot1.isIncomplete()
            && !lot2.isIncomplete()
            && (lot1.item() == lot2.item())
            && (lot1.color() == lot2.color())
            && (lot1.condition() == lot2.condition())
            && (lot1.subCondition() == lot2.subCondition())
            && ((lot1.status() == BrickLink::Status::Exclude) ==
                (lot2.status() == BrickLink::Status::Exclude)));
}

bool DocumentModel::mergeLotFields(const Lot &from, Lot &to, const FieldMergeModes &fieldMergeModes)
{
    if (!canLotsBeMerged(from, to))
        return false;

    if (fieldMergeModes.isEmpty())
        return false;

    auto mergeModeFor = [&fieldMergeModes](Field f) -> MergeMode {
        return fieldMergeModes.value(f, MergeMode::Ignore);
    };

    bool changed = false;

    if (FieldOp<&Lot::price, &Lot::setPrice>::merge(from, to, mergeModeFor(Price)))
        changed = true;
    if (FieldOp<&Lot::cost, &Lot::setCost>::merge(from, to, mergeModeFor(Cost)))
        changed = true;
    if (FieldOp<&Lot::tierPrice0, &Lot::setTierPrice0>::merge(from, to, mergeModeFor(TierP1)))
        changed = true;
    if (FieldOp<&Lot::tierPrice1, &Lot::setTierPrice1>::merge(from, to, mergeModeFor(TierP2)))
        changed = true;
    if (FieldOp<&Lot::tierPrice2, &Lot::setTierPrice2>::merge(from, to, mergeModeFor(TierP3)))
        changed = true;
    if (FieldOp<&Lot::quantity, &Lot::setQuantity>::merge(from, to, mergeModeFor(Quantity)))
        changed = true;
    if (FieldOp<&Lot::bulkQuantity, &Lot::setBulkQuantity>::merge(from, to, mergeModeFor(Bulk)))
        changed = true;
    if (FieldOp<&Lot::sale, &Lot::setSale>::merge(from, to, mergeModeFor(Sale)))
        changed = true;
    if (FieldOp<&Lot::comments, &Lot::setComments>::merge(from, to, mergeModeFor(Comments)))
        changed = true;
    if (FieldOp<&Lot::remarks, &Lot::setRemarks>::merge(from, to, mergeModeFor(Remarks)))
        changed = true;
    if (FieldOp<&Lot::reserved, &Lot::setReserved>::merge(from, to, mergeModeFor(Reserved)))
        changed = true;
    if (FieldOp<&Lot::retain, &Lot::setRetain>::merge(from, to, mergeModeFor(Retain)))
        changed = true;
    if (FieldOp<&Lot::stockroom, &Lot::setStockroom>::merge(from, to, mergeModeFor(Stockroom)))
        changed = true;

    //TODO: merge markers

    // this is not ideal, but should do the trick for the majority of cases
    if (FieldOp<&Lot::dateAdded, &Lot::setDateAdded>::merge(from, to, MergeMode::Merge))
        changed = true;
    if (FieldOp<&Lot::dateLastSold, &Lot::setDateLastSold>::merge(from, to, MergeMode::Merge))
        changed = true;

    return changed;
}

void DocumentModel::setConsolidateFunction(const std::function<ConsolidateFunction> &consolidateFunction)
{
    s_consolidateFunction = consolidateFunction;
}


double DocumentModel::maxLocalPrice(const QString &currencyCode)
{
    double crate = Currency::inst()->rate(currencyCode);
    if (qFuzzyIsNull(crate))
        return maxPrice;

    // add or remove '9's to or from maxPrice, depending on the conversion rate:
    //   crate = ]0.5 .. 5[ -> no change
    //   crate = ]0.05 .. 0.5] -> remove one '9'
    //   crate = [5 .. 50[ -> add one '9'
    //   add or remove one more '9' for each factor of 10 in conversion rate
    return pow(10, int(log10(maxPrice * crate) + .3)) - 1;
}

const LotList &DocumentModel::lots() const
{
    return m_lots;
}

const LotList &DocumentModel::sortedLots() const
{
    return m_sortedLots;
}

const LotList &DocumentModel::filteredLots() const
{
    return m_filteredLots;
}

DocumentStatistics DocumentModel::statistics(const LotList &list, bool ignoreExcluded,
                                             bool ignorePriceAndQuantityErrors) const
{
    return { this, list, ignoreExcluded, ignorePriceAndQuantityErrors };
}

void DocumentModel::beginMacro(const QString &label)
{
    m_undo->beginMacro(label);
}

void DocumentModel::endMacro(const QString &label)
{
    m_undo->endMacro(label);
}

QUndoStack *DocumentModel::undoStack() const
{
    return m_undo;
}


bool DocumentModel::clear()
{
    removeLots(m_lots);
    return true;
}

void DocumentModel::appendLot(Lot * &&lot)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, { }, { }, { }, { lot }));
    lot = nullptr;
}

void DocumentModel::appendLots(LotList &&lots)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, { }, { }, { }, lots));
    lots.clear();
}

void DocumentModel::insertLotsAfter(const Lot *afterLot, LotList &&lots)
{
    if (lots.empty())
        return;

    int afterPos = m_lotIndex.value(afterLot, -1) + 1;
    int afterSortedPos = int(m_sortedLots.indexOf(const_cast<Lot *>(afterLot))) + 1;
    int afterFilteredPos = m_filteredLotIndex.value(afterLot, -1) + 1;

    Q_ASSERT((afterPos > 0) && (afterSortedPos > 0));
    if (afterFilteredPos == 0)
        afterFilteredPos = int(m_filteredLots.size());

    QVector<int> positions(int(lots.size()));
    std::iota(positions.begin(), positions.end(), afterPos);
    QVector<int> sortedPositions(int(lots.size()));
    std::iota(sortedPositions.begin(), sortedPositions.end(), afterSortedPos);
    QVector<int> filteredPositions(int(lots.size()));
    std::iota(filteredPositions.begin(), filteredPositions.end(), afterFilteredPos);

    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, sortedPositions,
                                  filteredPositions, lots));
    lots.clear();
}

void DocumentModel::insertLotsAfter(const LotList &afterLots, LotList &&lots)
{
    if (lots.empty())
        return;
    if (afterLots.size() != lots.size())
        return;

    QVector<int> positions(int(lots.size()));
    QVector<int> sortedPositions(int(lots.size()));
    QVector<int> filteredPositions(int(lots.size()));

    // since we're inserting multiple lots at different places, inserting one might influence the
    // position (index) of the following ones. To account for this, we need to simulate the
    // actual insertion (x*Lots) to get the correct position for each lot.
    auto xlots = m_lots;
    auto xsortedLots = m_sortedLots;
    auto xfilteredLots = m_filteredLots;

    for (auto i = 0; i < lots.size(); ++i) {
        const Lot *afterLot = afterLots.at(i);

        int afterPos = int(xlots.indexOf(const_cast<Lot *>(afterLot))) + 1;
        int afterSortedPos = int(xsortedLots.indexOf(const_cast<Lot *>(afterLot))) + 1;
        int afterFilteredPos = int(xfilteredLots.indexOf(const_cast<Lot *>(afterLot))) + 1;

        Q_ASSERT((afterPos > 0) && (afterSortedPos > 0));
        if (afterFilteredPos == 0)
            afterFilteredPos = int(m_filteredLots.size());

        positions[i] = afterPos;
        sortedPositions[i] = afterSortedPos;
        filteredPositions[i] = afterFilteredPos;

        xlots.insert(afterPos, nullptr);
        xsortedLots.insert(afterSortedPos, nullptr);
        xfilteredLots.insert(afterFilteredPos, nullptr);
    }

    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, sortedPositions,
                                  filteredPositions, lots));
    lots.clear();
}

void DocumentModel::removeLot(Lot *lot)
{
    removeLots({ lot });
}

void DocumentModel::removeLots(const LotList &lots)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, { }, { }, { }, lots));
}

void DocumentModel::changeLot(Lot *lot, const Lot &value, DocumentModel::Field hint)
{
    m_undo->push(new ChangeCmd(this, {{ lot, value }}, hint));
}

void DocumentModel::changeLots(const std::vector<std::pair<Lot *, Lot>> &changes, Field hint)
{
    if (!changes.empty())
        m_undo->push(new ChangeCmd(this, changes, hint));
}

void DocumentModel::setLotsDirect(const LotList &lots)
{
    if (lots.empty())
        return;
    QVector<int> posDummy, sortedPosDummy, filteredPosDummy;
    insertLotsDirect(lots, posDummy, sortedPosDummy, filteredPosDummy);
}

void DocumentModel::insertLotsDirect(const LotList &lots, QVector<int> &positions,
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
    const QModelIndexList before = persistentIndexList();

    m_lotIndex.clear();
    m_filteredLotIndex.clear();

    for (Lot *lot : std::as_const(lots)) {
        if (!isAppend) {
            m_lots.insert(*pos++, lot);
            m_sortedLots.insert(*sortedPos++, lot);
            int filteredIndex = *filteredPos++;
            if (filteredIndex >= 0)
                m_filteredLots.insert(filteredIndex, lot);
        } else {
            m_lots.append(lot);
            m_sortedLots.append(lot);
            m_filteredLots.append(lot);
        }

        // this is really a new lot, not just a redo - start with no differences
        if (!m_differenceBase.contains(lot))
            m_differenceBase.insert(lot, *lot);
    }

    rebuildLotIndex();
    rebuildFilteredLotIndex();

    for (Lot *lot : std::as_const(lots))
        updateLotFlags(lot);

    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &idx : before)
        after.append(index(lot(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    emit lotCountChanged(int(m_lots.count()));
    emitStatisticsChanged();

    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void DocumentModel::removeLotsDirect(const LotList &lots, QVector<int> &positions,
                                     QVector<int> &sortedPositions, QVector<int> &filteredPositions)
{
    positions.resize(lots.count());
    sortedPositions.resize(lots.count());
    filteredPositions.resize(lots.count());

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    const QModelIndexList before = persistentIndexList();

    m_lotIndex.clear();
    m_filteredLotIndex.clear();

    for (int i = int(lots.count()) - 1; i >= 0; --i) {
        Lot *lot = lots.at(i);
        int idx = int(m_lots.indexOf(lot));
        int sortIdx = int(m_sortedLots.indexOf(lot));
        int filterIdx = int(m_filteredLots.indexOf(lot));
        Q_ASSERT(idx >= 0 && sortIdx >= 0);
        positions[i] = idx;
        sortedPositions[i] = sortIdx;
        filteredPositions[i] = filterIdx;
        m_lots.removeAt(idx);
        m_sortedLots.removeAt(sortIdx);
        if (filterIdx >= 0)
            m_filteredLots.removeAt(filterIdx);
    }

    rebuildLotIndex();
    rebuildFilteredLotIndex();

    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &idx : before)
        after.append(index(lot(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    emit lotCountChanged(int(m_lots.count()));
    emitStatisticsChanged();

    //TODO: we should remember and re-apply the isSorted/isFiltered state
    if (isSorted())
        emit isSortedChanged(m_isSorted = false);
    if (isFiltered())
        emit isFilteredChanged(m_isFiltered = false);
}

void DocumentModel::changeLotsDirect(std::vector<std::pair<Lot *, Lot>> &changes)
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

void DocumentModel::changeCurrencyDirect(const QString &ccode, double crate, double *&prices)
{
    m_currencycode = ccode;

    if (!qFuzzyCompare(crate, 1.) || (ccode != m_currencycode)) {
        bool createPrices = (prices == nullptr);
        if (createPrices)
            prices = new double[uint(5 * m_lots.count())];

        for (int i = 0; i < m_lots.count(); ++i) {
            Lot *lot = m_lots.value(i);
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

void DocumentModel::emitDataChanged(const QModelIndex &tl, const QModelIndex &br)
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
            if ((m_nextDataChangedEmit.first != QPoint(-1, -1)) &&
                (m_nextDataChangedEmit.second != QPoint(-1, -1))) {
                emit dataChanged(index(m_nextDataChangedEmit.first.y(),
                                       m_nextDataChangedEmit.first.x()),
                                 index(m_nextDataChangedEmit.second.y(),
                                       m_nextDataChangedEmit.second.x()));
            }

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

void DocumentModel::emitStatisticsChanged()
{
    if (!m_delayedEmitOfStatisticsChanged) {
        m_delayedEmitOfStatisticsChanged = new QTimer(this);
        m_delayedEmitOfStatisticsChanged->setSingleShot(true);
        m_delayedEmitOfStatisticsChanged->setInterval(0);

        connect(m_delayedEmitOfStatisticsChanged, &QTimer::timeout,
                this, &DocumentModel::statisticsChanged);
    }
    m_delayedEmitOfStatisticsChanged->start();
}

void DocumentModel::updateLotFlags(const Lot *lot)
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
    if (lot->status() == BrickLink::Status::Exclude)
        errors = 0;

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
                | (1ULL << TotalWeight)
                | (1ULL << YearReleased)
                | (1ULL << Marker)
                | (1ULL << DateAdded)
                | (1ULL << DateLastSold);

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

void DocumentModel::resetDifferenceMode(const LotList &lotList)
{
    m_undo->push(new ResetDifferenceModeCmd(this, lotList.isEmpty() ? lots() : lotList));
}

void DocumentModel::resetDifferenceModeDirect(QHash<const Lot *, Lot> &differenceBase)
{
    std::swap(m_differenceBase, differenceBase);

    for (const auto *lot : std::as_const(m_lots))
        updateLotFlags(lot);

    emitDataChanged();
}

const Lot *DocumentModel::differenceBaseLot(const Lot *lot) const
{
    if (!lot)
        return nullptr;

    auto it = m_differenceBase.constFind(lot);
    return (it != m_differenceBase.end()) ? &(*it) : nullptr;
}

bool DocumentModel::legacyCurrencyCode() const
{
    return m_currencycode.isEmpty();
}

QString DocumentModel::currencyCode() const
{
    return m_currencycode.isEmpty() ? u"USD"_qs : m_currencycode;
}

void DocumentModel::setCurrencyCode(const QString &ccode, double crate)
{
    if (ccode != currencyCode())
        m_undo->push(new CurrencyCmd(this, ccode, crate));
}

void DocumentModel::adjustLotCurrencyToModel(BrickLink::LotList &lots, const QString &fromCurrency)
{
    if (currencyCode() != fromCurrency) {
        double r = Currency::inst()->crossRate(fromCurrency, currencyCode());
        if (!qFuzzyCompare(r, 1.)) {
            for (auto lot : lots) {
                lot->setCost(lot->cost() * r);
                lot->setPrice(lot->price() * r);
                lot->setTierPrice(0, lot->tierPrice(0) * r);
                lot->setTierPrice(1, lot->tierPrice(1) * r);
                lot->setTierPrice(2, lot->tierPrice(2) * r);
            }
        }
    }
}

void DocumentModel::applyTo(const LotList &lots, const std::function<DocumentModel::ApplyToResult(const Lot &, Lot &)> &callback,
                            const QString &actionText)
{
    if (lots.isEmpty())
        return;
    QString at = actionText;
    if (actionText.endsWith(u"..."))
        at.chop(3);
    if (!at.isEmpty())
        beginMacro();

    int count = int(lots.size());
    std::vector<std::pair<Lot *, Lot>> changes;
    changes.reserve(uint(count));

    for (Lot *from : lots) {
        Lot to = *from;
        switch (callback(*from, to)) {
        case LotChanged:
            changes.emplace_back(from, to);
            break;
        case LotDidNotChange:
            --count;
            break;
        default:
            break;
        }
    }
    changeLots(changes);

    if (!at.isEmpty()) {
        //: Generic undo/redo text: %1 == action name (e.g. "Set price")
        endMacro(tr("%1 on %Ln item(s)", nullptr, count).arg(at));
    }
}

Filter::Parser *DocumentModel::filterParser()
{
    return m_filterParser.get();
}

void DocumentModel::setFakeIndexes(const QVector<int> &fakeIndexes)
{
    m_fakeIndexes = fakeIndexes;
}

void DocumentModel::rebuildLotIndex()
{
    m_lotIndex.clear();
    for (auto i = 0; i < m_lots.size(); ++i)
        m_lotIndex[m_lots.at(i)] = i;
}

void DocumentModel::rebuildFilteredLotIndex()
{
    m_filteredLotIndex.clear();
    for (auto i = 0; i < m_filteredLots.size(); ++i)
        m_filteredLotIndex[m_filteredLots.at(i)] = i;
}

bool DocumentModel::isModified() const
{
    bool modified = !m_undo->isClean();
    if (modified && !Config::inst()->visualChangesMarkModified())
        modified = !m_visuallyClean;

    return modified;
}

bool DocumentModel::canBeSaved() const
{
    return !m_undo->isClean();
}

void DocumentModel::unsetModified()
{
    m_undo->setClean();
    updateModified();
}

QHash<const Lot *, Lot> DocumentModel::differenceBase() const
{
    return m_differenceBase;
}

void DocumentModel::updateModified()
{
    emit modificationChanged(isModified());
}

void DocumentModel::setLotFlagsMask(QPair<quint64, quint64> flagsMask)
{
    m_lotFlagsMask = flagsMask;
    emitStatisticsChanged();
    emitDataChanged();
}

QPair<quint64, quint64> DocumentModel::lotFlags(const Lot *lot) const
{
    auto flags = m_lotFlags.value(lot, { });
    flags.first &= m_lotFlagsMask.first;
    flags.second &= m_lotFlagsMask.second;
    return flags;
}

void DocumentModel::setLotFlags(const Lot *lot, quint64 errors, quint64 updated)
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


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
// Itemviews API
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


QModelIndex DocumentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid() && row >= 0 && column >= 0 && row < rowCount() && column < columnCount())
        return createIndex(row, column, m_filteredLots.at(row));
    return { };
}

Lot *DocumentModel::lot(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<Lot *>(idx.internalPointer()) : nullptr;
}

QModelIndex DocumentModel::index(const Lot *lot, int column) const
{
    int row = m_filteredLotIndex.value(lot, -1);
    if (row >= 0)
        return createIndex(row, column, const_cast<Lot *>(lot));
    return { };
}

int DocumentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_filteredLots.size());
}

int DocumentModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : FieldCount;
}

Qt::ItemFlags DocumentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return { };

    Qt::ItemFlags ifs = QAbstractItemModel::flags(index);
    const auto &c = m_columns.value(index.column());
    if (c.editable)
        ifs |= Qt::ItemIsEditable;
    if (c.checkable)
        ifs |= Qt::ItemIsUserCheckable;
    return ifs;
}

bool DocumentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (role != Qt::EditRole))
        return false;

    auto f = static_cast<Field>(index.column());
    Lot *lot = this->lot(index);
    Lot lotCopy = *lot;

    if (auto sdata = m_columns.value(f).setDataFn)
        sdata(lot, value);

    // this a bit awkward with all the copying, but setDataFn needs a lot pointer that is valid in the model
    if (*lot != lotCopy) {
        std::swap(*lot, lotCopy);
        changeLot(lot, lotCopy, f);
        return true;
    }
    return false;
}

QVariant DocumentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };

    const Lot *lot = static_cast<const Lot *>(index.internalPointer());
    auto f = static_cast<Field>(index.column());

    switch (role) {
    case Qt::DisplayRole      : return dataForDisplayRole(lot, f, false);
    case Qt::ToolTipRole      : return dataForDisplayRole(lot, f, true);
    case BaseDisplayRole      : return dataForDisplayRole(differenceBaseLot(lot), f, false);
    case BaseToolTipRole      : return dataForDisplayRole(differenceBaseLot(lot), f, true);
    case Qt::DecorationRole: {
        const auto &c = m_columns.value(f);
        return c.auxDataFn ? c.auxDataFn(lot) : QVariant { };
    }
    case Qt::TextAlignmentRole: return m_columns.value(int(f)).alignment | Qt::AlignVCenter;

    case Qt::EditRole         : return dataForEditRole(lot, f);
    case BaseEditRole         : return dataForEditRole(differenceBaseLot(lot), f);
    case FilterRole           : return dataForFilterRole(lot, f);

    case LotPointerRole       : return QVariant::fromValue(lot);
    case BaseLotPointerRole   : return QVariant::fromValue(differenceBaseLot(lot));
    case ErrorFlagsRole       : return lotFlags(lot).first;
    case DifferenceFlagsRole  : return lotFlags(lot).second;
    case DocumentFieldRole    : return f;
    case NullValueRole        : return headerData(int(f), Qt::Horizontal, HeaderNullValueRole);
    }

    return { };
}

QVariant DocumentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return { };

    const auto &c = m_columns.value(section);

    switch (role) {
    case Qt::TextAlignmentRole : return int(Qt::AlignLeft) | Qt::AlignVCenter;
    case Qt::DisplayRole       : return tr(c.title);
    case DocumentFieldRole     : return section;
    case HeaderDefaultWidthRole: return c.defaultWidth;
    case HeaderValueModelRole  : {
        if (c.valueModelFn) {
            return QVariant::fromValue(c.valueModelFn());
        } else if (!c.enumValues.empty()) {
            QStringList sl;
            sl.reserve(c.enumValues.size());
            for (const auto &[e, tt, filter] : c.enumValues)
                sl << filter;
            return QVariant::fromValue(new QStringListModel(sl));
        } else {
            return { };
        }
    }
    case HeaderFilterableRole  : return c.filterable;
    case HeaderNullValueRole   : {
        switch (c.type) {
        case Column::Type::Currency:
        case Column::Type::Weight:
            return 0.;
        case Column::Type::NonLocalizedInteger:
        case Column::Type::Integer:
            return c.integerNullValue;
        case Column::Type::Date:
            return QDateTime { };
        default:
            return { };
        }
    }
    default                    : return { };
    }
}


QString DocumentModel::dataForDisplayRole(const Lot *lot, Field f, bool asToolTip) const
{
    const auto &c = m_columns.value(f);
    if (c.displayFn) {
        return c.displayFn(lot, asToolTip);
    } else {
        QVariant v = c.dataFn ? c.dataFn(lot) : QVariant { };
        QLocale loc;

        switch (c.type) {
        case Column::Type::String:
            return v.toString();
        case Column::Type::Integer:
            return loc.toString(v.toLongLong());
        case Column::Type::NonLocalizedInteger:
            return QString::number(v.toLongLong());
        case Column::Type::Currency:
            return Currency::toDisplayString(v.toDouble());
        case Column::Type::Date: {
            if (QDateTime dt = v.toDateTime(); dt.isValid()) {
                if (dt.time() == QTime(0, 0)) {
                    return loc.toString(dt.date(), QLocale::ShortFormat);
                    //return HumanReadableTimeDelta::toString(QDate::currentDate().startOfDay(), dt);
                } else {
                    return loc.toString(dt.toLocalTime(), QLocale::ShortFormat);
                    //return HumanReadableTimeDelta::toString(QDateTime::currentDateTime(), dt);
                }
            }
            break;
        }
        case Column::Type::Weight: {
            const auto w = v.toDouble();
            return Utility::weightToString(w, Config::inst()->measurementSystem(), true, true);
        }
        case Column::Type::Enum: {
            if (!asToolTip)
                return { };
            const qint64 eint = v.toLongLong();
            for (const auto &[e, tt, filter] : c.enumValues) {
                if (eint == qint64(e))
                    return tt;
            }
            return { };
        }
        case Column::Type::Special:
            return { };
        }
        return { };
    }
}

QVariant DocumentModel::dataForEditRole(const Lot *lot, Field f) const
{
    auto data = m_columns.value(f).dataFn;
    return (data && lot) ? data(lot) : QVariant { };
}

QVariant DocumentModel::dataForFilterRole(const Lot *lot, Field f) const
{
    const auto &c = m_columns.value(f);
    if (!c.filterable) {
        return { };
    } else {
        QVariant v;
        if (c.dataFn)
            v = c.dataFn(lot);

        if ((c.type == Column::Type::Enum) && v.isValid()) {
            const qint64 e = v.toLongLong();
            for (const auto &[value, tooltip, filter] : c.enumValues) {
                if (e == value) {
                    v = filter;
                    break;
                }
            }
        }

        if ((v.isNull() || (v.userType() >= QMetaType::User)) && c.displayFn)
            v = c.displayFn(lot, false);

        return v;
    }
}

QHash<int, QByteArray> DocumentModel::roleNames() const
{
    static QHash<int, QByteArray> roles = {
        { Qt::DisplayRole,        "display" },
        { Qt::DecorationRole,     "decoration" },
        { Qt::TextAlignmentRole,  "textAlignment" },
        { Qt::EditRole,           "edit" },
        { BaseDisplayRole,        "baseDisplay" },
        { BaseEditRole,           "baseEdit" },
        { FilterRole,             "filter" },
        { LotPointerRole,         "lot" },
        { BaseLotPointerRole,     "baseLot" },
        { ErrorFlagsRole,         "errorFlags" },
        { DifferenceFlagsRole,    "differenceFlags" },
        { DocumentFieldRole,      "documentField" },
        { NullValueRole,          "nullValue" },
        { HeaderDefaultWidthRole, "headerDefaultWidth" },
        { HeaderValueModelRole,   "headerValueModel" },
        { HeaderFilterableRole,   "headerFilterable" },
        { HeaderNullValueRole,    "headerNullValue" },
    };
    return roles;
}

void DocumentModel::initializeColumns()
{
    if (!m_columns.isEmpty())
        return;

    auto C = [this](Field f, const Column &c) { m_columns.insert(f, c); };

    C(Index, Column {
          .defaultWidth = 3,
          .alignment = Qt::AlignRight,
          .editable = false,
          .title = QT_TR_NOOP("Index"),
          .dataFn = [&](const Lot *lot) {
              if (m_fakeIndexes.isEmpty()) {
                  return QVariant { m_lotIndex.value(lot, -1) + 1 };
              } else {
                  auto fi = m_fakeIndexes.at(m_lotIndex.value(lot, -1));
                  return fi >= 0 ? QVariant { fi + 1 } : QVariant { u"+"_qs };
              }
          },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              return m_lotIndex.value(l1, -1) <=> m_lotIndex.value(l2, -1);
          },
      });

    C(Status, Column {
          .type = Column::Type::Enum,
          .defaultWidth = 6,
          .alignment = Qt::AlignHCenter,
          .title = QT_TR_NOOP("Status"),
          .enumValues = { { int(BrickLink::Status::Include), { },  tr("Include") },
                          { int(BrickLink::Status::Extra), { }, tr("Extra") },
                          { int(BrickLink::Status::Exclude), { }, tr("Exclude") } },
          .dataFn = [](const Lot *lot) { return QVariant::fromValue(lot->status()); },
          .setDataFn = [](Lot *lot, const QVariant &v) { lot->setStatus(v.value<BrickLink::Status>()); },

          .displayFn = [](const Lot *lot, bool asToolTip) -> QString {
              if (!asToolTip)
                  return { };

              QString tip;
              switch (lot->status()) {
              case BrickLink::Status::Exclude: tip = tr("Exclude"); break;
              case BrickLink::Status::Extra  : tip = tr("Extra"); break;
              case BrickLink::Status::Include: tip = tr("Include"); break;
              default                        : break;
              }

              if (lot->counterPart())
                  tip = tip + u"<br>(" + tr("Counter part") + u")";
              else if (lot->alternateId())
                  tip = tip + u"<br>(" + tr("Alternate match id: %1").arg(lot->alternateId()) + u")";
              return tip;
          },
          .compareFn = [](const Lot *l1, const Lot *l2) -> std::partial_ordering {
              if (l1->status() != l2->status()) {
                  return l1->status() <=> l2->status();
              } else if (l1->counterPart() != l2->counterPart()) {
                  return l1->counterPart() <=> l2->counterPart();
              } else if (l1->alternateId() != l2->alternateId()) {
                  return l1->alternateId() <=> l2->alternateId();
              } else if (l1->alternate() != l2->alternate()) {
                  return l1->alternate() <=> l2->alternate();
              } else {
                  return std::partial_ordering::equivalent;
              }
          }
      });

    C(Picture, Column {
          .type = Column::Type::Special,
          .defaultWidth = -80,
          .alignment = Qt::AlignHCenter,
          .filterable = false,
          .title = QT_TR_NOOP("Image"),
          .dataFn = [&](const Lot *lot) {
              return QVariant::fromValue(BrickLink::core()->pictureCache()->picture(lot->item(), lot->color()));
          },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setItem(v.value<const BrickLink::Item *>()); },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              return Utility::naturalCompare(QLatin1StringView { l1->itemId() },
                                             QLatin1StringView { l2->itemId() });
          },
      });
    C(PartNo, Column {
          .defaultWidth = 10,
          .title = QT_TR_NOOP("Item Id"),
          .dataFn = [&](const Lot *lot) { return lot->itemId(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) {
              char itid = lot->itemTypeId() ? lot->itemTypeId() : 'P';
              if (auto newItem = BrickLink::core()->item(itid, v.toString().toLatin1()))
                  lot->setItem(newItem);
          },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              return Utility::naturalCompare(QLatin1StringView { l1->itemId() },
                                             QLatin1StringView { l2->itemId() });
          },
      });
    C(Description, Column {
          .defaultWidth = 28,
          .title = QT_TR_NOOP("Description"),
          .dataFn = [&](const Lot *lot) { return QVariant::fromValue(lot->item()); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setItem(v.value<const BrickLink::Item *>()); },
          .displayFn = [&](const Lot *lot, bool) { return lot->itemName(); },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              return Utility::naturalCompare(l1->itemName(), l2->itemName());
          },
      });
    C(Comments, Column {
          .title = QT_TR_NOOP("Comments"),
          .dataFn = [&](const Lot *lot) { return lot->comments(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setComments(v.toString()); },
      });
    C(Remarks, Column {
          .title = QT_TR_NOOP("Remarks"),
          .dataFn = [](const Lot *lot) { return lot->remarks(); },
          .setDataFn = [](Lot *lot, const QVariant &v) { lot->setRemarks(v.toString()); },
      });
    C(QuantityOrig, Column {
                        .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .editable = false,
          .title = QT_TR_NOOP("Orig. Quantity"),
          .dataFn = [this](const Lot *lot) {
              auto base = differenceBaseLot(lot);
              return base ? base->quantity() : 0;
          },
      });
    C(QuantityDiff, Column {
                        .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Diff. Quantity"),
          .dataFn = [this](const Lot *lot) {
              auto base = differenceBaseLot(lot);
              return base ? lot->quantity() - base->quantity() : 0;
          },
          .setDataFn = [this](Lot *lot, const QVariant &v) {
              if (auto base = differenceBaseLot(lot))
                  lot->setQuantity(base->quantity() + v.toInt());
          },
      });
    C(Quantity, Column {
          .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Quantity"),
          .dataFn = [&](const Lot *lot) { return lot->quantity(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setQuantity(v.toInt()); },
      });
    C(Bulk, Column {
          .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .integerNullValue = 1,
          .title = QT_TR_NOOP("Bulk"),
          .dataFn = [&](const Lot *lot) { return lot->bulkQuantity(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setBulkQuantity(v.toInt()); },
      });
    C(PriceOrig, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .editable = false,
          .title = QT_TR_NOOP("Orig. Price"),
          .dataFn = [&](const Lot *lot) {
              auto base = differenceBaseLot(lot);
              return base ? base->price() : 0;
          },
      });
    C(PriceDiff, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Diff. Price"),
          .dataFn = [&](const Lot *lot) {
              auto base = differenceBaseLot(lot);
              return base ? lot->price() - base->price() : 0;
          },
          .setDataFn = [&](Lot *lot, const QVariant &v) {
              if (auto base = differenceBaseLot(lot))
                  lot->setPrice(base->price() + v.toDouble());
          },
      });
    C(Cost, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Cost"),
          .dataFn = [&](const Lot *lot) { return lot->cost(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setCost(v.toDouble()); },
      });
    C(Price, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Price"),
          .dataFn = [&](const Lot *lot) { return lot->price(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setPrice(v.toDouble()); },
      });
    C(Total, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .editable = false,
          .title = QT_TR_NOOP("Total"),
          .dataFn = [&](const Lot *lot) { return lot->total(); },
      });
    C(Sale, Column {
          .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Sale"),
          .dataFn = [&](const Lot *lot) { return lot->sale(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setSale(v.toInt()); },
      });
    C(Condition, Column {
          .type = Column::Type::Enum,
          .defaultWidth = 5,
          .alignment = Qt::AlignHCenter,
          .title = QT_TR_NOOP("Condition"),
          .enumValues = { { int(BrickLink::Condition::New),  { }, tr("New") },
                          { int(BrickLink::Condition::Used), { }, tr("Used") } },
          .dataFn = [&](const Lot *lot) { return QVariant::fromValue(lot->condition()); },
          .auxDataFn = [&](const Lot *lot) { return QVariant::fromValue(lot->subCondition()); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setCondition(v.value<BrickLink::Condition>()); },
          .displayFn = [](const Lot *lot, bool asToolTip) {
              QString str;
              if (!asToolTip) {
                  str = (lot->condition() == BrickLink::Condition::New)
                            ?  tr("N", "List>Cond>New") : tr("U", "List>Cond>Used");
              } else {
                  str = (lot->condition() == BrickLink::Condition::New)
                            ?  tr("New", "ToolTip Cond>New") : tr("Used", "ToolTip Cond>Used");
              }

              if (lot->itemType() && lot->itemType()->hasSubConditions()
                      && (lot->subCondition() != BrickLink::SubCondition::None)) {
                  QString scStr;
                  switch (lot->subCondition()) {
                  case BrickLink::SubCondition::None      : break;
                  case BrickLink::SubCondition::Sealed    : scStr = tr("Sealed"); break;
                  case BrickLink::SubCondition::Complete  : scStr = tr("Complete"); break;
                  case BrickLink::SubCondition::Incomplete: scStr = tr("Incomplete"); break;
                  default: break;
                  }
                  if (!scStr.isEmpty())
                      str = str + u" (" + scStr + u")";
              }
              return str;
          },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              auto d = l1->condition() <=> l2->condition();
              return (d != 0) ? d : (l1->subCondition() <=> l2->subCondition());
          },
      });
    C(Color, Column {
          .type = Column::Type::Special,
          .defaultWidth = 15,
          .title = QT_TR_NOOP("Color"),
          .valueModelFn = [&]() {
              auto model = new BrickLink::ColorModel(nullptr);
              model->sort(0, Qt::AscendingOrder);
              QSet<const BrickLink::Color *> colors;
              for (const auto &lot : std::as_const(m_lots))
                  colors.insert(lot->color());
              model->setColorListFilter(QVector<const BrickLink::Color *>(colors.cbegin(), colors.cend()));
              return model;
          },
          .dataFn = [&](const Lot *lot) { return QVariant::fromValue(lot->color()); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setColor(v.value<const BrickLink::Color *>()); },
          .displayFn = [&](const Lot *lot, bool) { return lot->colorName(); },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
                     return l1->colorName().localeAwareCompare(l2->colorName()) <=> 0;
          },
      });
    C(Category, Column {
          .defaultWidth = 12,
          .editable = false,
          .title = QT_TR_NOOP("Category"),
          .valueModelFn = [&]() {
              QStringList sl;
              const auto &cats = BrickLink::core()->categories();
              std::for_each(cats.cbegin(), cats.cend(), [&](const auto &cat) { sl << cat.name(); });
              sl.sort(Qt::CaseInsensitive);
              return new QStringListModel(sl);
          },
          .dataFn = [&](const Lot *lot) { return lot->categoryName(); },
          .auxDataFn = [&](const Lot *lot) { return lot->categoryId(); },
      });
    C(ItemType, Column {
          .defaultWidth = 12,
          .editable = false,
          .title = QT_TR_NOOP("Item Type"),
          .valueModelFn = [&]() {
              QStringList sl;
              const auto &itts = BrickLink::core()->itemTypes();
              std::for_each(itts.cbegin(), itts.cend(), [&](const auto &itt) { sl << itt.name(); });
              return new QStringListModel(sl);
          },
          .dataFn = [&](const Lot *lot) { return lot->itemTypeName(); },
          .auxDataFn = [&](const Lot *lot) { return int(lot->itemTypeId()); },
      });
    C(TierQ1, Column {
          .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Tier Q1"),
          .dataFn = [&](const Lot *lot) { return lot->tierQuantity(0); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTierQuantity(0, v.toInt()); },
      });
    C(TierP1, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Tier P1"),
          .dataFn = [&](const Lot *lot) { return lot->tierPrice(0); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTierPrice(0, v.toDouble()); },
      });
    C(TierQ2, Column {
          .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Tier Q2"),
          .dataFn = [&](const Lot *lot) { return lot->tierQuantity(1); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTierQuantity(1, v.toInt()); },
      });
    C(TierP2, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Tier P2"),
          .dataFn = [&](const Lot *lot) { return lot->tierPrice(1); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTierPrice(1, v.toDouble()); },
      });
    C(TierQ3, Column {
          .type = Column::Type::Integer,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Tier Q3"),
          .dataFn = [&](const Lot *lot) { return lot->tierQuantity(2); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTierQuantity(2, v.toInt()); },
      });
    C(TierP3, Column {
          .type = Column::Type::Currency,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Tier P3"),
          .dataFn = [&](const Lot *lot) { return lot->tierPrice(2); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTierPrice(2, v.toDouble()); },
      });
    C(LotId, Column {
          .type = Column::Type::NonLocalizedInteger,
          .alignment = Qt::AlignRight,
          .editable = false,
          .title = QT_TR_NOOP("Lot Id"),
          .dataFn = [&](const Lot *lot) { return lot->lotId(); },
      });
    C(Retain, Column {
          .type = Column::Type::Enum,
          .alignment = Qt::AlignHCenter,
          .checkable = true,
          .title = QT_TR_NOOP("Retain"),
          .enumValues = { { 1, tr("Retain"),        tr("Yes", "Filter>Retain>Yes") },
                          { 0, tr("Do not retain"), tr("No",  "Filter>Retain>No") } },
          .dataFn = [&](const Lot *lot) { return lot->retain() ? 1 : 0; },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setRetain(v.toInt() != 0); },
      });
    C(Stockroom, Column {
          .type = Column::Type::Enum,
          .alignment = Qt::AlignHCenter,
          .title = QT_TR_NOOP("Stockroom"),
          .enumValues = { { int(BrickLink::Stockroom::None), tr("Stockroom") + u": " + tr("None", "Tooltip>Stockroom>None"), tr("None", "Filter>Stockroom>None") },
                          { int(BrickLink::Stockroom::A), tr("Stockroom") + u": A", u"A"_qs },
                          { int(BrickLink::Stockroom::B), tr("Stockroom") + u": B", u"B"_qs },
                          { int(BrickLink::Stockroom::C), tr("Stockroom") + u": C", u"C"_qs } },
          .dataFn = [&](const Lot *lot) { return QVariant::fromValue(lot->stockroom()); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setStockroom(v.value<BrickLink::Stockroom>()); },
      });
    C(Reserved, Column {
          .title = QT_TR_NOOP("Reserved"),
          .dataFn = [&](const Lot *lot) { return lot->reserved(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setReserved(v.toString()); },
      });
    C(Weight, Column {
          .type = Column::Type::Weight,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Weight"),
          .dataFn = [&](const Lot *lot) { return lot->weight(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setWeight(v.toDouble()); },
      });
    C(TotalWeight, Column {
          .type = Column::Type::Weight,
          .alignment = Qt::AlignRight,
          .title = QT_TR_NOOP("Total Weight"),
          .dataFn = [&](const Lot *lot) { return lot->totalWeight(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setTotalWeight(v.toDouble()); },
      });
    C(YearReleased, Column {
          .type = Column::Type::Special,
          .defaultWidth = 5,
          .alignment = Qt::AlignRight,
          .editable = false,
          .title = QT_TR_NOOP("Year"),
          .dataFn = [](const Lot *lot) { return lot->itemYearReleased(); },
          .displayFn = [&](const Lot *lot, bool asToolTip) {
              int from = lot->itemYearReleased();
              int to = lot->itemYearLastProduced();
              return !from ? (asToolTip ? u""_qs : u"-"_qs)
                           : ((!to || (to == from)) ? QString::number(from)
                                                    : QString::number(from) + u" - " + QString::number(to));
          },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              return l1->itemYearReleased() <=> l2->itemYearReleased();
          },
      });
    C(Marker, Column {
          .title = QT_TR_NOOP("Marker"),
          .dataFn = [&](const Lot *lot) { return lot->markerText(); },
          .auxDataFn = [&](const Lot *lot) { return lot->markerColor(); },
          .setDataFn = [&](Lot *lot, const QVariant &v) { lot->setMarkerText(v.toString()); },
          .compareFn = [&](const Lot *l1, const Lot *l2) {
              auto d = Utility::naturalCompare(l1->markerText(), l2->markerText());
              return (d != 0) ? d : l1->markerColor().rgba() <=> l2->markerColor().rgba();
          },
      });
    C(DateAdded, Column {
          .type = Column::Type::Date,
          .defaultWidth = 11,
          .editable = false,
          .title = QT_TR_NOOP("Added"),
          .dataFn = [&](const Lot *lot) { return lot->dateAdded(); },
      });
    C(DateLastSold, Column {
          .type = Column::Type::Date,
          .defaultWidth = 11,
          .editable = false,
          .title = QT_TR_NOOP("Last Sold"),
          .dataFn = [&](const Lot *lot) { return lot->dateLastSold(); },
      });
    C(AlternateIds, Column {
          .defaultWidth = 15,
          .editable = false,
          .title = QT_TR_NOOP("Alternate Id"),
          .dataFn = [&](const Lot *lot) { return lot->item() ? QString::fromLatin1(lot->item()->alternateIds()) : QString { }; },
      });
}

void DocumentModel::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item())
        return;

    for (const auto *lot : std::as_const(m_lots)) {
        if ((pic->item() == lot->item()) && (pic->color() == lot->color())) {
            QModelIndex idx = index(const_cast<Lot *>(lot), Picture);
            emitDataChanged(idx, idx);
        }
    }
}

bool DocumentModel::isSorted() const
{
    return m_isSorted;
}

QVector<QPair<int, Qt::SortOrder>> DocumentModel::sortColumns() const
{
    return m_sortColumns;
}

void DocumentModel::multiSort(const QVector<QPair<int, Qt::SortOrder>> &columns)
{
    if (((columns.size() == 1) && (columns.at(0).first == -1)) || (columns == m_sortColumns))
        return;

    if (m_undo) {
        m_undo->push(new SortCmd(this, columns));
    } else {
        bool dummy1;
        LotList dummy2;
        sortDirect(columns, dummy1, dummy2);
    }
}

bool DocumentModel::isFiltered() const
{
    return m_isFiltered;
}

const QVector<Filter> &DocumentModel::filter() const
{
    return m_filter;
}

void DocumentModel::setFilter(const QVector<Filter> &filter)
{
    if (filter == m_filter)
        return;

    if (m_undo) {
        m_undo->push(new FilterCmd(this, filter));
    } else {
        bool dummy1 = false;
        LotList dummy2;
        filterDirect(filter, dummy1, dummy2);
    }
}

void DocumentModel::sortDirect(const QVector<QPair<int, Qt::SortOrder>> &columns, bool &sorted,
                               LotList &unsortedLots)
{
    bool emitSortColumnsChanged = (columns != m_sortColumns);
    bool wasSorted = isSorted();

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    const QModelIndexList before = persistentIndexList();

    m_sortColumns = columns;

    if (!unsortedLots.isEmpty()) {
        m_isSorted = sorted;
        m_sortedLots = unsortedLots;
        unsortedLots.clear();

    } else {
        unsortedLots = m_sortedLots;
        sorted = m_isSorted;
        m_isSorted = true;
        m_sortedLots = m_lots;

        if ((columns.size() != 1) || (columns.at(0).first != -1)) {
            // make the sort deterministic
            auto columnsPlusIndex = columns;
            bool needIndex = true;
            for (const auto &[columnIndex, sortOrder] : columns) {
                if (columnIndex == 0) {
                    needIndex = false;
                    break;
                }
            }
            if (needIndex) {
                columnsPlusIndex.append(qMakePair(0, columns.isEmpty() ? Qt::AscendingOrder
                                                                       : columns.constFirst().second));
            }

            qParallelSort(m_sortedLots.begin(), m_sortedLots.end(),
                          [this, columnsPlusIndex](const auto *lot1, const auto *lot2) {
                std::partial_ordering o = std::partial_ordering::equivalent;
                for (const auto &[columnIndex, sortOrder] : columnsPlusIndex) {
                    const auto &column = m_columns.value(columnIndex);

                    if (column.compareFn) {
                        o = column.compareFn(lot1, lot2);
                    } else if (column.dataFn) {
                        const auto v1 = column.dataFn(lot1);
                        const auto v2 = column.dataFn(lot2);

                        // Qt (as of 6.5.0) does not support operator<=> on QVariant yet
                        switch (column.type) {
                        case Column::Type::String:
                            o = v1.toString().localeAwareCompare(v2.toString()) <=> 0; break;
                        case Column::Type::Integer:
                        case Column::Type::NonLocalizedInteger:
                        case Column::Type::Enum:
                            o = v1.toLongLong() <=> v2.toLongLong(); break;
                        case Column::Type::Currency:
                        case Column::Type::Weight:
                            o = Utility::fuzzyCompare(v1.toDouble(), v2.toDouble()); break;
                        case Column::Type::Date:
                            o = v1.toDateTime().toSecsSinceEpoch() <=> v2.toDateTime().toSecsSinceEpoch(); break;
                        case Column::Type::Special:
                            o = std::partial_ordering::equivalent; break;
                        default:
                            o = std::partial_ordering::unordered; break;
                        }
                    }
                    if (o != 0) {
                        if (sortOrder == Qt::DescendingOrder)
                            o = (o < 0) ? std::partial_ordering::greater : std::partial_ordering::less;
                        break;
                    }
                }
                return o < 0;
            });
        }
    }

    // we were filtered before, but we don't want to re-filter: the solution is to
    // keep the old filtered lots, but use the order from m_sortedLots
    if (!m_filteredLots.isEmpty()
            && (m_filteredLots.size() != m_sortedLots.size())
            && (m_filteredLots != m_sortedLots)) {
        m_filteredLots = QtConcurrent::blockingFiltered(m_sortedLots, [this](auto *lot) {
            return m_filteredLotIndex.contains(lot);
        });
    } else {
        m_filteredLots = m_sortedLots;
    }

    rebuildFilteredLotIndex();

    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &idx : before)
        after.append(index(lot(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    if (emitSortColumnsChanged)
        emit sortColumnsChanged(columns);

    if (isSorted() != wasSorted)
        emit isSortedChanged(isSorted());
}

void DocumentModel::filterDirect(const QVector<Filter> &filter, bool &filtered,
                            LotList &unfilteredLots)
{
    bool emitFilterChanged = (filter != m_filter);
    bool wasFiltered = isFiltered();
    qsizetype filteredSizeBefore = m_filteredLots.size();

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    const QModelIndexList before = persistentIndexList();

    m_filter = filter;

    if (!unfilteredLots.isEmpty()) {
        m_isFiltered = filtered;
        m_filteredLots = unfilteredLots;
        unfilteredLots.clear();

    } else {
        unfilteredLots = m_filteredLots;
        filtered = m_isFiltered;
        m_isFiltered = true;
        m_filteredLots = m_sortedLots;

        if (!filter.isEmpty()) {
            m_filteredLots = QtConcurrent::blockingFiltered(m_sortedLots, [this](auto *lot) {
                return filterAcceptsLot(lot);
            });
        }
    }

    rebuildFilteredLotIndex();

    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &idx : before)
        after.append(index(lot(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);

    if (emitFilterChanged)
        emit filterChanged(filter);

    if (isFiltered() != wasFiltered)
        emit isFilteredChanged(isFiltered());

    qsizetype filteredSizeNow = m_filteredLots.size();
    if (filteredSizeBefore != filteredSizeNow)
        emit filteredLotCountChanged(int(filteredSizeNow));
}

QByteArray DocumentModel::saveSortFilterState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SFST") << qint32(4);

    ds << qint8(m_sortColumns.size());
    for (const auto &[section, order] : m_sortColumns)
        ds << qint8(section) << qint8(order);

    ds << qint8(m_filter.size());
    for (const auto &f : m_filter)
        ds << qint8(f.field()) << qint8(f.comparison()) << qint8(f.combination()) << f.expression();

    ds << qint32(m_sortedLots.size());
    for (const auto &lot : m_sortedLots) {
        qint32 row = qint32(m_lotIndex.value(lot, -1));
        bool visible = m_filteredLotIndex.contains(lot);

        ds << (visible ? row : (-row - 1)); // can't have -0
    }

    return ba;
}

bool DocumentModel::restoreSortFilterState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SFST") || (version < 2) || (version > 4))
        return false;
    QVector<QPair<int, Qt::SortOrder>> sortColumns;
    QVector<Filter> filter;
    qint32 viewSize;

    qint8 sortColumnsSize;
    if (version == 2)
        sortColumnsSize = 1;
    else
        ds >> sortColumnsSize;
    sortColumns.reserve(sortColumnsSize);
    for (int i = 0; i < sortColumnsSize; ++i) {
        qint8 sortColumn, sortOrder;
        ds >> sortColumn >> sortOrder;
        sortColumns.emplace_back(sortColumn, Qt::SortOrder(sortOrder));
    }

    if (version <= 3) {
        QString filterString;
        ds >> filterString;
        filter = m_filterParser->parse(filterString);
    } else {
        qint8 filterSize;
        ds >> filterSize;
        filter.reserve(filterSize);
        for (int i = 0; i < filterSize; ++i) {
            qint8 filterField, filterComparison, filterCombination;
            QString filterExpression;
            ds >> filterField >> filterComparison >> filterCombination >> filterExpression;
            Filter f;
            f.setField(filterField);
            f.setComparison(static_cast<Filter::Comparison>(filterComparison));
            f.setCombination(static_cast<Filter::Combination>(filterCombination));
            f.setExpression(filterExpression);
            filter.append(f);
        }
    }
    ds >> viewSize;
    if ((ds.status() != QDataStream::Ok) || (viewSize != m_lots.size()))
        return false;

    LotList sortedLots;
    LotList filteredLots;
    int lotsSize = int(m_lots.size());
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

    bool willBeSorted = true;
    sortDirect(sortColumns, willBeSorted, sortedLots);
    bool willBeFiltered = true;
    filterDirect(filter, willBeFiltered, filteredLots);
    return true;
}

QStringList DocumentModel::hasDifferenceUpdateWarnings(const BrickLink::LotList &lots)
{
    bool withoutLotId = false;
    bool duplicateLotId = false;
    bool hasDifferences = false;
    QSet<uint> lotIds;
    for (const Lot *lot : lots) {
        const uint lotId = lot->lotId();
        if (!lotId) {
            withoutLotId = true;
        } else {
            if (lotIds.contains(lotId))
                duplicateLotId = true;
            else
                lotIds.insert(lotId);
        }
        if (auto *base = differenceBaseLot(lot)) {
            if (*base != *lot)
                hasDifferences = true;
        }
    }

    QStringList warnings;

    if (!hasDifferences)
        warnings << tr("This document has no differences that could be exported.");
    if (withoutLotId)
        warnings << tr("This list contains items without a BrickLink Lot Id.");
    if (duplicateLotId)
        warnings << tr("This list contains items with duplicate BrickLink Lot Ids.");

    return warnings;
}

// TODO: the Document c'tor needs this to set the initial sort order. find something better
void DocumentModel::sortDirectForDocument(const QVector<QPair<int, Qt::SortOrder> > &columns)
{
    bool dummy1 = false;
    LotList dummy2;
    sortDirect(columns, dummy1, dummy2);
}

QString DocumentModel::filterToolTip() const
{
    return m_filterParser->toolTip();
}

void DocumentModel::reSort()
{
    if (!isSorted())
        m_undo->push(new SortCmd(this, m_sortColumns));
}

void DocumentModel::reFilter()
{
    if (!isFiltered())
        m_undo->push(new FilterCmd(this, m_filter));
}


bool DocumentModel::filterAcceptsLot(const Lot *lot) const
{
    if (!lot)
        return false;
    else if (m_filter.isEmpty())
        return true;

    bool filterResult = false;
    Filter::Combination nextcomb = Filter::Or;

    for (const Filter &f : m_filter) {
        // short circuit
        if (((nextcomb == Filter::And) && !filterResult) || ((nextcomb == Filter::Or) && filterResult)) {
            nextcomb = f.combination();
            continue;
        }

        int firstcol = f.field();
        int lastcol = firstcol;
        if (firstcol < 0) {
            firstcol = 0;
            lastcol = columnCount() - 1;
        }

        bool rowResult = false;
        for (int col = firstcol; col <= lastcol && !rowResult; ++col) {
            const auto field = static_cast<Field>(col);
            const auto &c = m_columns.value(field);

            if (!c.filterable)
                continue;

            bool displayTextMatch = true;
            switch (f.comparison()) {
            case Filter::Is:
            case Filter::IsNot:
            case Filter::Less:
            case Filter::GreaterEqual:
            case Filter::Greater:
            case Filter::LessEqual:
                displayTextMatch = false;
                break;
            default:
                break;
            }

            const QString s1 = f.expression();
            QString s2 = dataForDisplayRole(lot, field, false);

            if (displayTextMatch) {
                // display text based filters

                if (c.type == Column::Type::Enum) {
                    // the display role for enums might be empty
                    qint64 e = dataForEditRole(lot, field).toLongLong();
                    for (const auto &[value, tooltip, filter] : c.enumValues) {
                        if (e == value) {
                            s2 = filter;
                            break;
                        }
                    }
                }

                switch (f.comparison()) {
                case Filter::StartsWith:
                    rowResult = s1.isEmpty() || s2.startsWith(s1, Qt::CaseInsensitive); break;
                case Filter::DoesNotStartWith:
                    rowResult = s1.isEmpty() || !s2.startsWith(s1, Qt::CaseInsensitive); break;
                case Filter::EndsWith:
                    rowResult = s1.isEmpty() || s2.endsWith(s1, Qt::CaseInsensitive); break;
                case Filter::DoesNotEndWith:
                    rowResult = s1.isEmpty() || !s2.endsWith(s1, Qt::CaseInsensitive); break;
                case Filter::Matches:
                case Filter::DoesNotMatch: {
                    if (s1.isEmpty()) {
                        rowResult = true; break;
                    } else if (f.is<QRegularExpression>()) {
                        // We are using QRegularExpressions in multiple threads here, although the class is not
                        // marked thread-safe. We are relying on the const match() function to be thread-safe,
                        // which it currently is up to Qt 6.2.

                        bool res = f.as<QRegularExpression>().match(s2).hasMatch();
                        rowResult = (f.comparison() == Filter::Matches) ? res : !res; break;
                    } else {
                        bool res = s2.contains(s1, Qt::CaseInsensitive);
                        rowResult = (f.comparison() == Filter::Matches) ? res : !res; break;
                    }
                }
                default:
                    break;
                }
            } else {
                // data based filters
                QVariant v = dataForEditRole(lot, field);
                if (v.isNull())
                    v = s2;

                bool isInt = false;
                qint64 i1 = 0, i2 = 0;

                if (c.type == Column::Type::Enum) {
                    i1 = -1;
                    for (const auto &[value, tooltip, filter] : c.enumValues) {
                        if (s1 == filter) {
                            i1 = value;
                            break;
                        }
                    }
                    i2 = v.toLongLong();
                    isInt = true;

                } else {
                    switch (v.userType()) {
                    case QMetaType::Int:
                    case QMetaType::UInt:
                    case QMetaType::LongLong:
                    case QMetaType::ULongLong: {
                        if (f.is<int>()) {
                            i1 = f.as<int>();
                            i2 = v.toInt();
                            isInt = true;
                        }
                        break;
                    }
                    case QMetaType::Double: {
                        if (f.is<double>()) {
                            i1 = qRound64(f.as<double>() * 1000.);
                            i2 = qRound64(v.toDouble() * 1000.);
                            isInt = true;
                        }
                        break;
                    }
                    case QMetaType::QDateTime: {
                        if (auto dt = f.as<QDateTime>(); dt.isValid()) {
                            if (f.is<QDate>()) {
                                // just compare the date, not the time
                                i1 = dt.toLocalTime().date().toJulianDay();
                                i2 = v.toDateTime().toLocalTime().date().toJulianDay();
                            } else {
                                // round down to the nearest minute
                                i1 = dt.addSecs(-dt.time().second()).toSecsSinceEpoch();
                                i2 = v.toDateTime().addSecs(-v.toDateTime().time().second()).toSecsSinceEpoch();
                            }
                            isInt = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                switch (f.comparison()) {
                case Filter::Is:
                    rowResult = isInt ? i2 == i1 : s2.compare(s1, Qt::CaseInsensitive) == 0; break;
                case Filter::IsNot:
                    rowResult = isInt ? i2 != i1 : s2.compare(s1, Qt::CaseInsensitive) != 0; break;
                case Filter::Less:
                    rowResult = isInt ? i2 < i1 : false; break;
                case Filter::LessEqual:
                    rowResult = isInt ? i2 <= i1 : false; break;
                case Filter::Greater:
                    rowResult = isInt ? i2 > i1 : false; break;
                case Filter::GreaterEqual:
                    rowResult = isInt ? i2 >= i1 : false; break;
                default:
                    break;
                }
            }
        }
        if (nextcomb == Filter::And)
            filterResult = filterResult && rowResult;
        else
            filterResult = filterResult || rowResult;

        nextcomb = f.combination();
    }
    return filterResult;
}

bool DocumentModel::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QAbstractTableModel::event(e);
}

void DocumentModel::languageChange()
{
    m_filterParser->setStandardCombinationTokens(Filter::And | Filter::Or);
    m_filterParser->setStandardComparisonTokens(Filter::Matches | Filter::DoesNotMatch |
                                                Filter::Is | Filter::IsNot |
                                                Filter::Less | Filter::LessEqual |
                                                Filter::Greater | Filter::GreaterEqual |
                                                Filter::StartsWith | Filter::DoesNotStartWith |
                                                Filter::EndsWith | Filter::DoesNotEndWith);

    QVector<QPair<int, QString>> fields;
    fields.append({ -1, tr("Any") });
    QString str;
    for (int i = 0; i < columnCount(); ++i) {
        str = headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!str.isEmpty())
            fields.append({ i, str });
    }

    m_filterParser->setFieldTokens(fields);
}

LotList DocumentModel::sortLotList(const LotList &list) const
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


const QString DocumentLotsMimeData::s_mimetype = u"application/x-bricklink-invlots"_qs;

DocumentLotsMimeData::DocumentLotsMimeData(const LotList &lots, const QString &currencyCode)
    : QMimeData()
{
    setLots(lots, currencyCode);
}

void DocumentLotsMimeData::setLots(const LotList &lots, const QString &currencyCode)
{
    QByteArray data;
    QString text;

    QDataStream ds(&data, QIODevice::WriteOnly);
    ds << QByteArray("LOTS") << qint32(2);

    ds << currencyCode << BrickLink::core()->latestChangelogId() << quint32(lots.count());
    for (const Lot *lot : lots) {
        lot->save(ds);
        if (!text.isEmpty())
            text.append(u"\n"_qs);
        text.append(QString::fromLatin1(lot->itemId()));
    }
    setText(text);
    setData(s_mimetype, data);
}

std::tuple<LotList, QString> DocumentLotsMimeData::lots(const QMimeData *md)
{
    LotList lots;
    QString currencyCode;

    if (md) {
        QByteArray data = md->data(s_mimetype);
        QDataStream ds(data);
        uint startChangelogAt = 0;

        QByteArray tag;
        qint32 version;
        ds >> tag >> version;
        if ((ds.status() != QDataStream::Ok) || (tag != "LOTS") || (version != 2))
            return { };

        quint32 count = 0;
        ds >> currencyCode >> startChangelogAt >> count;

        if ((ds.status() != QDataStream::Ok) || (currencyCode.size() != 3) || (count > 1000000))
            return { };

        lots.reserve(count);
        for (; count && !ds.atEnd(); count--) {
            if (auto lot = Lot::restore(ds, startChangelogAt))
                lots << lot;
        }
    }
    return { lots, currencyCode };
}

QStringList DocumentLotsMimeData::formats() const
{
    static QStringList sl;

    if (sl.isEmpty())
        sl << s_mimetype << u"text/plain"_qs;

    return sl;
}

bool DocumentLotsMimeData::hasFormat(const QString &mimeType) const
{
    return mimeType.compare(s_mimetype) || mimeType.compare(u"text/plain"_qs);
}


#include "moc_documentmodel.cpp"
