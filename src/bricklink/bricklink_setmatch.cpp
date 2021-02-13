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
#include <QThread>

#include "bricklink_setmatch.h"

/*

What sets can be built (one at a time)
What sets can be built (all at once)


Greedy + Recursive:
    Constraints are handled in nextPossibleMatch

Greedy:
    Preferences are handled when sorting the array ("cost/weight")

    Preferences:
        * Prefer big/small
        * Prefer old/new
       (* Prefer Category/ItemType --> would clutter UI)

Recursive:
    No Preferences possible
    NOT POSSIBLE anymore, because BL has too many items.


Constraints:
    * part count: QPair<int, int>(min, max) (min, max == -1 -> don't care)
    * year released: QPair<int, int>(min, max) (min, max == -1 -> don't care)
    * category: QList<const Category *> (empty -> don't care)
                bool not_these_categories;
    * item type: QList<const ItemType *> (empty -> don't care)
                 bool not_these_itemtypes;
    * price: WOULD BE COOL, BUT WE NEED A PRECALCULATED PRICE/SET DB FIRST!!


--------------------------------------------------

Match your items against the BrickLink set inventory database:

Select constraints



Select algorithm to use [Greedy             v] (i)



*/

//QVector<QPair<const BrickLink::Item *, BrickLink::SetMatch::InvMatchList> > BrickLink::SetMatch::s_inventories;

BrickLink::SetMatch::InvMatchList::InvMatchList(const InvItemList &list)
    : InvMatchList()
{
    add(list);
}

void BrickLink::SetMatch::InvMatchList::add(const InvItemList &list)
{
    for (const InvItem *ii : list) {
        if (!ii->isIncomplete() &&
            (ii->quantity() > 0) &&
            !ii->counterPart() &&
            (ii->status() == BrickLink::Status::Include) &&
            (!ii->alternateId() || !ii->alternate())) {
            InvMatchItem mi = { ii->item(), ii->color(), ii->quantity() };
            m_list << mi;

            m_count += mi.qty;
        }
    }
}

bool BrickLink::SetMatch::InvMatchList::subtract(const InvMatchList &list)
{
    if (list.isEmpty() || list.count() > count())
        return false;

    // for every item in the given set inventory
    for (const auto &item : list.m_list) {
        // we need that many parts in this step
        int qty_to_match = item.qty;

        // quick check if there are at least those many parts left (regardless of type and color)
        if (m_count < qty_to_match)
            return false;

        // for every item in the inventory
        //foreach (InvMatchItem &mi, m_list) {
        for (auto &matchItem : m_list) {
            // check if type and color matches
            if (item.item == matchItem.item && item.color == matchItem.color) {
                // can we satisfy this request with a single lot?
                if (qty_to_match > matchItem.qty) { // no
                    qty_to_match -= matchItem.qty;
                    m_count -= matchItem.qty;
                    matchItem.qty = 0; // only a partial match, try again
                } else { // yes
                    matchItem.qty -= qty_to_match;
                    m_count -= qty_to_match;
                    qty_to_match = 0;
                    break; // we matched this item completely
                }
            }
        }
        // we couldn't match one of the items completely -> failure
        if (qty_to_match > 0)
            return false;
    }

    // we matched all items completely -> success
    return true;
}

int BrickLink::SetMatch::InvMatchList::count() const
{
    return m_count;
}

bool BrickLink::SetMatch::InvMatchList::isEmpty() const
{
    return m_count == 0;
}


//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////


BrickLink::SetMatch::SetMatch(QObject *parent)
    : QObject(parent)
{ }

bool BrickLink::SetMatch::isActive() const
{
    return m_active;
}

void BrickLink::SetMatch::setPartCountConstraint(int _min, int _max)
{
    if (!isActive()) {
        m_part_min = _min;
        m_part_max = _max;
    }
}

void BrickLink::SetMatch::setYearReleasedConstraint(int _min, int _max)
{
    if (!isActive()) {
        m_year_min = _min;
        m_year_max = _max;
    }
}

void BrickLink::SetMatch::setCategoryConstraint(const QVector<const Category *> &list)
{
    if (!isActive())
        m_categories = list;
}

void BrickLink::SetMatch::setItemTypeConstraint(const QVector<const ItemType *> &list)
{
    if (!isActive())
        m_itemtypes = list;
}

void BrickLink::SetMatch::setGreedyPreference(GreedyPreference p)
{
    if (!isActive())
        m_prefer = p;
}

namespace BrickLink {

class MatchThread : public QThread {
    Q_OBJECT
public:
    MatchThread(bool all, BrickLink::SetMatch *sm, const BrickLink::InvItemList &list)
        : QThread(sm)
        , m_sm(sm)
        , m_list(list)
        , m_all(all)
    { }

    void run() override
    {
        QVector<const BrickLink::Item *> result;

        if (m_all)
            result = m_sm->allPossibleSetMatch(m_list);
        else
            result = m_sm->maximumPossibleSetMatch(m_list);

        emit m_sm->finished(result);
        delete this;
    }

private:
    BrickLink::SetMatch *m_sm;
    const BrickLink::InvItemList m_list;
    bool m_all;
};

} // namespace BrickLink

bool BrickLink::SetMatch::startMaximumPossibleSetMatch(const InvItemList &list, Algorithm a)
{
    if (isActive())
        return false;
    m_active = true;

    m_algorithm = a;

    auto *mt = new MatchThread(false, this, list);
    mt->run();
    return true;
}

bool BrickLink::SetMatch::startAllPossibleSetMatch(const InvItemList &list)
{
    if (isActive())
        return false;
    m_active = true;

    auto *mt = new MatchThread(true, this, list);
    mt->run();
    return true;
}

QVector<const BrickLink::Item *> BrickLink::SetMatch::allPossibleSetMatch(const InvItemList &list)
{
    InvMatchList parts(list);
    QVector<const Item *> result;
    int p = 0, pmax = m_inventories.count(), pstep = pmax / 100;

    for (auto it = m_inventories.constBegin(); it != m_inventories.constEnd(); ++it) {
        InvMatchList parts_copy = parts;

        if (parts_copy.subtract(it->second))
            result << it->first;

        if ((++p % pstep) == 0)
            emit progress(p, pmax);
    }
    return result;
}


QVector<const BrickLink::Item *> BrickLink::SetMatch::maximumPossibleSetMatch(const InvItemList &list)
{
    create_inventory_list();

    InvMatchList parts(list);
    QPair<int, QVector<const Item *>> result(list.count(), QVector<const Item *>());

    m_step = 0;

    qWarning("Starting set match using algorithm \'%s\'", m_algorithm == Greedy ? "greedy" : "???");

    // greedy Knapsack algorithm O(n)
    if (m_algorithm == Greedy) {
        result = set_match_greedy(parts);
    }

    qWarning("found a solution with %d parts left", result.first);
    return result.second;
}

QPair<int, QVector<const BrickLink::Item *>> BrickLink::SetMatch::set_match_greedy(InvMatchList &parts)
{
    QVector<const Item *> result;
    int p = 0, pmax = m_inventories.count() * 2, pstep = pmax / 100;

    // pass == 0: try to match one of each set
    // pass == 1: fill up with as many copies as possible
    for (int pass = 0; pass < 2; ++pass) {
        // try to get one of each
        for (auto it = m_inventories.constBegin(); it != m_inventories.constEnd(); ++it) {
            while (true) {
                InvMatchList parts_copy = parts;

                // can we take this one?
                if (!parts_copy.subtract(it->second))
                    break;

                result << it->first;
                parts = parts_copy;

                if (pass == 0)
                    break; // only one copy this time
            }
            if ((++p % pstep) == 0)
                emit progress(p, pmax);
        }
    }
    emit progress(pmax, pmax);

    return qMakePair(parts.count(), result);
}


void BrickLink::SetMatch::create_inventory_list()
{
    clear_inventory_list();
    const auto &items = core()->items();

    for (auto item : items) {
        bool ok = true;

        ok = ok && (item->itemType()->hasInventories() && item->hasInventory());
        ok = ok && ((m_year_min == -1) || (item->yearReleased() >= m_year_min));
        ok = ok && ((m_year_max == -1) || (item->yearReleased() <= m_year_max));

        ok = ok && (m_itemtypes.isEmpty() || m_itemtypes.contains(item->itemType()));

        ok = ok && (m_categories.isEmpty() || m_categories.contains(item->category()));

        if (ok) {
            InvMatchList iml(item->consistsOf());
            ok = ok && !iml.isEmpty();
            ok = ok && ((m_part_min == -1) || (iml.count() >= m_part_min));
            ok = ok && ((m_part_max == -1) || (iml.count() <= m_part_max));

            if (ok) {
                m_inventories.append(qMakePair(item, iml));
            }
        }
    }

    if (m_algorithm == Greedy) {
        qWarning() << "Sorting inventories for greedy matching";
        std::sort(m_inventories.begin(), m_inventories.end(), [this](const auto &p1, const auto &p2) {
            return m_prefer == PreferSmallerSets ? p1.second.count() < p2.second.count()
                                                 : p1.second.count() > p2.second.count();

        });
    }
    qWarning("InvMatchList has %d entries", m_inventories.count());
}


void BrickLink::SetMatch::clear_inventory_list()
{
    m_inventories.clear();
}

#include "bricklink_setmatch.moc"

#include "moc_bricklink_setmatch.cpp"
