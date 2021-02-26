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
#include <QFontMetrics>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QPainter>
#include <QThreadStorage>
#include <QHelpEvent>
#include <QAbstractItemView>
#include <QApplication>
#include <QRegularExpression>
#include <QToolTip>
#include <QLabel>
#include <QScreen>
#include <QWindow>
#include <QBuffer>
#include <QStringBuilder>

#include "bricklink.h"
#include "bricklink_model.h"
#include "utility.h"

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif



/////////////////////////////////////////////////////////////
// COLORMODEL
/////////////////////////////////////////////////////////////

BrickLink::ColorModel::ColorModel(QObject *parent)
    : StaticPointerModel(parent)
{
    MODELTEST_ATTACH(this)
}

int BrickLink::ColorModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int BrickLink::ColorModel::pointerCount() const
{
    return int(core()->colors().size());
}

const void *BrickLink::ColorModel::pointerAt(int index) const
{
    return core()->colors()[size_t(index)];
}

int BrickLink::ColorModel::pointerIndexOf(const void *pointer) const
{
    const auto &colors = core()->colors();
    auto it = std::find(colors.cbegin(), colors.cend(), static_cast<const Color *>(pointer));
    if (it != colors.cend())
        return int(std::distance(colors.cbegin(), it));
    return -1;
}

const BrickLink::Color *BrickLink::ColorModel::color(const QModelIndex &index) const
{
    return static_cast<const BrickLink::Color *>(pointer(index));
}

QModelIndex BrickLink::ColorModel::index(const Color *color) const
{
    return index(static_cast<const void *>(color));
}

QVariant BrickLink::ColorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !color(index))
        return QVariant();

    QVariant res;
    const Color *c = color(index);

    if (role == Qt:: DisplayRole) {
        res = c->name();
    }
    else if (role == Qt::DecorationRole) {
        QFontMetrics fm = QApplication::fontMetrics();
        QImage img = core()->colorImage(c, fm.height(), fm.height());
        if (!img.isNull()) {
            QPixmap pix = QPixmap::fromImage(img);
            QIcon ico;
            ico.addPixmap(pix, QIcon::Normal);
            ico.addPixmap(pix, QIcon::Selected);
            res = ico;
        }
    }
    else if (role == Qt::ToolTipRole) {
        if (c->id()) {
            res = QString(R"(<table width="100%" border="0" bgcolor="%3"><tr><td><br><br></td></tr></table><br />%1: %2)")
                    .arg(tr("RGB"), c->color().name(), c->color().name());
        } else {
            res = c->name();
        }
    }
    else if (role == ColorPointerRole) {
        res.setValue(c);
    }
    return res;
}

QVariant BrickLink::ColorModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Color by %1").arg(sortOrder() == Qt::AscendingOrder ? tr("Name") : tr("Hue"));
    return QVariant();
}

bool BrickLink::ColorModel::isFiltered() const
{
    return m_itemtype_filter || m_type_filter || !qFuzzyIsNull(m_popularity_filter)
            || !m_color_filter.isEmpty();
}

void BrickLink::ColorModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    m_color_filter.clear();
    invalidateFilter();
}

void BrickLink::ColorModel::setFilterType(Color::Type type)
{
    if (type == m_type_filter)
        return;
    m_type_filter = type;
    m_color_filter.clear();
    invalidateFilter();
}

void BrickLink::ColorModel::unsetFilter()
{
    m_popularity_filter = 0;
    m_type_filter = Color::Type();
    m_itemtype_filter = nullptr;
    m_color_filter.clear();
    invalidateFilter();
}

void BrickLink::ColorModel::setFilterPopularity(qreal p)
{
    if (qFuzzyCompare(p, m_popularity_filter))
        return;
    m_popularity_filter = p;
    m_color_filter.clear();
    invalidateFilter();
}

void BrickLink::ColorModel::setColorListFilter(const QVector<const BrickLink::Color *> &colorList)
{
    if (colorList == m_color_filter)
        return;
    m_popularity_filter = 0;
    m_type_filter = Color::Type();
    m_itemtype_filter = nullptr;
    m_color_filter = colorList;
    invalidateFilter();
}

bool BrickLink::ColorModel::lessThan(const void *p1, const void *p2, int /*column*/) const
{
    const auto *c1 = static_cast<const Color *>(p1);
    const auto *c2 = static_cast<const Color *>(p2);

    if (!c1)
        return true;
    else if (!c2)
        return false;
    else {
        if (sortOrder() == Qt::AscendingOrder) {
            return (c1->name().localeAwareCompare(c2->name()) < 0);
        } else {
            int lh, rh, ls, rs, lv, rv, d;

            c1->color().getHsv(&lh, &ls, &lv);
            c2->color().getHsv(&rh, &rs, &rv);

            if (lh != rh)
                d = lh - rh;
            else if (ls != rs)
                d = ls - rs;
            else
                d = lv - rv;

            return d < 0;
        }
    }
}

bool BrickLink::ColorModel::filterAccepts(const void *pointer) const
{
    const auto *color = static_cast<const Color *>(pointer);

    if (m_itemtype_filter && !(m_itemtype_filter->hasColors() || (color && color->id() == 0)))
        return false;
    if (m_type_filter && !(color->type() & m_type_filter))
        return false;
    if (!qFuzzyIsNull(m_popularity_filter) && (color->popularity() < m_popularity_filter))
        return false;
    if (!m_color_filter.isEmpty() && !m_color_filter.contains(color))
        return false;

    return true;
}



/////////////////////////////////////////////////////////////
// CATEGORYMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const BrickLink::Category *BrickLink::CategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);

BrickLink::CategoryModel::CategoryModel(QObject *parent)
    : StaticPointerModel(parent), m_itemtype_filter(nullptr), m_all_filter(false)
{
    MODELTEST_ATTACH(this)
}

int BrickLink::CategoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int BrickLink::CategoryModel::pointerCount() const
{
    return int(core()->categories().size() + 1);
}

const void *BrickLink::CategoryModel::pointerAt(int index) const
{
    return (index == 0) ? AllCategories : core()->categories()[size_t(index) - 1];
}

int BrickLink::CategoryModel::pointerIndexOf(const void *pointer) const
{
    if (pointer == AllCategories) {
        return 0;
    } else {
        const auto &cats = core()->categories();
        auto it = std::find(cats.cbegin(), cats.cend(), static_cast<const Category *>(pointer));
        if (it != cats.cend())
            return int(std::distance(cats.cbegin(), it)) + 1;
    }
    return -1;
}

const BrickLink::Category *BrickLink::CategoryModel::category(const QModelIndex &index) const
{
    return static_cast<const BrickLink::Category *>(pointer(index));
}

QModelIndex BrickLink::CategoryModel::index(const Category *category) const
{
    return index(static_cast<const void *>(category));
}

QVariant BrickLink::CategoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !category(index))
        return QVariant();

    QVariant res;
    const Category *c = category(index);

    if (role == Qt::DisplayRole)
        res = c != AllCategories ? c->name() : QString("%1").arg(tr("All Items"));
    else if (role == CategoryPointerRole)
        res.setValue(c);
    return res;
}

QVariant BrickLink::CategoryModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Category");
    return QVariant();
}

bool BrickLink::CategoryModel::isFiltered() const
{
    return m_itemtype_filter || m_all_filter;
}

void BrickLink::CategoryModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::CategoryModel::setFilterAllCategories(bool b)
{
    if (b == m_all_filter)
        return;

    m_all_filter = b;
    invalidateFilter();
}

bool BrickLink::CategoryModel::lessThan(const void *p1, const void *p2, int /*column*/) const
{
    const auto *c1 = static_cast<const Category *>(p1);
    const auto *c2 = static_cast<const Category *>(p2);
    bool asc = (sortOrder() == Qt::AscendingOrder);

    if (!c1 || c1 == AllCategories)
        return asc;
    else if (!c2 || c2 == AllCategories)
        return !asc;
    else
        return c1->name().localeAwareCompare(c2->name()) < 0;
}

bool BrickLink::CategoryModel::filterAccepts(const void *pointer) const
{
    const auto *c = static_cast<const Category *>(pointer);

    if (m_itemtype_filter || m_all_filter) {
        if (!c)
            return false;
        else if (c == AllCategories)
            return !m_all_filter;
        else if (m_itemtype_filter)
            return m_itemtype_filter->categories().contains(c);
    }
    return true;
}


/////////////////////////////////////////////////////////////
// ITEMTYPEMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemTypeModel::ItemTypeModel(QObject *parent)
    : StaticPointerModel(parent), m_inv_filter(false)
{
    MODELTEST_ATTACH(this)
}

int BrickLink::ItemTypeModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int BrickLink::ItemTypeModel::pointerCount() const
{
    return int(core()->itemTypes().size());
}

const void *BrickLink::ItemTypeModel::pointerAt(int index) const
{
    return core()->itemTypes()[size_t(index)];
}

int BrickLink::ItemTypeModel::pointerIndexOf(const void *pointer) const
{
    const auto &itts = core()->itemTypes();
    auto it = std::find(itts.cbegin(), itts.cend(), static_cast<const ItemType *>(pointer));
    if (it != itts.cend())
        return int(std::distance(itts.cbegin(), it));
    return -1;
}

const BrickLink::ItemType *BrickLink::ItemTypeModel::itemType(const QModelIndex &index) const
{
    return static_cast<const BrickLink::ItemType *>(pointer(index));
}

QModelIndex BrickLink::ItemTypeModel::index(const ItemType *itemtype) const
{
    return index(static_cast<const void *>(itemtype));
}

QVariant BrickLink::ItemTypeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !itemType(index))
        return QVariant();

    QVariant res;
    const ItemType *i = itemType(index);

    if (role == Qt::DisplayRole) {
        res = i->name();
    }
    else if (role == ItemTypePointerRole) {
        res.setValue(i);
    }
    return res;
}

QVariant BrickLink::ItemTypeModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return QVariant();
}

bool BrickLink::ItemTypeModel::isFiltered() const
{
    return m_inv_filter;
}

void BrickLink::ItemTypeModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    invalidateFilter();
}

bool BrickLink::ItemTypeModel::lessThan(const void *p1, const void *p2, int /*column*/) const
{
    const auto *i1 = static_cast<const ItemType *>(p1);
    const auto *i2 = static_cast<const ItemType *>(p2);

    return !i1 ? true : (!i2 ? false : i1->name().localeAwareCompare(i2->name()) < 0);
}

bool BrickLink::ItemTypeModel::filterAccepts(const void *pointer) const
{
    const auto *itemtype = static_cast<const ItemType *>(pointer);

    return !m_inv_filter || (itemtype && itemtype->hasInventories());
}



/////////////////////////////////////////////////////////////
// ITEMMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemModel::ItemModel(QObject *parent)
    : StaticPointerModel(parent), m_itemtype_filter(nullptr), m_category_filter(nullptr), m_inv_filter(false)
{
    MODELTEST_ATTACH(this)
    connect(core(), &Core::pictureUpdated, this, &ItemModel::pictureUpdated);
}

int BrickLink::ItemModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

int BrickLink::ItemModel::pointerCount() const
{
    return int(core()->items().size());
}

const void *BrickLink::ItemModel::pointerAt(int index) const
{
    return core()->items().at(size_t(index));
}

int BrickLink::ItemModel::pointerIndexOf(const void *pointer) const
{
    const auto &items = core()->items();
    auto it = std::find(items.cbegin(), items.cend(), static_cast<const Item *>(pointer));
    return it != items.cend() ? int(std::distance(items.cbegin(), it)): -1;
}

const BrickLink::Item *BrickLink::ItemModel::item(const QModelIndex &index) const
{
    return static_cast<const BrickLink::Item *>(pointer(index));
}

QModelIndex BrickLink::ItemModel::index(const Item *item) const
{
    return index(static_cast<const void *>(item));
}

QVariant BrickLink::ItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !item(index))
        return QVariant();

    QVariant res;
    const Item *i = item(index);

    if (role == Qt::DisplayRole) {
        switch(index.column()) {
        case 1: res = i->id(); break;
        case 2: res = i->name(); break;
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == 0) {
            return Qt::AlignCenter;
        }
    }
    else if (role == Qt::ToolTipRole) {
        if (index.column() == 0)
            res = i->name();
    }
    else if (role == ItemPointerRole) {
        res.setValue(i);
    }
    else if (role == ItemTypePointerRole) {
        res.setValue(i->itemType());
    }
    else if (role == CategoryPointerRole) {
        res.setValue(i->category());
    }
    return res;
}

QVariant BrickLink::ItemModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch(section) {
        case 1: return tr("Part #");
        case 2: return tr("Description");
        }
    }
    return QVariant();
}

void BrickLink::ItemModel::pictureUpdated(Picture *pic)
{
    if (!pic || !pic->item() || pic->color() != pic->item()->defaultColor())
        return;

    QModelIndex idx = index(pic->item());
    if (idx.isValid())
        emit dataChanged(idx, idx);
}

bool BrickLink::ItemModel::isFiltered() const
{
    return m_itemtype_filter || m_category_filter || m_inv_filter || !m_text_filter.isEmpty();
}

void BrickLink::ItemModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;

    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::ItemModel::setFilterCategory(const Category *cat)
{
    if (cat == m_category_filter)
        return;
    m_category_filter = cat;
    invalidateFilter();
}

void BrickLink::ItemModel::setFilterText(const QString &filter)
{
    if (filter == m_text_filter)
        return;

    m_text_filter = filter;
    m_filter_text.clear();
    m_filter_appearsIn.clear();
    m_filter_consistsOf.clear();

    const QStringList sl = filter.simplified().split(QChar(' '));

    const QString consistsOfPrefix = tr("consists-of:", "Filter prefix");
    const QString appearsInPrefix = tr("appears-in:", "Filter prefix");

    QString quoted;
    bool quotedNegate = false;

    for (const auto &s : sl) {
        if (s.isEmpty())
            continue;

        if (!quoted.isEmpty()) {
            quoted.append(s);
            if (quoted.endsWith(QChar('"'))) {
                quoted.chop(1);
                m_filter_text << qMakePair(quotedNegate, quoted);
                quoted.clear();
            } else {
                quoted.append(QChar(' '));
            }

        } else if (s.length() == 1) {
            // just a single character -> search for it literally
            m_filter_text << qMakePair(false, s);

        } else {
            const QChar first = s.at(0);
            const bool negate = (first == QChar('-'));
            auto str = negate ? s.mid(1) : s;

            if (str.startsWith(consistsOfPrefix)) {
                str = str.mid(consistsOfPrefix.length());

                // contains either a minifig or a part, optionally with color-id
                const BrickLink::Color *color = nullptr;

                int atPos = str.lastIndexOf(QChar('@'));
                if (atPos != -1) {
                    color = BrickLink::core()->color(str.mid(atPos + 1).toUInt());
                    str = str.left(atPos);
                }

                auto item = BrickLink::core()->item('M', str);
                if (!item)
                    item = BrickLink::core()->item('P', str);
                if (item)
                    m_filter_consistsOf << qMakePair(negate, qMakePair(item, color));

            } else if (str.startsWith(appearsInPrefix)) {
                str = str.mid(appearsInPrefix.length());

                // appears-in either a minifig or a set
                auto item = BrickLink::core()->item('M', str);
                if (!item)
                    item = BrickLink::core()->item('S', str);
                if (item)
                    m_filter_appearsIn << qMakePair(negate, item);

            } else {
                const bool firstIsQuote = (str.at(0) == QChar('"'));

                if (firstIsQuote) {
                    quoted = str.mid(1) % u' ';
                    quotedNegate = negate;
                } else {
                    m_filter_text << qMakePair(negate, str);
                }
            }
        }
    }

    invalidateFilter();
}

void BrickLink::ItemModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    invalidateFilter();
}

bool BrickLink::ItemModel::lessThan(const void *p1, const void *p2, int column) const
{
    const Item *i1 = static_cast<const Item *>(p1);
    const Item *i2 = static_cast<const Item *>(p2);

    return Utility::naturalCompare((column == 2) ? i1->name() : i1->id(),
                                   (column == 2) ? i2->name() : i2->id()) < 0;
}

bool BrickLink::ItemModel::filterAccepts(const void *pointer) const
{
    const Item *item = static_cast<const Item *>(pointer);

    if (!item)
        return false;
    else if (m_itemtype_filter && item->itemType() != m_itemtype_filter)
        return false;
    else if (m_category_filter && (m_category_filter != BrickLink::CategoryModel::AllCategories) && (item->category() != m_category_filter))
        return false;
    else if (m_inv_filter && !item->hasInventory())
        return false;
    else {
        const QString matchStr = item->id() % u' ' % item->name();

        // .first is always "bool negate"

        bool match = true;
        for (const auto &p : m_filter_text)
            match = match && (matchStr.contains(p.second, Qt::CaseInsensitive) == !p.first); // contains() xor negate

        for (const auto &a : m_filter_appearsIn) {
            bool found = false;
            const auto appearsvec = item->appearsIn();
            for (const AppearsInColor &vec : appearsvec) {
                for (const AppearsInItem &ai : vec) {
                    if (ai.second == a.second) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
            match = match && (found == !a.first); // found xor negate
        }
        for (const auto &c : m_filter_consistsOf) {
            bool found = false;
            const auto containslist = item->consistsOf();
            for (const auto &ci : containslist) {
                if ((ci->item() == c.second.first)
                        && (!c.second.second || (ci->color() == c.second.second))) {
                    found = true;
                    break;
                }
            }
            qDeleteAll(containslist);
            match = match && (found == !c.first); // found xor negate
        }

        return match;
    }
}


/////////////////////////////////////////////////////////////
// APPEARSINMODEL
/////////////////////////////////////////////////////////////


BrickLink::InternalAppearsInModel::InternalAppearsInModel(const Item *item, const Color *color, QObject *parent)
    : QAbstractTableModel(parent)
{
    InvItemList list;
    InvItem invitem(color, item);
    list.append(&invitem);
    init(list);
}

BrickLink::InternalAppearsInModel::InternalAppearsInModel(const InvItemList &list, QObject *parent)
    : QAbstractTableModel(parent)
{
    init(list);
}

void BrickLink::InternalAppearsInModel::init(const InvItemList &list)
{
    MODELTEST_ATTACH(this)

    QHash<const Item *, int> unique;
    bool first_item = true;
    bool single_item = (list.count() == 1);

    for (const InvItem *invitem : list) {
        if (!invitem->item())
            continue;

        const auto appearsvec = invitem->item()->appearsIn(invitem->color());
        for (const AppearsInColor &vec : appearsvec) {
            for (const AppearsInItem &item : vec) {
                if (single_item) {
                    m_items.append(new AppearsInItem(item.first, item.second));
                } else {
                    auto it = unique.find(item.second);
                    if (it != unique.end())
                        ++it.value();
                    else if (first_item)
                        unique.insert(item.second, 1);
                }
            }
        }
        first_item = false;
    }
    for (auto it = unique.begin(); it != unique.end(); ++it) {
        if (it.value() >= list.count())
            m_items.append(new AppearsInItem(-1, it.key()));
    }
}

BrickLink::InternalAppearsInModel::~InternalAppearsInModel()
{
    qDeleteAll(m_items);
}

QModelIndex BrickLink::InternalAppearsInModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent) && !parent.isValid())
        return createIndex(row, column, m_items.at(row));
    return {};
}

const BrickLink::AppearsInItem *BrickLink::InternalAppearsInModel::appearsIn(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<const AppearsInItem *>(idx.internalPointer()) : nullptr;
}

QModelIndex BrickLink::InternalAppearsInModel::index(const AppearsInItem *const_ai) const
{
    auto *ai = const_cast<AppearsInItem *>(const_ai);

    return ai ? createIndex(m_items.indexOf(ai), 0, ai) : QModelIndex();
}

int BrickLink::InternalAppearsInModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int BrickLink::InternalAppearsInModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant BrickLink::InternalAppearsInModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    const AppearsInItem *appears = appearsIn(index);
    int col = index.column();

    if (!appears)
        return res;

    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: res = appears->first < 0 ? QLatin1String("-") : QString::number(appears->first); break;
        case 1: res = appears->second->id(); break;
        case 2: res = appears->second->name(); break;
        }
    }
    else if (role == BrickLink::AppearsInItemPointerRole) {
        res.setValue(appears);
    }
    else if (role == BrickLink::ItemPointerRole) {
        res.setValue(appears->second);
    }
    else if (role == BrickLink::ColorPointerRole) {
        res.setValue(appears->second->defaultColor());
    }
    return res;
}

QVariant BrickLink::InternalAppearsInModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch (section) {
        case 0: return tr("Qty.");
        case 1: return tr("Set");
        case 2: return tr("Name");
        }
    }
    return QVariant();
}

BrickLink::AppearsInModel::AppearsInModel(const BrickLink::InvItemList &list, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new InternalAppearsInModel(list, this));
}

BrickLink::AppearsInModel::AppearsInModel(const Item *item, const Color *color, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new InternalAppearsInModel(item, color, this));
}

const BrickLink::AppearsInItem *BrickLink::AppearsInModel::appearsIn(const QModelIndex &index) const
{
    auto *aim = static_cast<InternalAppearsInModel *>(sourceModel());

    if (aim && index.isValid())
        return aim->appearsIn(mapToSource(index));
    return nullptr;
}

QModelIndex BrickLink::AppearsInModel::index(const AppearsInItem *item) const
{
    auto *aim = static_cast<InternalAppearsInModel *>(sourceModel());

    if (aim && item)
        return mapFromSource(aim->index(item));
    return {};
}

bool BrickLink::AppearsInModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const auto *aim = static_cast<const InternalAppearsInModel *>(sourceModel());
    const AppearsInItem *ai1 = aim->appearsIn(left);
    const AppearsInItem *ai2 = aim->appearsIn(right);

    if (!ai1)
        return true;
    else if (!ai2)
        return false;
    else {
        switch (left.column()) {
        default:
        case  0: return ai1->first < ai2->first;
        case  1: return (Utility::naturalCompare(ai1->second->id(),
                                                 ai2->second->id()) < 0);
        case  2: return (Utility::naturalCompare(ai1->second->name(),
                                                 ai2->second->name()) < 0);
        }
    }
}


/////////////////////////////////////////////////////////////
// ITEMDELEGATE
/////////////////////////////////////////////////////////////



BrickLink::ItemDelegate::ItemDelegate(QObject *parent, Options options)
    : QStyledItemDelegate(parent)
    , m_options(options)
{ }

void BrickLink::ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem myoption(option);

    bool firstColumnImageOnly = (m_options & FirstColumnImageOnly) && (index.column() == 0);

    bool useFrameHover = false;
    bool useFrameSelection = false;

    if (firstColumnImageOnly) {
        useFrameSelection = (option.state & QStyle::State_Selected);
        useFrameHover = (option.state & QStyle::State_MouseOver);
        if (useFrameSelection)
            myoption.state &= ~QStyle::State_Selected;
    }

    if ((m_options & AlwaysShowSelection) && (option.state & QStyle::State_Enabled))
        myoption.state |= QStyle::State_Active;

    QStyledItemDelegate::paint(painter, myoption, index);

    if (firstColumnImageOnly) {
        if (auto *item = index.data(BrickLink::ItemPointerRole).value<const BrickLink::Item *>()) {
            QImage image;

            Picture *pic = core()->picture(item, item->defaultColor());

            if (pic && pic->isValid())
                image = pic->image();
            else
                image = BrickLink::core()->noImage(option.rect.size());

            if (!image.isNull()) {
                QSizeF s = image.size().scaled(option.rect.size(), Qt::KeepAspectRatio);
                QPointF p = option.rect.center() - QRectF({ }, s).center();
                // scale while drawing, so we don't have to deal with hi-dpi calculations
                painter->drawImage({ p, s }, image, image.rect());
            }
        }
    }

    if (firstColumnImageOnly && (useFrameSelection || useFrameHover)) {
        painter->save();
        QColor c = option.palette.color(QPalette::Highlight);
        for (int i = 0; i < 6; ++i) {
            c.setAlphaF((useFrameHover && !useFrameSelection ? 0.6 : 1.) - i * .1);
            painter->setPen(c);
            painter->drawRect(option.rect.adjusted(i, i, -i - 1, -i - 1));
        }
        painter->restore();
    }
}

QSize BrickLink::ItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index) + QSize(0, 2);
}

bool BrickLink::ItemDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::ToolTip && index.isValid()) {
        if (auto item = index.data(BrickLink::ItemPointerRole).value<const BrickLink::Item *>()) {
            ToolTip::inst()->show(item, nullptr, event->globalPos(), view);
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}



BrickLink::ToolTip *BrickLink::ToolTip::s_inst = nullptr;

BrickLink::ToolTip *BrickLink::ToolTip::inst()
{
    if (!s_inst) {
        s_inst = new ToolTip();
        connect(core(), &Core::pictureUpdated, s_inst, &ToolTip::pictureUpdated);
    }
    return s_inst;
}

bool BrickLink::ToolTip::show(const BrickLink::Item *item, const BrickLink::Color *color, const QPoint &globalPos, QWidget *parent)
{
    Q_UNUSED(color)

    if (BrickLink::Picture *pic = BrickLink::core()->picture(item, nullptr, true)) {
        m_tooltip_pic = (pic->updateStatus() == UpdateStatus::Updating) ? pic : nullptr;

        // need to 'clear' to reset the image cache of the QTextDocument
        const auto tlwidgets = QApplication::topLevelWidgets();
        for (QWidget *w : tlwidgets) {
            if (w->inherits("QTipLabel")) {
                qobject_cast<QLabel *>(w)->clear();
                break;
            }
        }
        QString tt = createToolTip(pic->item(), pic);
        if (!tt.isEmpty()) {
            QToolTip::showText(globalPos, tt, parent);
            return true;
        }
    }
    return false;
}

QString BrickLink::ToolTip::createToolTip(const BrickLink::Item *item, BrickLink::Picture *pic) const
{
    static const QString str = QLatin1String(R"(<div class="tooltip_picture">%2<br><b>%3</b><br>%1</div>)");
    static const QString img_left = QLatin1String(R"(<center><img src="data:image/png;base64,%1" width="%2" height="%3"/></center>)");
    QString note_left = u"<i>" % BrickLink::ItemDelegate::tr("[Image is loading]") % u"</i>";

    if (pic && (pic->updateStatus() == UpdateStatus::Updating)) {
        return str.arg(note_left, item->id(), item->name());
    } else {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        const QImage img = pic->image();
        img.save(&buffer, "PNG");

        return str.arg(img_left.arg(QString::fromLatin1(ba.toBase64())).arg(img.width()).arg(img.height()),
                       item->id(), item->name());
    }
}

void BrickLink::ToolTip::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || pic != m_tooltip_pic)
        return;

    m_tooltip_pic = nullptr;

    if (QToolTip::isVisible() && QToolTip::text().startsWith(R"(<div class="tooltip_picture">)")) {
        const auto tlwidgets = QApplication::topLevelWidgets();
        for (QWidget *w : tlwidgets) {
            if (w->inherits("QTipLabel")) {
                qobject_cast<QLabel *>(w)->clear();
                qobject_cast<QLabel *>(w)->setText(createToolTip(pic->item(), pic));

                QRect r(w->pos(), w->sizeHint());
                QRect desktop = w->window()->windowHandle()->screen()->availableGeometry();

                r.translate(r.right() > desktop.right() ? desktop.right() - r.right() : 0,
                            r.bottom() > desktop.bottom() ? desktop.bottom() - r.bottom() : 0);
                w->setGeometry(r);
                break;
            }
        }
    }
}
