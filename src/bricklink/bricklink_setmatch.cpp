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

BrickLink::SetMatch::InvMatchList::InvMatchList()
    : m_count(0)
{ }

BrickLink::SetMatch::InvMatchList::InvMatchList(const InvItemList &list)
    : m_count(0)
{
    add(list);
}

BrickLink::SetMatch::InvMatchList::InvMatchList(const InvMatchList &copy)
    : m_list(copy.m_list), m_count(copy.m_count)
{ }

BrickLink::SetMatch::InvMatchList &BrickLink::SetMatch::InvMatchList::operator=(const InvMatchList &copy)
{
    m_list = copy.m_list;
    m_count = copy.m_count;
    return *this;
}

void BrickLink::SetMatch::InvMatchList::add(const InvItemList &list)
{
    foreach (const InvItem *ii, list) {
        if (!ii->isIncomplete() &&
            (ii->quantity() > 0) &&
            !ii->counterPart() &&
            (ii->status() == BrickLink::Include) &&
            (!ii->alternateId() || !ii->alternate())) {
            InvMatchItem mi = { ii->item(), ii->color(), ii->quantity() };
            m_list << mi;

            m_count += mi.qty;
        }
    }
}
#if 0
bool BrickLink::SetMatch::InvMatchList::subtract(const InvItemList &list)
{
    if (list.isEmpty())
        return false;

    // for every item in the given set inventory
    foreach (const InvItem *ii, list) {
        // we need that many parts in this step
        int qty_to_match = ii->quantity();

        // quick check if there are at least those many parts left (regardless of type and color)
        if (m_count < qty_to_match)
            return false;

        // for every item in the inventory
        //foreach (InvMatchItem &mi, m_list) {
        for (QList<InvMatchItem>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
            InvMatchItem &mi = *it;

            // check if type and color matches
            if (ii->item() == mi.item && ii->color() == mi.color) {
                // can we satisfy this request with a single lot?
                if (qty_to_match > mi.qty) { // no
                    qty_to_match -= mi.qty;
                    m_count -= mi.qty;
                    mi.qty = 0; // only a partial match, try again
                } else { // yes
                    mi.qty -= qty_to_match;
                    m_count -= qty_to_match;
                    qty_to_match = 0;
                    break; // we matched this item completl
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
#endif

bool BrickLink::SetMatch::InvMatchList::subtract(const InvMatchList &list)
{
    if (list.isEmpty() || list.count() > count())
        return false;

    // for every item in the given set inventory
    for (QList<InvMatchItem>::const_iterator ii = list.m_list.begin(); ii != list.m_list.end(); ++ii) {
        // we need that many parts in this step
        int qty_to_match = ii->qty;

        // quick check if there are at least those many parts left (regardless of type and color)
        if (m_count < qty_to_match)
            return false;

        // for every item in the inventory
        //foreach (InvMatchItem &mi, m_list) {
        for (QList<InvMatchItem>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
            InvMatchItem &mi = *it;

            // check if type and color matches
            if (ii->item == mi.item && ii->color == mi.color) {
                // can we satisfy this request with a single lot?
                if (qty_to_match > mi.qty) { // no
                    qty_to_match -= mi.qty;
                    m_count -= mi.qty;
                    mi.qty = 0; // only a partial match, try again
                } else { // yes
                    mi.qty -= qty_to_match;
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



BrickLink::SetMatch::GreedyComparePairs::GreedyComparePairs(bool prefer_small)
   : m_prefer_small(prefer_small)
{ }

bool BrickLink::SetMatch::GreedyComparePairs::operator()(const QPair<const BrickLink::Item *, BrickLink::SetMatch::InvMatchList> &p1, const QPair<const BrickLink::Item *, BrickLink::SetMatch::InvMatchList> &p2)
{
    return m_prefer_small ? p1.second.count() < p2.second.count() : p1.second.count() > p2.second.count();
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////


BrickLink::SetMatch::SetMatch(QObject *parent)
    : QObject(parent), m_algorithm(Recursive), m_part_min(-1), m_part_max(-1), m_year_min(-1), m_year_max(-1),
      m_prefer(PreferLargerSets), m_stopvalue(0), m_step(0), m_active(false)
{
}

BrickLink::SetMatch::~SetMatch()
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

void BrickLink::SetMatch::setCategoryConstraint(const QList<const Category *> &list)
{
    if (!isActive())
        m_categories = list;
}

void BrickLink::SetMatch::setItemTypeConstraint(const QList<const ItemType *> &list)
{
    if (!isActive())
        m_itemtypes = list;
}

void BrickLink::SetMatch::setGreedyPreference(GreedyPreference p)
{
    if (!isActive())
        m_prefer = p;
}

void BrickLink::SetMatch::setRecursiveBound(float f)
{
    if (!isActive())
        m_stopvalue = f;
}

namespace BrickLink {

class MatchThread : public QThread {
    Q_OBJECT
public:
    MatchThread(bool all, BrickLink::SetMatch *sm, const BrickLink::InvItemList &list)
        : QThread(sm), m_sm(sm), m_list(list), m_all(all)
    { }

    void run()
    {
        QList<const BrickLink::Item *> result;

        if (m_all)
            result = m_sm->allPossibleSetMatch(m_list);
        else
            result = m_sm->maximumPossibleSetMatch(m_list);

        emit m_sm->finished(result);
        delete this;
    }

signals:
    void finished(int, QList<const BrickLink::Item *> &);
    void progress(int, int);

private:
    BrickLink::SetMatch *m_sm;
    BrickLink::InvItemList m_list;
    bool m_all;
};

} // namespace BrickLink

bool BrickLink::SetMatch::startMaximumPossibleSetMatch(const InvItemList &list, Algorithm a)
{
    if (isActive())
        return false;
    m_active = true;

    m_algorithm = a;

    MatchThread *mt = new MatchThread(false, this, list);
    mt->run();
    return true;
}

bool BrickLink::SetMatch::startAllPossibleSetMatch(const InvItemList &list)
{
    if (isActive())
        return false;
    m_active = true;

    MatchThread *mt = new MatchThread(true, this, list);
    mt->run();
    return true;
}

QList<const BrickLink::Item *> BrickLink::SetMatch::allPossibleSetMatch(const InvItemList &list)
{
    InvMatchList parts(list);
    QList<const Item *> result;
    int p = 0, pmax = m_inventories.count(), pstep = pmax / 100;

    for (QVector<QPair<const Item *, InvMatchList> >::const_iterator it = m_inventories.begin(); it != m_inventories.end(); ++it) {
        InvMatchList parts_copy = parts;

        if (parts_copy.subtract(it->second))
            result << it->first;

        if ((++p % pstep) == 0)
            emit progress(p, pmax);
    }
    return result;
}


QList<const BrickLink::Item *> BrickLink::SetMatch::maximumPossibleSetMatch(const InvItemList &list)
{
    create_inventory_list();

    InvMatchList parts(list);
    QPair<int, QList<const Item *> > result(list.count(), QList<const Item *>());

    m_step = 0;

    qWarning("Starting set match using algorithm \'%s\'", m_algorithm == Recursive ? "recursive" : "greedy");

    // simple Knapsack algorithm O(2^n)
    if (m_algorithm == Recursive) {
        m_doesnotfit.resize(m_inventories.count());
        m_doesnotfit.fill(false);
        m_startcount = parts.count();

        result = set_match_recursive(m_inventories.begin(), parts);

        m_doesnotfit.resize(0);
    }
    // greedy Knapsack algorithm O(n)
    else if (m_algorithm == Greedy) {
        result = set_match_greedy(parts);
    }

    qWarning("found a solution with %d parts left", result.first);
    return result.second;
}

QPair<int, QList<const BrickLink::Item *> > BrickLink::SetMatch::set_match_greedy(InvMatchList &parts)
{
    QList<const Item *> result;
    int p = 0, pmax = m_inventories.count() * 2, pstep = pmax / 100;

    // pass == 0: try to match one of each set
    // pass == 1: fill up with as many copies as possible
    for (int pass = 0; pass < 2; ++pass) {
        // try to get one of each
        for (QVector<QPair<const Item *, InvMatchList> >::const_iterator it = m_inventories.begin(); it != m_inventories.end(); ++it) {
            forever {
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

QPair<int, QList<const BrickLink::Item *> > BrickLink::SetMatch::set_match_recursive(QVector<QPair<const Item *, InvMatchList> >::const_iterator it, InvMatchList &parts)
{
    // emit progress every 1024 (0x400) steps
    if ((++m_step & 0x3ff) == 0)
        emit progress(m_step >> 10, -1);

    if (it == m_inventories.end()) // no more sets to match against
        return qMakePair(parts.count(), QList<const Item *>());
    if (parts.count() == 0) // all parts matched
        return qMakePair(0, QList<const Item *>());

//    qWarning("Recursing at %d - %s [%s], %d", i, core()->items()[i]->name(), core()->items()[i]->id(), core()->items()[i]->hasInventory() ? 1 : 0);

    QVector<QPair<const Item *, InvMatchList> >::const_iterator next = it;
    ++next;

    QPair<int, QList<const BrickLink::Item *> > result_take;
    QPair<int, QList<const BrickLink::Item *> > result_do_not_take;

    result_take.first = INT_MAX;

    // check parts left, when not taking this one
    result_do_not_take = set_match_recursive(next, parts);

    auto idx = (it - m_inventories.begin());

    if (!m_doesnotfit[int(idx)]) {
        InvMatchList parts_copy = parts;

        // check parts left, when taking this one
        bool ok = parts_copy.subtract(it->second);
        if (ok)
            result_take = set_match_recursive(next, parts_copy);

        // if it doesn't fit into the starting set, it won't fit in a smaller set
        if (!ok && (parts.count() == m_startcount)) {
            //qWarning("does not fit: %d:  %s", idx, it->first->id());
            m_doesnotfit[int(idx)] = true;
        }
    }

    if (result_take.first < result_do_not_take.first) {
        result_take.second.append(it->first);
        return result_take;
    } else {
        return result_do_not_take;
    }
}

void BrickLink::SetMatch::create_inventory_list()
{
    clear_inventory_list();
    const QVector<const Item *> &items = core()->items();

    for (QVector<const Item *>::const_iterator it = items.begin(); it != items.end(); ++it) {
        bool ok = true;

        ok &= ((*it)->itemType()->hasInventories() && (*it)->hasInventory());
        ok &= ((m_year_min == -1) || ((*it)->yearReleased() >= m_year_min));
        ok &= ((m_year_max == -1) || ((*it)->yearReleased() <= m_year_max));

        ok &= (m_itemtypes.isEmpty() || m_itemtypes.contains((*it)->itemType()));

        if (ok && !m_categories.isEmpty()) {
            ok = false;
            for (QList<const Category *>::const_iterator cat_it = m_categories.begin(); !ok && cat_it != m_categories.end(); ++cat_it)
                ok = (*it)->hasCategory(*cat_it);
        }
        if (ok) {
            InvMatchList iml((*it)->consistsOf());
            ok &= !iml.isEmpty();
            ok &= ((m_part_min == -1) || (iml.count() >= m_part_min));
            ok &= ((m_part_max == -1) || (iml.count() <= m_part_max));

            if (ok)
                m_inventories.append(qMakePair(*it, iml));
        }
    }

    if (m_algorithm == Greedy) {
        GreedyComparePairs gpc(m_prefer == PreferSmallerSets);
        std::sort(m_inventories.begin(), m_inventories.end(), gpc);
    }
    qWarning("InvMatchList has %d entries", m_inventories.count());
}


void BrickLink::SetMatch::clear_inventory_list()
{
    m_inventories.clear();
}

#include "bricklink_setmatch.moc"
