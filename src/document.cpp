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

#include <QApplication>
#include <QCursor>
#include <QFileInfo>
#include <QDir>
#include <QStringBuilder>

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


enum {
    CID_Change,
    CID_AddRemove,
    CID_Currency
};


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


ChangeCmd::ChangeCmd(Document *doc, int pos, const Document::Item &item, bool merge_allowed)
    : QUndoCommand(qApp->translate("ChangeCmd", "Modified item")), m_doc(doc), m_position(pos), m_item(item), m_merge_allowed(merge_allowed)
{ }

int ChangeCmd::id() const
{
    return CID_Change;
}

void ChangeCmd::redo()
{
    m_doc->changeItemDirect(m_position, m_item);
}

void ChangeCmd::undo()
{
    redo();
}

bool ChangeCmd::mergeWith(const QUndoCommand *other)
{
#if 0 // untested
    const ChangeCmd *that = static_cast <const ChangeCmd *>(other);

    if ((m_merge_allowed && that->m_merge_allowed) &&
        (m_doc == that->m_doc) &&
        (m_position == that->m_position))
    {
        m_item = that->m_item;
        return true;
    }
#else
    Q_UNUSED(other)
    Q_UNUSED(m_merge_allowed)
#endif
    return false;
}


AddRemoveCmd::AddRemoveCmd(Type t, Document *doc, const QVector<int> &positions, const Document::ItemList &items, bool merge_allowed)
    : QUndoCommand(genDesc(t == Add, qMax(items.count(), positions.count()))),
      m_doc(doc), m_positions(positions), m_items(items), m_type(t), m_merge_allowed(merge_allowed)
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
        // Document::insertItemsDirect() adds all m_items at the positions given in m_positions
        // (or append them to the document in case m_positions is empty)
        m_doc->insertItemsDirect(m_items, m_positions);
        m_positions.clear();
        m_type = Remove;
    }
    else {
        // Document::removeItemsDirect() removes all m_items and records the positions in m_positions
        m_doc->removeItemsDirect(m_items, m_positions);
        m_type = Add;
    }
}

void AddRemoveCmd::undo()
{
    redo();
}

bool AddRemoveCmd::mergeWith(const QUndoCommand *other)
{
    const auto *that = static_cast <const AddRemoveCmd *>(other);

    if ((m_merge_allowed && that->m_merge_allowed) &&
        (m_doc == that->m_doc) &&
        (m_type == that->m_type)) {
        m_items     += that->m_items;
        m_positions += that->m_positions;
        setText(genDesc(m_type == Remove, qMax(m_items.count(), m_positions.count())));

        const_cast<AddRemoveCmd *>(that)->m_items.clear();
        const_cast<AddRemoveCmd *>(that)->m_positions.clear();
        return true;
    }
    return false;
}

QString AddRemoveCmd::genDesc(bool is_add, int count)
{
    if (is_add)
        return Document::tr("Added %n item(s)", nullptr, count);
    else
        return Document::tr("Removed %n item(s)", nullptr, count);
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

Document::Statistics::Statistics(const Document *doc, const ItemList &list, bool ignoreExcluded)
{
    m_lots = 0;
    m_items = 0;
    m_val = m_minval = m_cost = 0.;
    m_weight = .0;
    m_errors = 0;
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

        if (quint64 errors = doc->itemFlags(item).first) {
            for (quint64 i = 1ULL << (FieldCount - 1); i;  i >>= 1) {
                if (errors & i)
                    m_errors++;
            }
        }

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

    doc->setBrickLinkItems(items); // setBrickLinkItems owns the items now
    doc->setFakeIndexes(fakeIndexes);
    return doc;
}

Document::Document(int /*is temporary*/)
    : m_currencycode(Config::inst()->defaultCurrencyCode())
{
    MODELTEST_ATTACH(this)

    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            this, &Document::pictureUpdated);
}

// the caller owns the items
Document::Document(const BrickLink::InvItemList &items, const QString &currencyCode)
    : Document(0)
{
    // we take ownership of the items

    if (!currencyCode.isEmpty())
        m_currencycode = currencyCode;

    m_undo = new UndoStack(this);
    connect(m_undo, &QUndoStack::cleanChanged,
            this, [this](bool clean) {
        // if the gui state is modified, the overall state stays at "modified"
        if (!m_gui_state_modified)
            emit modificationChanged(!clean);
    });

    setBrickLinkItems(items); // setBrickLinkItems owns the items now

    s_documents.append(this);
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

Document::Statistics Document::statistics(const ItemList &list) const
{
    return Statistics(this, list);
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
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, QVector<int>(), m_items));
    return true;
}

int Document::positionOf(Item *item) const
{
    return m_items.indexOf(item);
}

Document::Item *Document::itemAt(int position)
{
    return (position >= 0 && position < m_items.count()) ? m_items.at(position) : nullptr;
}

bool Document::insertItems(const QVector<int> &positions, const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, items /*, true*/));
    return true;
}

bool Document::removeItem(Item *item)
{
    return removeItems({ item });
}

bool Document::removeItems(const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, QVector<int>(), items /*, true*/));
    return true;
}

bool Document::appendItem(Item *item)
{
    return insertItems({ }, { item });
}

bool Document::changeItem(Item *item, const Item &value)
{
    return changeItem(positionOf(item), value);
}

bool Document::changeItem(int position, const Item &value)
{
    m_undo->push(new ChangeCmd(this, position, value /*, true*/));
    return true;
}

void Document::insertItemsDirect(const ItemList &items, QVector<int> &positions)
{
    auto pos = positions.constBegin();
    bool single = (items.count() == 1);
    QModelIndexList before;
    QModelIndex root;

    if (!single) {
        emit layoutAboutToBeChanged();
        before = persistentIndexList();
    }

    for (Item *item : qAsConst(items)) {
        int rows = rowCount(root);

        if (pos != positions.constEnd()) {
            if (single)
                beginInsertRows(root, *pos, *pos);
            m_items.insert(*pos, item);
            ++pos;
        } else {
            if (single)
                beginInsertRows(root, rows, rows);
            m_items.append(item);
        }

        // this is really a new item, not just a redo - start with no differences
        if (!m_differenceBase.contains(item))
            m_differenceBase.insert(item, *item);

        updateItemFlags(item);
        if (single)
            endInsertRows();
    }

    if (!single) {
        QModelIndexList after;
        foreach (const QModelIndex &idx, before)
            after.append(index(item(idx), idx.column()));
        changePersistentIndexList(before, after);
        emit layoutChanged();
    }

    emit itemCountChanged(m_items.count());
    emitStatisticsChanged();
}

void Document::removeItemsDirect(ItemList &items, QVector<int> &positions)
{
    positions.resize(items.count());

    bool single = (items.count() == 1);
    QModelIndexList before;

    if (!single) {
        emit layoutAboutToBeChanged();
        before = persistentIndexList();
    }

    for (int i = items.count() - 1; i >= 0; --i) {
        Item *item = items[i];
        int idx = m_items.indexOf(item);
        if (single)
            beginRemoveRows(QModelIndex(), idx, idx);
        positions[i] = idx;
        m_items.removeAt(idx);
        if (single)
            endRemoveRows();
    }

    if (!single) {
        QModelIndexList after;
        foreach (const QModelIndex &idx, before)
            after.append(index(item(idx), idx.column()));
        changePersistentIndexList(before, after);
        emit layoutChanged();
    }

    emit itemCountChanged(m_items.count());
    emitStatisticsChanged();
}

void Document::changeItemDirect(int position, Item &item)
{
    Item *olditem = m_items[position];
    std::swap(*olditem, item);

    QModelIndex idx1 = index(olditem);
    QModelIndex idx2 = createIndex(idx1.row(), columnCount(idx1.parent()) - 1, idx1.internalPointer());

    emitDataChanged(idx1, idx2);
    updateItemFlags(olditem);
    emitStatisticsChanged();
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
    }
    emit currencyCodeChanged(m_currencycode);
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

    if (isDifferenceModeActive()) {
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
                if (dataForFilterRole(item, f, -1) != dataForFilterRole(base, f, -1))
                    updated |= fmask;
            }
        }
    }

    setItemFlags(item, errors, updated);
}

bool Document::isDifferenceModeActive() const
{
    return m_differenceModeActive;
}

bool Document::startDifferenceModeInternal(const QHash<const Item *, Item> &updateBase)
{
    m_differenceBase = updateBase;
    m_differenceModeActive = true;

    for (const BrickLink::InvItem *i : qAsConst(m_items))
        updateItemFlags(i);

    emit differenceModeChanged(true);
    emitDataChanged();
    return true;
}

bool Document::startDifferenceMode()
{
    if (m_differenceModeActive || m_items.isEmpty())
        return false;

    m_differenceBase.clear();
    decltype(m_differenceBase) base;

    for (const BrickLink::InvItem *i : qAsConst(m_items))
        base.insert(i, *i);

    return startDifferenceModeInternal(base);
}

void Document::endDifferenceMode()
{
    if (m_differenceModeActive) {
        m_differenceBase.clear();
        m_differenceModeActive = false;

        for (const BrickLink::InvItem *i : qAsConst(m_items))
            updateItemFlags(i);

        emit differenceModeChanged(false);
        emitDataChanged();
    }
}

const Document::Item *Document::differenceBaseItem(const Document::Item *item) const
{
    if (!item || !isDifferenceModeActive())
        return nullptr;

    auto it = m_differenceBase.constFind(item);
    return (it != m_differenceBase.end()) ? &(*it) : nullptr;
}

QString Document::currencyCode() const
{
    return m_currencycode;
}

void Document::setCurrencyCode(const QString &ccode, qreal crate)
{
    if (ccode != m_currencycode)
        m_undo->push(new CurrencyCmd(this, ccode, crate));
}

bool Document::hasGuiState() const
{
    return !m_gui_state.isNull();
}

QDomElement Document::guiState() const
{
    return m_gui_state;
}

void Document::setGuiState(QDomElement dom)
{
    m_gui_state = dom;
}

void Document::clearGuiState()
{
    m_gui_state.clear();
}

void Document::setGuiStateModified(bool modified)
{
    bool wasModified = isModified();
    m_gui_state_modified = modified;
    if (wasModified != isModified())
        emit modificationChanged(!wasModified);
}

void Document::setOrder(BrickLink::Order *order)
{
    if (m_order)
        delete m_order;
    m_order = order;
}


void Document::setBrickLinkItems(const BrickLink::InvItemList &items)
{
    //TODO: switch to std::unique_ptr and remove this

    // we take ownership of the items
    QVector<int> pos;
    insertItemsDirect(items, pos);
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
    return !m_undo->isClean() || m_gui_state_modified;
}

void Document::unsetModified()
{
    m_undo->setClean();

    // at this point, we can cleanup the diff mode base
    QSet goneItems = m_differenceBase.keys().toSet();
    for (const auto &item : qAsConst(m_items))
        goneItems.remove(item);
    for (const auto &item : qAsConst(goneItems))
        m_differenceBase.remove(item);
}

quint64 Document::errorMask() const
{
    return m_error_mask;
}

void Document::setErrorMask(quint64 em)
{
    m_error_mask = em;
    emitStatisticsChanged();
    emitDataChanged();
}

QPair<quint64, quint64> Document::itemFlags(const Item *item) const
{
    auto flags = m_itemFlags.value(item, { });
    flags.first &= m_error_mask;
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
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, m_items.at(row));
    return {};
}

Document::Item *Document::item(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<Item *>(idx.internalPointer()) : nullptr;
}

QModelIndex Document::index(const Item *ci, int column) const
{
    Item *i = const_cast<Item *>(ci);

    return i ? createIndex(m_items.indexOf(i), column, i) : QModelIndex();
}


int Document::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : items().size();
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
    case LotId       : break;
    case Retain      : ifs |= Qt::ItemIsUserCheckable; Q_FALLTHROUGH();
    default          : ifs |= Qt::ItemIsEditable; break;
    }
    return ifs;
}

bool Document::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        Item *itemp = items().at(index.row());
        Item item = *itemp;
        auto f = static_cast<Field>(index.column());

        switch (f) {
        case Document::PartNo      : {
            char itid = item.itemType() ? item.itemType()->id() : 'P';
            const BrickLink::Item *newitem = BrickLink::core()->item(itid, value.toString().toLatin1().constData());
            if (newitem)
                item.setItem(newitem);
            break;
        }
        case Comments    : item.setComments(value.toString()); break;
        case Remarks     : item.setRemarks(value.toString()); break;
        case Reserved    : item.setReserved(value.toString()); break;
        case Sale        : item.setSale(value.toInt()); break;
        case Bulk        : item.setBulkQuantity(value.toInt()); break;
        case TierQ1      : item.setTierQuantity(0, value.toInt()); break;
        case TierQ2      : item.setTierQuantity(1, value.toInt()); break;
        case TierQ3      : item.setTierQuantity(2, value.toInt()); break;
        case TierP1      : item.setTierPrice(0, Currency::fromString(value.toString())); break;
        case TierP2      : item.setTierPrice(1, Currency::fromString(value.toString())); break;
        case TierP3      : item.setTierPrice(2, Currency::fromString(value.toString())); break;
        case Weight      : item.setTotalWeight(Utility::stringToWeight(value.toString(), Config::inst()->measurementSystem())); break;
        case Quantity    : item.setQuantity(value.toInt()); break;
        case QuantityDiff: {
            if (auto base = differenceBaseItem(itemp))
                item.setQuantity(base->quantity() + value.toInt());
            break;
        }
        case Price       : item.setPrice(Currency::fromString(value.toString())); break;
        case PriceDiff   :  {
            if (auto base = differenceBaseItem(itemp))
                item.setPrice(base->price() + Currency::fromString(value.toString()));
            break;
        }
        case Cost        : item.setCost(Currency::fromString(value.toString())); break;
        default          : break;
        }
        if (item != *itemp) {
            changeItem(index.row(), item);
            emitDataChanged(index, index);
            return true;
        }
    }
    return false;
}


QVariant Document::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        Item *it = items().at(index.row());
        auto f = static_cast<Field>(index.column());

        switch (role) {
        case Qt::DisplayRole      : return dataForDisplayRole(it, f, index.row());
        case Qt::DecorationRole   : return dataForDecorationRole(it, f);
        case Qt::ToolTipRole      : return dataForToolTipRole(it, f, index.row());
        case Qt::TextAlignmentRole: return dataForTextAlignmentRole(it, f);
        case Qt::EditRole         : return dataForEditRole(it, f);
        case Qt::CheckStateRole   : return dataForCheckStateRole(it, f);
        case Document::FilterRole : return dataForFilterRole(it, f, index.row());
        }
    }
    return QVariant();
}

QVariant Document::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        auto f = static_cast<Field>(section);

        switch (role) {
        case Qt::DisplayRole      : return headerDataForDisplayRole(f);
        case Qt::TextAlignmentRole: return headerDataForTextAlignmentRole(f);
        case Qt::UserRole         : return headerDataForDefaultWidthRole(f);
        }
    }
    return QVariant();
}

QVariant Document::dataForEditRole(const Item *it, Field f) const
{
    switch (f) {
    case PartNo      : return it->itemId();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case Reserved    : return it->reserved();
    case Sale        : return it->sale();
    case Bulk        : return it->bulkQuantity();
    case TierQ1      : return it->tierQuantity(0);
    case TierQ2      : return it->tierQuantity(1);
    case TierQ3      : return it->tierQuantity(2);
    case TierP1      : return Currency::toString(it->tierPrice(0));
    case TierP2      : return Currency::toString(it->tierPrice(1));
    case TierP3      : return Currency::toString(it->tierPrice(2));
    case Weight      : return Utility::weightToString(it->totalWeight(), Config::inst()->measurementSystem(), false);
    case Quantity    : return it->quantity();
    case QuantityDiff:  {
        auto base = differenceBaseItem(it);
        return base ? it->quantity() - base->quantity() : 0;
    }
    case Price       : return Currency::toString(it->price());
    case PriceDiff   : {
        auto base = differenceBaseItem(it);
        return Currency::toString(base ? it->price() - base->price() : 0);
    }
    case Cost        : return Currency::toString(it->cost());
    default          : return QString();
    }
}

QString Document::dataForDisplayRole(const Item *it, Field f, int row) const
{
    QString dash = QLatin1String("-");

    switch (f) {
    case Index       :
        if (m_fakeIndexes.isEmpty()) {
            return QString::number(row + 1);
        } else {
            auto fi = m_fakeIndexes.at(row);
            return fi >= 0 ? QString::number(fi + 1) : QString::fromLatin1("+");
        }
    case LotId       : return (it->lotId() == 0 ? dash : QString::number(it->lotId()));
    case PartNo      : return it->itemId();
    case Description : return it->itemName();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case Quantity    : return QString::number(it->quantity());
    case Bulk        : return (it->bulkQuantity() == 1 ? dash : QString::number(it->bulkQuantity()));
    case Price       : return Currency::toString(it->price());
    case Total       : return Currency::toString(it->total());
    case Sale        : return (it->sale() == 0 ? dash : QString::number(it->sale()) + QLatin1Char('%'));
    case Condition   : {
        QString c = (it->condition() == BrickLink::Condition::New) ? tr("N", "List>Cond>New")
                                                                   : tr("U", "List>Cond>Used");
        if (it->itemType() && it->itemType()->hasSubConditions()
                && (it->subCondition() != BrickLink::SubCondition::None)) {
            c = c % u" / " % subConditionLabel(it->subCondition());
        }
        return c;
    }
    case Color       : return it->colorName();
    case Category    : return it->category() ? it->category()->name() : dash;
    case ItemType    : return it->itemType() ? it->itemType()->name() : dash;
    case TierQ1      : return (it->tierQuantity(0) == 0 ? dash : QString::number(it->tierQuantity(0)));
    case TierQ2      : return (it->tierQuantity(1) == 0 ? dash : QString::number(it->tierQuantity(1)));
    case TierQ3      : return (it->tierQuantity(2) == 0 ? dash : QString::number(it->tierQuantity(2)));
    case TierP1      : return Currency::toString(it->tierPrice(0));
    case TierP2      : return Currency::toString(it->tierPrice(1));
    case TierP3      : return Currency::toString(it->tierPrice(2));
    case Reserved    : return it->reserved();
    case Weight      : return qFuzzyIsNull(it->totalWeight()) ? dash : Utility::weightToString(it->totalWeight(), Config::inst()->measurementSystem(), true, true);
    case YearReleased: return (it->itemYearReleased() == 0) ? dash : QString::number(it->itemYearReleased());

    case PriceOrig   : {
        auto base = differenceBaseItem(it);
        return Currency::toString(base ? base->price() : 0);
    }
    case PriceDiff   : {
        auto base = differenceBaseItem(it);
        return Currency::toString(base ? it->price() - base->price() : 0);
    }
    case Cost        : return Currency::toString(it->cost());
    case QuantityOrig: {
        auto base = differenceBaseItem(it);
        return QString::number(base ? base->quantity() : 0);
    }
    case QuantityDiff: {
        auto base = differenceBaseItem(it);
        return QString::number(base ? it->quantity() - base->quantity() : 0);
    }
    default          : return QString();
    }
}

QVariant Document::dataForFilterRole(const Item *it, Field f, int row) const
{
    switch (f) {
    case Status:
        switch (it->status()) {
        case BrickLink::Status::Include: return tr("I", "Filter>Status>Include"); break;
        case BrickLink::Status::Extra  : return tr("X", "Filter>Status>Extra"); break;
        default:
        case BrickLink::Status::Exclude: return tr("E", "Filter>Status>Exclude"); break;
        }
    case Stockroom:
        switch (it->stockroom()) {
        case BrickLink::Stockroom::A: return QString("A");
        case BrickLink::Stockroom::B: return QString("B");
        case BrickLink::Stockroom::C: return QString("C");
        default                     : return QString("-");
        }
    case Index       : return row + 1;
    case Retain      : return it->retain() ? tr("Y", "Filter>Retain>Yes")
                                           : tr("N", "Filter>Retain>No");
    case Price       : return it->price();
    case PriceDiff   :  {
        auto base = differenceBaseItem(it);
        return base ? it->price() - base->price() : 0;
    }
    case Cost        : return it->cost();
    case TierP1      : return it->tierPrice(0);
    case TierP2      : return it->tierPrice(1);
    case TierP3      : return it->tierPrice(2);

    default: {
        QVariant v = dataForEditRole(it, f);
        if (v.isNull())
            v = dataForDisplayRole(it, f, row);
        return v;
    }
    }
}

QVariant Document::dataForDecorationRole(const Item *it, Field f) const
{
    switch (f) {
    case Picture: return it->image();
    default     : return QPixmap();
    }
}

Qt::CheckState Document::dataForCheckStateRole(const Item *it, Field f) const
{
    switch (f) {
    case Retain   : return it->retain() ? Qt::Checked : Qt::Unchecked;
    default       : return Qt::Unchecked;
    }
}

int Document::dataForTextAlignmentRole(const Item *, Field f) const
{
    switch (f) {
    case Status      :
    case Retain      :
    case Stockroom   :
    case Picture     :
    case Condition   : return Qt::AlignVCenter | Qt::AlignHCenter;
    case Index       :
    case LotId       :
    case PriceOrig   :
    case PriceDiff   :
    case Cost        :
    case Price       :
    case Total       :
    case Sale        :
    case TierP1      :
    case TierP2      :
    case TierP3      :
    case Quantity    :
    case Bulk        :
    case QuantityOrig:
    case QuantityDiff:
    case TierQ1      :
    case TierQ2      :
    case TierQ3      :
    case YearReleased:
    case Weight      : return Qt::AlignRight | Qt::AlignVCenter;
    default          : return Qt::AlignLeft | Qt::AlignVCenter;
    }
}

QString Document::dataForToolTipRole(const Item *it, Field f, int row) const
{
    switch (f) {
    case Status: {
        QString str;
        switch (it->status()) {
        case BrickLink::Status::Exclude: str = tr("Exclude"); break;
        case BrickLink::Status::Extra  : str = tr("Extra"); break;
        case BrickLink::Status::Include: str = tr("Include"); break;
        default                : break;
        }
        if (it->counterPart())
            str += QLatin1String("\n(") + tr("Counter part") + QLatin1Char(')');
        else if (it->alternateId())
            str += QLatin1String("\n(") + tr("Alternate match id: %1").arg(it->alternateId()) + QLatin1Char(')');
        return str;
    }
    case Picture: {
        return dataForDisplayRole(it, PartNo, row) + QLatin1Char(' ') + dataForDisplayRole(it, Description, row);
    }
    case Condition: {
        QString c = (it->condition() == BrickLink::Condition::New) ? tr("New") : tr("Used");
        if (it->itemType() && it->itemType()->hasSubConditions()
                && (it->subCondition() != BrickLink::SubCondition::None)) {
            c = c % u" / " % subConditionLabel(it->subCondition());
        }
        return c;
    }
    case Category: {
        if (!it->item())
            break;

        const auto allcats = it->item()->allCategories();

        if (allcats.size() == 1) {
            return allcats[0]->name();
        }
        else {
            QString str = QLatin1String("<b>") + allcats[0]->name() + QLatin1String("</b>");
            for (int i = 1; i < allcats.size(); ++i)
                str = str + QLatin1String("<br />") + allcats[i]->name();
            return str;
        }
    }
    default: {
        return dataForDisplayRole(it, f, row);
    }
    }
    return QString();
}


QString Document::headerDataForDisplayRole(Field f)
{
    switch (f) {
    case Index       : return tr("Index");
    case Status      : return tr("Status");
    case Picture     : return tr("Image");
    case PartNo      : return tr("Part #");
    case Description : return tr("Description");
    case Comments    : return tr("Comments");
    case Remarks     : return tr("Remarks");
    case QuantityOrig: return tr("Qty.Orig");
    case QuantityDiff: return tr("Qty.Diff");
    case Quantity    : return tr("Qty.");
    case Bulk        : return tr("Bulk");
    case PriceOrig   : return tr("Pr.Orig");
    case PriceDiff   : return tr("Pr.Diff");
    case Cost        : return tr("Cost");
    case Price       : return tr("Price");
    case Total       : return tr("Total");
    case Sale        : return tr("Sale");
    case Condition   : return tr("Cond.");
    case Color       : return tr("Color");
    case Category    : return tr("Category");
    case ItemType    : return tr("Item Type");
    case TierQ1      : return tr("Tier Q1");
    case TierP1      : return tr("Tier P1");
    case TierQ2      : return tr("Tier Q2");
    case TierP2      : return tr("Tier P2");
    case TierQ3      : return tr("Tier Q3");
    case TierP3      : return tr("Tier P3");
    case LotId       : return tr("Lot Id");
    case Retain      : return tr("Retain");
    case Stockroom   : return tr("Stockroom");
    case Reserved    : return tr("Reserved");
    case Weight      : return tr("Weight");
    case YearReleased: return tr("Year");
    default          : return QString();
    }
}

int Document::headerDataForTextAlignmentRole(Field f) const
{
    return dataForTextAlignmentRole(nullptr, f);
}

int Document::headerDataForDefaultWidthRole(Field f) const
{
    int width = 0;
    QSize picsize = BrickLink::core()->standardPictureSize();

    switch (f) {
    case Index       : width = 3; break;
    case Status      : width = 6; break;
    case Picture     : width = -picsize.width(); break;
    case PartNo      : width = 10; break;
    case Description : width = 28; break;
    case Comments    : width = 8; break;
    case Remarks     : width = 8; break;
    case QuantityOrig: width = 5; break;
    case QuantityDiff: width = 5; break;
    case Quantity    : width = 5; break;
    case Bulk        : width = 5; break;
    case PriceOrig   : width = 8; break;
    case PriceDiff   : width = 8; break;
    case Cost        : width = 8; break;
    case Price       : width = 8; break;
    case Total       : width = 8; break;
    case Sale        : width = 5; break;
    case Condition   : width = 5; break;
    case Color       : width = 15; break;
    case Category    : width = 12; break;
    case ItemType    : width = 12; break;
    case TierQ1      : width = 5; break;
    case TierP1      : width = 8; break;
    case TierQ2      : width = 5; break;
    case TierP2      : width = 8; break;
    case TierQ3      : width = 5; break;
    case TierP3      : width = 8; break;
    case LotId       : width = 8; break;
    case Retain      : width = 8; break;
    case Stockroom   : width = 8; break;
    case Reserved    : width = 8; break;
    case Weight      : width = 8; break;
    case YearReleased: width = 5; break;
    default          : break;
    }
    return width;
}


void Document::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item())
        return;

    int row = 0;
    for (const Item *it : qAsConst(m_items)) {
        if ((pic->item() == it->item()) && (pic->color() == it->color())) {
            QModelIndex idx = index(row, Picture);
            emitDataChanged(idx, idx);
        }
        row++;
    }
}


QString Document::subConditionLabel(BrickLink::SubCondition sc) const
{
    switch (sc) {
    case BrickLink::SubCondition::None      : return tr("-", "no subcondition");
    case BrickLink::SubCondition::Sealed    : return tr("Sealed");
    case BrickLink::SubCondition::Complete  : return tr("Complete");
    case BrickLink::SubCondition::Incomplete: return tr("Incomplete");
    default                                 : return QString();
    }
}




DocumentProxyModel::DocumentProxyModel(Document *model, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_lastSortColumn[0] = m_lastSortColumn[1] = -1;

    setDynamicSortFilter(false);
    setSourceModel(model);

    m_parser = new Filter::Parser();

    languageChange();
}

DocumentProxyModel::~DocumentProxyModel()
{
    delete m_parser;
}

void DocumentProxyModel::setFilterExpression(const QString &str)
{
    if (str == m_filter_expression)
        return;

    bool had_filter = !m_filter.isEmpty();

    m_filter_expression = str;
    m_filter = m_parser->parse(str);

    if (had_filter || !m_filter.isEmpty())
        invalidateFilter();

    emit filterExpressionChanged(str);
}

QString DocumentProxyModel::filterExpression() const
{
    return m_filter_expression;
}

QString DocumentProxyModel::filterToolTip() const
{
    return m_parser->toolTip();
}

bool DocumentProxyModel::filterAcceptsColumn(int, const QModelIndex &) const
{
    return true;
}

bool DocumentProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid() || source_row < 0 || source_row >= sourceModel()->rowCount())
        return false;
    else if (m_filter.isEmpty())
        return true;

    bool result = false;
    Filter::Combination nextcomb = Filter::Or;

    for (const Filter &f : m_filter) {
        int firstcol = f.field();
        int lastcol = firstcol;
        if (firstcol < 0) {
            firstcol = 0;
            lastcol = sourceModel()->columnCount() - 1;
        }

        bool localresult = false;
        for (int c = firstcol; c <= lastcol && !localresult; ++c) {
            QVariant v = sourceModel()->data(sourceModel()->index(source_row, c), Document::FilterRole);
            if (v.isNull())
                v = sourceModel()->data(sourceModel()->index(source_row, c), Qt::DisplayRole).toString();
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

bool DocumentProxyModel::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QSortFilterProxyModel::event(e);
}

void DocumentProxyModel::languageChange()
{
    auto model = sourceModel();

    m_parser->setStandardCombinationTokens(Filter::And | Filter::Or);
    m_parser->setStandardComparisonTokens(Filter::Matches | Filter::DoesNotMatch |
                                          Filter::Is | Filter::IsNot |
                                          Filter::Less | Filter::LessEqual |
                                          Filter::Greater | Filter::GreaterEqual |
                                          Filter::StartsWith | Filter::DoesNotStartWith |
                                          Filter::EndsWith | Filter::DoesNotEndWith);

    QMultiMap<int, QString> fields;
    QString str;
    for (int i = 0; i < model->columnCount(QModelIndex()); ++i) {
        str = model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!str.isEmpty())
            fields.insert(i, str);
    }
    fields.insert(-1, tr("Any"));

    m_parser->setFieldTokens(fields);
}

void DocumentProxyModel::sort(int column, Qt::SortOrder order)
{
    if (column == -1) {
        m_lastSortColumn[0] = m_lastSortColumn[1] = -1;
    } else {
        m_lastSortColumn[1] = m_lastSortColumn[0];
        m_lastSortColumn[0] = sortColumn();
    }

    QSortFilterProxyModel::sort(column, order);
}

bool DocumentProxyModel::lessThan(const QModelIndex &idx1, const QModelIndex &idx2) const
{
    const Document::Item *i1 = static_cast<Document *>(sourceModel())->item(idx1);
    const Document::Item *i2 = static_cast<Document *>(sourceModel())->item(idx2);

    int result = compare(i1, i2, sortColumn());
    if (!result && m_lastSortColumn[0] > -1)
        result = compare(i1, i2, m_lastSortColumn[0]);
    if (!result && m_lastSortColumn[1] > -1)
        result = compare(i1, i2, m_lastSortColumn[1]);
    return result < 0;
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

int DocumentProxyModel::compare(const Document::Item *i1, const Document::Item *i2, int sortColumn) const
{
    switch (sortColumn) {
    case Document::Index       : return static_cast<Document *>(sourceModel())->index(i1).row()
                                        - static_cast<Document *>(sourceModel())->index(i2).row();
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
        auto base1 = static_cast<Document *>(sourceModel())->differenceBaseItem(i1);
        auto base2 = static_cast<Document *>(sourceModel())->differenceBaseItem(i2);
        return doubleCompare(base1 ? base1->price() : 0, base2 ? base2->price() : 0);
    }
    case Document::PriceDiff   : {
        auto base1 = static_cast<Document *>(sourceModel())->differenceBaseItem(i1);
        auto base2 = static_cast<Document *>(sourceModel())->differenceBaseItem(i2);
        return doubleCompare(base1 ? i1->price() - base1->price() : 0,
                             base2 ? i2->price() - base2->price() : 0);
    }
    case Document::QuantityOrig: {
        auto base1 = static_cast<Document *>(sourceModel())->differenceBaseItem(i1);
        auto base2 = static_cast<Document *>(sourceModel())->differenceBaseItem(i2);
        return (base1 ? base1->quantity() : 0) - (base2 ? base2->quantity() : 0);
    }

    case Document::QuantityDiff:  {
        auto base1 = static_cast<Document *>(sourceModel())->differenceBaseItem(i1);
        auto base2 = static_cast<Document *>(sourceModel())->differenceBaseItem(i2);
        return (base1 ? i1->quantity() - base1->quantity() : 0)
                - (base2 ? i2->quantity() - base2->quantity() : 0);
    }
    }
    return 0;
}

Document::ItemList DocumentProxyModel::sortItemList(const Document::ItemList &list) const
{
    Document::ItemList result(list);
    qParallelSort(result.begin(), result.end(), [this](const auto &i1, const auto &i2) {
        return index(i1).row() < index(i2).row();
    });
    return result;
}

#include "moc_document.cpp"
