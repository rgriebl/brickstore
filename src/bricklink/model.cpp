/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QtCore/QBuffer>
#include <QtCore/QStringBuilder>
#include <QtCore/QThreadStorage>
#include <QtCore/QRegularExpression>
#include <QtGui/QGuiApplication>
#include <QtGui/QFontMetrics>
#include <QtGui/QPixmap>
#include <QtGui/QImage>
#include <QtGui/QIcon>

#include "utility/utility.h"
#include "bricklink/core.h"
#include "bricklink/category.h"
#include "bricklink/item.h"
#include "bricklink/picture.h"
#include "bricklink/model.h"

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
    return &core()->colors()[size_t(index)];
}

int BrickLink::ColorModel::pointerIndexOf(const void *pointer) const
{
    const auto &colors = core()->colors();
    auto d = static_cast<const Color *>(pointer) - colors.data();
    return (d >= 0 && d < int(colors.size())) ? int(d) : -1;
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

    if ((role == Qt::DisplayRole) || (role == Qt::EditRole)) {
        res = c->name();
    }
    else if (role == Qt::DecorationRole) {
        QFontMetricsF fm(QGuiApplication::font());
        QImage img = c->image(int(fm.height()) + 4, int(fm.height()) + 4);
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
            res = QString::fromLatin1(R"(<table width="100%" border="0" bgcolor="%3"><tr><td><br><br></td></tr></table><br />%1: %2)")
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

void BrickLink::ColorModel::setFilterPopularity(float p)
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
    : StaticPointerModel(parent)
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
    return (index == 0) ? AllCategories : &core()->categories()[size_t(index) - 1];
}

int BrickLink::CategoryModel::pointerIndexOf(const void *pointer) const
{
    if (pointer == AllCategories) {
        return 0;
    } else {
        const auto &categories = core()->categories();
        auto d = static_cast<const Category *>(pointer) - categories.data();
        return (d >= 0 && d < int(categories.size())) ? int(d + 1) : -1;
    }
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
        res = c != AllCategories ? c->name() : tr("All Items");
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

QHash<int, QByteArray> BrickLink::CategoryModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "name" },
        { CategoryPointerRole, "category" },
    };
    return roles;
}

bool BrickLink::CategoryModel::isFiltered() const
{
    return filterItemType() || filterWithoutInventory();
}

const BrickLink::ItemType *BrickLink::CategoryModel::filterItemType() const
{
    return m_itemtype_filter;
}

void BrickLink::CategoryModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    emit isFilteredChanged();
    invalidateFilter();
}

bool BrickLink::CategoryModel::filterWithoutInventory() const
{
    return m_inv_filter;
}

void BrickLink::CategoryModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    emit isFilteredChanged();
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

    if (!c)
        return false;
    else if (c == AllCategories)
        return true;
    else if (m_inv_filter && !c->hasInventories())
        return false;
    else if (m_itemtype_filter && !m_itemtype_filter->categories().contains(c))
        return false;
    else if (m_inv_filter && m_itemtype_filter && !c->hasInventories(m_itemtype_filter))
        return false;
    else
        return true;
}


/////////////////////////////////////////////////////////////
// ITEMTYPEMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemTypeModel::ItemTypeModel(QObject *parent)
    : StaticPointerModel(parent)
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
    return &core()->itemTypes()[size_t(index)];
}

int BrickLink::ItemTypeModel::pointerIndexOf(const void *pointer) const
{
    const auto &itemTypes = core()->itemTypes();
    auto d = static_cast<const ItemType *>(pointer) - itemTypes.data();
    return (d >= 0 && d < int(itemTypes.size())) ? int(d) : -1;
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

QHash<int, QByteArray> BrickLink::ItemTypeModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "name" },
        { ItemTypePointerRole, "itemType" },
    };
    return roles;
}

bool BrickLink::ItemTypeModel::isFiltered() const
{
    return filterWithoutInventory();
}

bool BrickLink::ItemTypeModel::filterWithoutInventory() const
{
    return m_inv_filter;
}

void BrickLink::ItemTypeModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    emit isFilteredChanged();
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

QString BrickLink::ItemModel::s_appearsInPrefix;
QString BrickLink::ItemModel::s_consistsOfPrefix;
QString BrickLink::ItemModel::s_idPrefix;


BrickLink::ItemModel::ItemModel(QObject *parent)
    : StaticPointerModel(parent)
{
    MODELTEST_ATTACH(this)

    if (s_consistsOfPrefix.isEmpty()) {
        s_consistsOfPrefix = tr("consists-of:", "Filter prefix");
        s_appearsInPrefix = tr("appears-in:", "Filter prefix");
        s_idPrefix = tr("id:", "Id prefix");
    }

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
    return &core()->items().at(size_t(index));
}

int BrickLink::ItemModel::pointerIndexOf(const void *pointer) const
{
    const auto &items = core()->items();
    auto d = static_cast<const Item *>(pointer) - items.data();
    return (d >= 0 && d < int(items.size())) ? int(d) : -1;
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
        case 1: res = QLatin1String(i->id()); break;
        case 2: res = i->name(); break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == 0) {
            return Qt::AlignCenter;
        }
    } else if (role == Qt::ToolTipRole || role == NameRole) {
        if (index.column() == 0)
            res = i->name();
    } else if (role == IdRole) {
        res = QLatin1String(i->id());
    } else if (role == ItemPointerRole) {
        res.setValue(i);
    } else if (role == ItemTypePointerRole) {
        res.setValue(i->itemType());
    } else if (role == CategoryPointerRole) {
        res.setValue(i->category());
    }
    return res;
}

QVariant BrickLink::ItemModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch(section) {
        case 1: return tr("Item-Id");
        case 2: return tr("Description");
        }
    }
    return QVariant();
}

QHash<int, QByteArray> BrickLink::ItemModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { IdRole, "id" },
        { NameRole, "name" },
        { ItemPointerRole, "item" },
        { ItemTypePointerRole, "itemType" },
        { CategoryPointerRole, "category" },
    };
    return roles;
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
    return m_itemtype_filter || m_category_filter || m_color_filter
            || m_inv_filter || !m_text_filter.isEmpty();
}

const BrickLink::ItemType *BrickLink::ItemModel::filterItemType() const
{
    return m_itemtype_filter;
}

void BrickLink::ItemModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;

    m_itemtype_filter = it;
    emit isFilteredChanged();
    invalidateFilter();
}

const BrickLink::Category *BrickLink::ItemModel::filterCategory() const
{
    return m_category_filter;
}

void BrickLink::ItemModel::setFilterCategory(const Category *cat)
{
    if (cat == m_category_filter)
        return;
    m_category_filter = cat;
    emit isFilteredChanged();
    invalidateFilter();
}

const BrickLink::Color *BrickLink::ItemModel::filterColor() const
{
    return m_color_filter;
}

void BrickLink::ItemModel::setFilterColor(const BrickLink::Color *col)
{
    if (col == m_color_filter)
        return;
    m_color_filter = col;
    emit isFilteredChanged();
    invalidateFilter();
}

QString BrickLink::ItemModel::filterText() const
{
    return m_text_filter;
}

void BrickLink::ItemModel::setFilterText(const QString &filter)
{
    if (filter == m_text_filter)
        return;

    m_text_filter = filter;
    m_filter_text.clear();
    m_filter_appearsIn.clear();
    m_filter_consistsOf.clear();
    m_filter_ids.second.clear();
    m_filter_ids.first = false;

    const QStringList sl = filter.simplified().split(u' ');

    QString quoted;
    bool quotedNegate = false;

    for (const auto &s : sl) {
        if (s.isEmpty())
            continue;

        if (!quoted.isEmpty()) {
            quoted = quoted % u' ' % s;
            if (quoted.endsWith(u'"')) {
                quoted.chop(1);
                m_filter_text << qMakePair(quotedNegate, quoted);
                quoted.clear();
            }
        } else if (s.length() == 1) {
            // just a single character -> search for it literally
            m_filter_text << qMakePair(false, s);

        } else {
            const QChar first = s.at(0);
            const bool negate = (first == u'-');
            auto str = negate ? s.mid(1) : s;

            if (str.startsWith(s_consistsOfPrefix)) {
                str = str.mid(s_consistsOfPrefix.length());

                // contains either a minifig or a part, optionally with color-id
                const BrickLink::Color *color = nullptr;

                auto atPos = str.lastIndexOf(u'@');
                if (atPos != -1) {
                    color = BrickLink::core()->color(str.mid(atPos + 1).toUInt());
                    str = str.left(atPos);
                }

                if (auto item = BrickLink::core()->item("MP", str.toLatin1()))
                    m_filter_consistsOf << qMakePair(negate, qMakePair(item, color));

            } else if (str.startsWith(s_appearsInPrefix)) {
                str = str.mid(s_appearsInPrefix.length());

                // appears-in either a minifig or a set
                if (auto item = BrickLink::core()->item("MS", str.toLatin1()))
                    m_filter_appearsIn << qMakePair(negate, item);

            } else if (str.startsWith(s_idPrefix)) {
                str = str.mid(s_idPrefix.length());
                const auto ids = str.split(u","_qs);

                for (const auto &id : ids) {
                    if (auto item = BrickLink::core()->item("MPSG", id.toLatin1()))
                        m_filter_ids.second << item;
                }
                m_filter_ids.first = negate;

            } else {
                const bool firstIsQuote = str.startsWith(u"\"");
                const bool lastIsQuote = str.endsWith(u"\"");

                if (firstIsQuote && !lastIsQuote) {
                    quoted = str.mid(1);
                    quotedNegate = negate;
                } else if (firstIsQuote && lastIsQuote) {
                    m_filter_text << qMakePair(negate, str.mid(1, str.length() - 2));
                } else {
                    m_filter_text << qMakePair(negate, str);
                }
            }
        }
    }

    emit isFilteredChanged();
    invalidateFilter();
}

bool BrickLink::ItemModel::filterWithoutInventory() const
{
    return m_inv_filter;
}

void BrickLink::ItemModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    emit isFilteredChanged();
    invalidateFilter();
}

void BrickLink::ItemModel::setFilterYearRange(int minYear, int maxYear)
{
    if (m_year_min_filter == minYear && m_year_max_filter == maxYear)
        return;
    if (minYear && maxYear && minYear > maxYear)
        std::swap(minYear, maxYear);
    m_year_min_filter = minYear;
    m_year_max_filter = maxYear;
    invalidateFilter();
}

bool BrickLink::ItemModel::lessThan(const void *p1, const void *p2, int column) const
{
    const Item *i1 = static_cast<const Item *>(p1);
    const Item *i2 = static_cast<const Item *>(p2);

    return Utility::naturalCompare((column == 2) ? i1->name() : QLatin1String(i1->id()),
                                   (column == 2) ? i2->name() : QLatin1String(i2->id())) < 0;
}

bool BrickLink::ItemModel::filterAccepts(const void *pointer) const
{
    const Item *item = static_cast<const Item *>(pointer);

    if (!item)
        return false;
    else if (m_itemtype_filter && item->itemType() != m_itemtype_filter)
        return false;
    else if (m_category_filter && (m_category_filter != BrickLink::CategoryModel::AllCategories) && !item->additionalCategories(true).contains(m_category_filter))
        return false;
    else if (m_inv_filter && !item->hasInventory())
        return false;
    else if (m_color_filter && !item->hasKnownColor(m_color_filter))
        return false;
    else if (m_year_min_filter && (!item->yearReleased() || (item->yearReleased() < m_year_min_filter)))
        return false;
    else if (m_year_max_filter && (!item->yearLastProduced() || (item->yearLastProduced() > m_year_max_filter)))
        return false;
    else {
        const QString matchStr = QLatin1String(item->id()) % u' ' % item->name();

        // .first is always "bool negate"

        bool match = true;
        for (const auto &p : m_filter_text)
            match = match && (matchStr.contains(p.second, Qt::CaseInsensitive) == !p.first); // contains() xor negate

        bool idMatched = m_filter_ids.second.isEmpty();
        for (const auto &i : m_filter_ids.second) {
            if (i == item) {
                idMatched = true;
                break;
            }
        }
        match = match && (idMatched == !m_filter_ids.first); // found xor negate

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
            const auto &containslist = item->consistsOf();
            for (const auto &ci : containslist) {
                if ((ci.item() == c.second.first)
                        && (!c.second.second || (ci.color() == c.second.second))) {
                    found = true;
                    break;
                }
            }
            match = match && (found == !c.first); // found xor negate
        }

        return match;
    }
}


/////////////////////////////////////////////////////////////
// APPEARSINMODEL
/////////////////////////////////////////////////////////////


BrickLink::InternalAppearsInModel::InternalAppearsInModel(const QVector<QPair<const Item *,
                                                          const Color *>> &list, QObject *parent)
    : QAbstractTableModel(parent)
{
    MODELTEST_ATTACH(this)

    QHash<const Item *, int> unique;
    bool first_item = true;
    bool single_item = (list.count() == 1);

    for (const auto &p : list) {
        if (!p.first)
            continue;

        const auto appearsvec = p.first->appearsIn(p.second);
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

    return ai ? createIndex(int(m_items.indexOf(ai)), 0, ai) : QModelIndex();
}

int BrickLink::InternalAppearsInModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
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

    switch (role) {
    case Qt::DisplayRole:
        switch (col) {
        case 0: res = appears->first < 0 ? u"-"_qs : QString::number(appears->first); break;
        case 1: res = appears->second->id(); break;
        case 2: res = appears->second->name(); break;
        }
        break;
    case BrickLink::AppearsInItemPointerRole:
        res.setValue(appears);
        break;
    case BrickLink::ItemPointerRole:
        res.setValue(appears->second);
        break;
    case BrickLink::ColorPointerRole:
        res.setValue(appears->second->defaultColor());
        break;
    case QuantityRole:
        res = qMax(0, appears->first);
        break;
    case NameRole:
        res = appears->second->name();
        break;
    case IdRole:
        res = appears->second->id();
        break;
    default:
        break;
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

QHash<int, QByteArray> BrickLink::InternalAppearsInModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { IdRole, "id" },
        { NameRole, "name" },
        { QuantityRole, "quantity" },
        { ItemPointerRole, "item" },
        { ColorPointerRole, "color" },
    };
    return roles;
}

BrickLink::AppearsInModel::AppearsInModel(const QVector<QPair<const Item *, const Color *>> &list,
                                          QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new InternalAppearsInModel(list, this));
}

BrickLink::AppearsInModel::AppearsInModel(const Item *item, const Color *color, QObject *parent)
    : AppearsInModel({ { item, color } }, parent)
{ }

int BrickLink::AppearsInModel::count() const
{
    return rowCount();
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
        case  1: return (Utility::naturalCompare(QLatin1String(ai1->second->id()),
                                                 QLatin1String(ai2->second->id())) < 0);
        case  2: return (Utility::naturalCompare(ai1->second->name(),
                                                 ai2->second->name()) < 0);
        }
    }
}

#include "moc_model.cpp"
