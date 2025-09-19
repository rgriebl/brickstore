// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QBuffer>
#include <QtCore/QStringBuilder>
#include <QtCore/QRegularExpression>
#include <QtCore/QBitArray>
#include <QtConcurrent/QtConcurrentMap>
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
#include "bricklink/model_p.h"

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif


namespace BrickLink {

/////////////////////////////////////////////////////////////
// COLORMODEL
/////////////////////////////////////////////////////////////

ColorModel::ColorModel(QObject *parent)
    : StaticPointerModel(parent)
{
    MODELTEST_ATTACH(this)
}

int ColorModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int ColorModel::pointerCount() const
{
    return int(core()->colors().size());
}

const void *ColorModel::pointerAt(int index) const
{
    return &core()->colors()[size_t(index)];
}

int ColorModel::pointerIndexOf(const void *pointer) const
{
    const auto &colors = core()->colors();
    auto d = static_cast<const Color *>(pointer) - colors.data();
    return (d >= 0 && d < int(colors.size())) ? int(d) : -1;
}

const Color *ColorModel::color(const QModelIndex &index) const
{
    return static_cast<const Color *>(pointer(index));
}

QModelIndex ColorModel::index(const Color *color) const
{
    return index(static_cast<const void *>(color));
}

QVariant ColorModel::data(const QModelIndex &index, int role) const
{
    const Color *c = color(index);
    if (!c)
        return { };

    if ((role == Qt::DisplayRole) || (role == Qt::EditRole)) {
        return c->name();
    } else if (role == Qt::DecorationRole) {
        QFontMetricsF fm(QGuiApplication::font());
        QImage img = c->sampleImage(int(fm.height()) + 4, int(fm.height()) + 4);
        if (!img.isNull()) {
            QPixmap pix = QPixmap::fromImage(img);
            QIcon ico;
            ico.addPixmap(pix, QIcon::Normal);
            ico.addPixmap(pix, QIcon::Selected);
            return ico;
        }
    } else if (role == ColorPointerRole) {
        return QVariant::fromValue(c);
    } else if (role == PinnedRole) {
        return m_pinnedColorIds.contains(c->id());
    }
    return { };
}

bool ColorModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && (role == PinnedRole)) {
        if (auto c = color(index)) {
            pinId(c->id(), value.toBool());
            emit dataChanged(index, index, { PinnedRole });
            return true;
        }
    }
    return false;
}

QVariant ColorModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Color by %1").arg(sortOrder() == Qt::AscendingOrder ? tr("Name") : tr("Hue"));
    return { };
}

bool ColorModel::isFiltered() const
{
    return m_colorTypeFilter || !qFuzzyIsNull(m_popularityFilter) || !m_colorListFilter.isEmpty();
}

void ColorModel::clearFilters()
{
    if (isFiltered()) {
        m_popularityFilter = 0;
        m_colorTypeFilter = ColorType();
        m_colorListFilter.clear();
        emit colorTypeFilterChanged();
        emit popularityFilterChanged();
        emit colorListFilterChanged();
        invalidateFilter();
    }
}

ColorType ColorModel::colorTypeFilter() const
{
    return m_colorTypeFilter;
}

void ColorModel::setColorTypeFilter(ColorType type)
{
    if (type == m_colorTypeFilter)
        return;
    m_colorTypeFilter = type;
    emit colorTypeFilterChanged();
    invalidateFilter();
}

float ColorModel::popularityFilter() const
{
    return m_popularityFilter;
}

void ColorModel::setPopularityFilter(float p)
{
    if (qFuzzyCompare(p, m_popularityFilter))
        return;
    m_popularityFilter = p;
    emit popularityFilterChanged();
    invalidateFilter();
}

const QVector<const Color *> ColorModel::colorListFilter() const
{
    return m_colorListFilter;
}

void ColorModel::setColorListFilter(const QVector<const Color *> &colorList)
{
    if (colorList == m_colorListFilter)
        return;
    m_colorListFilter = colorList;
    emit colorListFilterChanged();
    invalidateFilter();
}

void ColorModel::pinId(uint colorId, bool down)
{
    auto countBefore = m_pinnedColorIds.count();

    if (down)
        m_pinnedColorIds.insert(colorId);
    else
        m_pinnedColorIds.remove(colorId);

    if (m_pinnedColorIds.count() != countBefore) {
        forceSort();
        emit pinnedIdsChanged();
    }
}

void ColorModel::setPinnedIds(const QSet<uint> &colorIds)
{
    if (m_pinnedColorIds != colorIds) {
        m_pinnedColorIds = colorIds;
        forceSort();
        emit pinnedIdsChanged();
    }
}

QSet<uint> ColorModel::pinnedIds() const
{
    return m_pinnedColorIds;
}

bool ColorModel::lessThan(const void *p1, const void *p2, int /*column*/, Qt::SortOrder order) const
{
    const auto *c1 = static_cast<const Color *>(p1);
    const auto *c2 = static_cast<const Color *>(p2);
    bool asc = (order == Qt::AscendingOrder);

    if (c1 == c2)
        return false;
    else if (!c1)
        return true;
    else if (!c2)
        return false;
    else {
        bool isPinned1 = m_pinnedColorIds.contains(c1->id());
        bool isPinned2 = m_pinnedColorIds.contains(c2->id());
        if (isPinned1 != isPinned2)
            return asc ? (isPinned1 > isPinned2) : (isPinned1 < isPinned2);

        if (asc) {
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

bool ColorModel::filterAccepts(const void *pointer) const
{
    const auto *color = static_cast<const Color *>(pointer);

    if (m_colorTypeFilter && !(color->type() & m_colorTypeFilter))
        return false;
    if (!qFuzzyIsNull(m_popularityFilter) && (color->popularity() < m_popularityFilter))
        return false;
    if (!m_colorListFilter.isEmpty() && !m_colorListFilter.contains(color))
        return false;

    return true;
}



/////////////////////////////////////////////////////////////
// CATEGORYMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const Category *CategoryModel::AllCategories = reinterpret_cast <const Category *>(-1);

CategoryModel::CategoryModel(QObject *parent)
    : StaticPointerModel(parent)
{
    MODELTEST_ATTACH(this)
}

int CategoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int CategoryModel::pointerCount() const
{
    return int(core()->categories().size() + 1);
}

const void *CategoryModel::pointerAt(int index) const
{
    return (index == 0) ? AllCategories : &core()->categories()[size_t(index) - 1];
}

int CategoryModel::pointerIndexOf(const void *pointer) const
{
    if (pointer == AllCategories) {
        return 0;
    } else {
        const auto &categories = core()->categories();
        auto d = static_cast<const Category *>(pointer) - categories.data();
        return (d >= 0 && d < int(categories.size())) ? int(d + 1) : -1;
    }
}

const Category *CategoryModel::category(const QModelIndex &index) const
{
    return static_cast<const Category *>(pointer(index));
}

QModelIndex CategoryModel::index(const Category *category) const
{
    return index(static_cast<const void *>(category));
}

QVariant CategoryModel::data(const QModelIndex &index, int role) const
{
    const Category *c = category(index);
    if (!c)
        return { };

    if (role == Qt::DisplayRole)
        return (c != AllCategories) ? c->name() : tr("All Items");
    else if (role == CategoryPointerRole)
        return QVariant::fromValue(c);
    else if ((role == PinnedRole) && (c != AllCategories))
        return m_pinnedCategoryIds.contains(c->id());
    return { };
}

bool CategoryModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && (role == PinnedRole)) {
        if (auto c = category(index)) {
            if (c != AllCategories) {
                pinId(c->id(), value.toBool());
                emit dataChanged(index, index, { PinnedRole });
                return true;
            }
        }
    }
    return false;
}

QVariant CategoryModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Category");
    return { };
}

QHash<int, QByteArray> CategoryModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "name" },
        { CategoryPointerRole, "categoryPointer" },
    };
    return roles;
}

bool CategoryModel::isFiltered() const
{
    return filterItemType() || filterWithoutInventory();
}

const ItemType *CategoryModel::filterItemType() const
{
    return m_itemtype_filter;
}

void CategoryModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    emit isFilteredChanged();
    invalidateFilter();
}

bool CategoryModel::filterWithoutInventory() const
{
    return m_inv_filter;
}

void CategoryModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    emit isFilteredChanged();
    invalidateFilter();
}

void CategoryModel::pinId(uint categoryId, bool down)
{
    auto countBefore = m_pinnedCategoryIds.count();

    if (down)
        m_pinnedCategoryIds.insert(categoryId);
    else
        m_pinnedCategoryIds.remove(categoryId);

    if (m_pinnedCategoryIds.count() != countBefore) {
        forceSort();
        emit pinnedIdsChanged();
    }
}

void CategoryModel::setPinnedIds(const QSet<uint> &categoryIds)
{
    if (m_pinnedCategoryIds != categoryIds) {
        m_pinnedCategoryIds = categoryIds;
        forceSort();
        emit pinnedIdsChanged();
    }
}

QSet<uint> CategoryModel::pinnedIds() const
{
    return m_pinnedCategoryIds;
}

bool CategoryModel::lessThan(const void *p1, const void *p2, int /*column*/, Qt::SortOrder order) const
{
    const auto *c1 = static_cast<const Category *>(p1);
    const auto *c2 = static_cast<const Category *>(p2);
    bool asc = (order == Qt::AscendingOrder);

    if (c1 == c2) {
        return false;
    } else if (!c1 || c1 == AllCategories) {
        return asc;
    } else if (!c2 || c2 == AllCategories) {
        return !asc;
    } else {
        bool isPinned1 = m_pinnedCategoryIds.contains(c1->id());
        bool isPinned2 = m_pinnedCategoryIds.contains(c2->id());
        if (isPinned1 != isPinned2)
            return asc ? (isPinned1 > isPinned2) : (isPinned1 < isPinned2);

        return c1->name().localeAwareCompare(c2->name()) < 0;
    }
}

bool CategoryModel::filterAccepts(const void *pointer) const
{
    const auto *c = static_cast<const Category *>(pointer);

    if (!c)
        return false;
    else if (c == AllCategories)
        return true;
    else if (m_inv_filter && !c->hasInventories())
        return false;
    else if (m_itemtype_filter && (m_itemtype_filter != ItemTypeModel::AllItemTypes) && !m_itemtype_filter->categories().contains(c))
        return false;
    else if (m_inv_filter && m_itemtype_filter && ((m_itemtype_filter == ItemTypeModel::AllItemTypes ? !c->hasInventories() : !c->hasInventories(m_itemtype_filter))))
        return false;
    else
        return true;
}


/////////////////////////////////////////////////////////////
// ITEMTYPEMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const ItemType *ItemTypeModel::AllItemTypes = reinterpret_cast <const ItemType *>(-1);

ItemTypeModel::ItemTypeModel(QObject *parent)
    : StaticPointerModel(parent)
{
    MODELTEST_ATTACH(this)
}

int ItemTypeModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int ItemTypeModel::pointerCount() const
{
    return int(core()->itemTypes().size() + 1);
}

const void *ItemTypeModel::pointerAt(int index) const
{
    return (index == 0) ? AllItemTypes : &core()->itemTypes()[size_t(index) - 1];
}

int ItemTypeModel::pointerIndexOf(const void *pointer) const
{
    if (pointer == AllItemTypes) {
        return 0;
    } else {
        const auto &itemTypes = core()->itemTypes();
        auto d = static_cast<const ItemType *>(pointer) - itemTypes.data();
        return (d >= 0 && d < int(itemTypes.size())) ? int(d + 1) : -1;
    }
}

const ItemType *ItemTypeModel::itemType(const QModelIndex &index) const
{
    return static_cast<const ItemType *>(pointer(index));
}

QModelIndex ItemTypeModel::index(const ItemType *itemtype) const
{
    return index(static_cast<const void *>(itemtype));
}

QVariant ItemTypeModel::data(const QModelIndex &index, int role) const
{
    const ItemType *i = itemType(index);
    if (!i)
        return { };

    if (role == Qt::DisplayRole)
        return (i != AllItemTypes) ? i->name() : tr("Any");
    else if (role == ItemTypePointerRole)
        return QVariant::fromValue(i);
    return { };
}

QVariant ItemTypeModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return { };
}

QHash<int, QByteArray> ItemTypeModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "name" },
        { ItemTypePointerRole, "itemTypePointer" },
    };
    return roles;
}

bool ItemTypeModel::isFiltered() const
{
    return filterWithoutInventory();
}

bool ItemTypeModel::filterWithoutInventory() const
{
    return m_inv_filter;
}

void ItemTypeModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    emit isFilteredChanged();
    invalidateFilter();
}

bool ItemTypeModel::lessThan(const void *p1, const void *p2, int /*column*/, Qt::SortOrder /*order*/) const
{
    const auto *i1 = static_cast<const ItemType *>(p1);
    const auto *i2 = static_cast<const ItemType *>(p2);

    if (i1 == i2)
        return false;
    else if (!i1 || i1 == AllItemTypes)
        return true;
    else if (!i2 || i2 == AllItemTypes)
        return false;
    else
        return i1->name().localeAwareCompare(i2->name()) < 0;
}

bool ItemTypeModel::filterAccepts(const void *pointer) const
{
    const auto *i = static_cast<const ItemType *>(pointer);

    if (!i)
        return false;
    else if (i == AllItemTypes)
        return true;
    else if (m_inv_filter && !i->hasInventories())
        return false;
    else
        return true;
}



/////////////////////////////////////////////////////////////
// ITEMMODEL
/////////////////////////////////////////////////////////////


ItemModel::ItemModel(QObject *parent)
    : StaticPointerModel(parent)
{
    MODELTEST_ATTACH(this)

    connect(core()->pictureCache(), &BrickLink::PictureCache::pictureUpdated,
            this, &ItemModel::pictureUpdated);
}

int ItemModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

int ItemModel::pointerCount() const
{
    return int(core()->items().size());
}

const void *ItemModel::pointerAt(int index) const
{
    return &core()->items().at(size_t(index));
}

int ItemModel::pointerIndexOf(const void *pointer) const
{
    const auto &items = core()->items();
    auto d = static_cast<const Item *>(pointer) - items.data();
    return (d >= 0 && d < int(items.size())) ? int(d) : -1;
}

const Item *ItemModel::item(const QModelIndex &index) const
{
    return static_cast<const Item *>(pointer(index));
}

QModelIndex ItemModel::index(const Item *item) const
{
    return index(static_cast<const void *>(item));
}

QVariant ItemModel::data(const QModelIndex &index, int role) const
{
    const Item *i = item(index);
    if (!i)
        return { };

    if (role == Qt::DisplayRole) {
        switch(index.column()) {
        case 1: return QString::fromLatin1(i->id()); break;
        case 2: return i->name(); break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == 0) {
            return Qt::AlignCenter;
        }
    } else if (role == Qt::ToolTipRole || role == NameRole) {
        if (index.column() == 0)
            return i->name();
    } else if (role == IdRole) {
        return QString::fromLatin1(i->id());
    } else if (role == ItemPointerRole) {
        return QVariant::fromValue(i);
    } else if (role == ItemTypePointerRole) {
        return QVariant::fromValue(i->itemType());
    } else if (role == CategoryPointerRole) {
        return QVariant::fromValue(i->category());
    } else if ((role == ColorPointerRole) && (m_color_filter)) {
        return QVariant::fromValue(m_color_filter);
    }
    return { };
}

QVariant ItemModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch(section) {
        case 1: return tr("Item Id");
        case 2: return tr("Description");
        }
    }
    return { };
}

QHash<int, QByteArray> ItemModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { IdRole, "id" },
        { NameRole, "name" },
        { ItemPointerRole, "itemPointer" },
        { ItemTypePointerRole, "itemTypePointer" },
        { CategoryPointerRole, "categoryPointer" },
        { ColorPointerRole, "colorPointer" },
    };
    return roles;
}

void ItemModel::pictureUpdated(Picture *pic)
{
    if (!pic || !pic->item())
        return;
    if (m_color_filter && (pic->color() != m_color_filter))
        return;
    else if (!m_color_filter && (pic->color() != pic->item()->defaultColor()))
        return;

    QModelIndex idx = index(pic->item());
    if (idx.isValid())
        emit dataChanged(idx, idx);
}

bool ItemModel::isFiltered() const
{
    return m_itemtype_filter || m_category_filter || m_color_filter
            || m_inv_filter || !m_text_filter.isEmpty();
}

const ItemType *ItemModel::filterItemType() const
{
    return m_itemtype_filter;
}

void ItemModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;

    m_itemtype_filter = it;
    emit isFilteredChanged();
    invalidateFilter();
}

const Category *ItemModel::filterCategory() const
{
    return m_category_filter;
}

void ItemModel::setFilterCategory(const Category *cat)
{
    if (cat == m_category_filter)
        return;
    m_category_filter = cat;
    emit isFilteredChanged();
    invalidateFilter();
}

const Color *ItemModel::filterColor() const
{
    return m_color_filter;
}

void ItemModel::setFilterColor(const Color *col)
{
    if (col == m_color_filter)
        return;
    m_color_filter = col;
    emit isFilteredChanged();
    invalidateFilter();
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

QString ItemModel::filterText() const
{
    return m_text_filter;
}

void ItemModel::setFilterText(const QString &filter)
{
    if (filter == m_text_filter)
        return;

    m_text_filter = filter;
    m_filter_terms.clear();
    m_filter_ids.clear();
    m_filter_ids_optional = true;

    const QStringList sl = filter.simplified().split(u' ');

    QString quoted;
    bool quotedNegate = false;
    QVector<const void *> scanOrder;

    for (const auto &s : sl) {
        if (s.isEmpty())
            continue;

        if (!quoted.isEmpty()) {
            quoted = quoted + u' ' + s;
            if (quoted.endsWith(u'"')) {
                quoted.chop(1);
                m_filter_terms << FilterTerm(quotedNegate, quoted);
                quoted.clear();
            }
        } else if (s.length() == 1) {
            // just a single character -> search for it literally
            m_filter_terms << FilterTerm(false, s);

        } else {
            const QChar first = s.at(0);
            const bool negate = (first == u'-');
            auto str = negate ? s.mid(1) : s;

            if (str.startsWith(u"scan:") && (sl.size() == 1) && !negate) {
                str = str.mid(u"scan:"_qs.length());
                const auto ids = str.split(u","_qs);

                // This is a bit of a hack to get a fixed sort order, when the internal filter
                // "scan:" is used. It has to be the only filter expression though, and it
                // cannot be negated.

                for (const auto &id : ids) {
                    if (id.length() < 2)
                        continue;
                    QByteArray ba = id.toLatin1();
                    if (auto item = core()->item(ba.at(0), ba.mid(1))) {
                        m_filter_ids << ba;
                        scanOrder << item;
                    }
                }
                m_filter_ids_optional = false;
            } else {
                const bool firstIsQuote = str.startsWith(u"\"");
                const bool lastIsQuote = str.endsWith(u"\"");

                if (firstIsQuote && !lastIsQuote) {
                    quoted = str.mid(1);
                    quotedNegate = negate;
                } else if (firstIsQuote && lastIsQuote) {
                    m_filter_terms << FilterTerm(negate, str.mid(1, str.length() - 2));
                } else {
                    m_filter_terms << FilterTerm(negate, str);
                }
            }
        }
    }
    // check for PCC ids
    for (const auto &ft : std::as_const(m_filter_terms)) {

        bool ok = false;
        uint pccId = ft.m_text.toUInt(&ok);
        if (ok && pccId) {
            const auto &[pccItem, pccColor] = core()->partColorCode(pccId);
            if (pccItem)
                m_filter_ids << QByteArray(pccItem->itemTypeId() + pccItem->id());
        }
    }

    emit isFilteredChanged();
    invalidateFilter();

    setFixedSortOrder(scanOrder);
}

bool ItemModel::filterWithoutInventory() const
{
    return m_inv_filter;
}

void ItemModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    emit isFilteredChanged();
    invalidateFilter();
}

void ItemModel::setFilterYearRange(int minYear, int maxYear)
{
    if (m_year_min_filter == minYear && m_year_max_filter == maxYear)
        return;
    if (minYear && maxYear && minYear > maxYear)
        std::swap(minYear, maxYear);
    m_year_min_filter = minYear;
    m_year_max_filter = maxYear;
    invalidateFilter();
}

bool ItemModel::lessThan(const void *p1, const void *p2, int column, Qt::SortOrder /*order*/) const
{
    const Item *i1 = static_cast<const Item *>(p1);
    const Item *i2 = static_cast<const Item *>(p2);

    return ((column == 2) ? Utility::naturalCompare(i1->name(), i2->name())
                          : Utility::naturalCompare(QLatin1StringView { i1->id() },
                                                    QLatin1StringView { i2->id() })) < 0;
}

bool ItemModel::filterAccepts(const void *pointer) const
{
    const Item *item = static_cast<const Item *>(pointer);

    if (!item)
        return false;
    else if (m_itemtype_filter && (m_itemtype_filter != ItemTypeModel::AllItemTypes) && (item->itemType() != m_itemtype_filter))
        return false;
    else if (m_category_filter && (m_category_filter != CategoryModel::AllCategories) && !item->categories(true).contains(m_category_filter))
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
        QString matchStr = QString::fromLatin1(item->id()) + u' ' + item->name();
        if (item->hasAlternateIds())
            matchStr = matchStr + u' ' + QString::fromLatin1(item->alternateIds());

        bool match = true;
        for (const auto &ft : m_filter_terms)
            match = match && (matchStr.contains(ft.m_text, Qt::CaseInsensitive) == !ft.m_negate); // contains() xor negate

        bool idMatched = m_filter_ids.isEmpty() && !m_filter_ids_optional;
        for (const auto &id : m_filter_ids) {
            if (id == QByteArray(item->itemTypeId() + item->id())) {
                idMatched = true;
                break;
            }
        }

        if (m_filter_ids_optional)
            return match || idMatched;
        else
            return match && idMatched;
    }
}


/////////////////////////////////////////////////////////////
// INTERNAL INVENTORYMODEL
/////////////////////////////////////////////////////////////


InternalInventoryModel::InternalInventoryModel(Mode mode, const QVector<SimpleLot> &list, QObject *parent)
    : QAbstractItemModel(parent)
    , m_mode(mode)
{
    MODELTEST_ATTACH(this)

    switch (mode) {
        case Mode::ConsistsOf:    fillConsistsOf(list); break;
        case Mode::AppearsIn:     fillAppearsIn(list); break;
        case Mode::CanBuild:      fillCanBuild(list); break;
        case Mode::Relationships: fillRelationships(list); break;
    }
    connect(core()->pictureCache(), &BrickLink::PictureCache::pictureUpdated,
            this, [this](Picture *pic) {
        if (!pic || !pic->item())
            return;

        QVector<QModelIndex> indexes;

        auto pictureMatch = [](const Entry *entry, const Picture *picture) {
            return (entry->m_item == picture->item())
                   && ((entry->m_color && (entry->m_color == picture->color()))
                       || (!entry->m_color && (picture->color() == entry->m_item->defaultColor())));
        };

        for (int row = 0; row < m_entries.size(); ++row) {
            auto *e = m_entries.at(row);
            if (!e->isSection() && pictureMatch(e, pic))
                indexes << index(row, InventoryModel::PictureColumn); // clazy:exclude=reserve-candidates
        }

        for (const auto &idx : std::as_const(indexes))
            emit dataChanged(idx, idx);
    });
}

void InternalInventoryModel::fillConsistsOf(const QVector<SimpleLot> &list)
{
    QHash<std::pair<const Item *, const Color *>, Entry *> unique;

    for (const auto &p : list) {
        if (!p.m_item)
            continue;

        if (!p.m_item->hasInventory())
            continue;

        const auto consistsvec = p.m_item->consistsOf();

        for (const auto &coi : consistsvec) {
            if (coi.isExtra() || coi.isCounterPart() || coi.isAlternate())
                continue;

            const auto *partItem = coi.item();
            const auto *partColor = coi.color();

            if (p.m_color && p.m_color->id() && partItem->itemType()->hasColors()
                    && partColor && (partColor->id() == 0)) {
                partColor = p.m_color;
            }

            const auto key = std::make_pair(partItem, partColor);

            auto it = unique.find(key);
            if (it != unique.end())
                it.value()->m_quantity += coi.quantity();
            else
                unique.emplace(key, new Entry { partItem, partColor, coi.quantity() });
        }
    }

    m_entries = unique.values();  // clazy:exclude=qt6-deprecated-api-fixes
}

void InternalInventoryModel::fillAppearsIn(const QVector<SimpleLot> &list)
{
    QSet<const BrickLink::Item *> listSet;
    bool firstItem = true;
    bool singleItem = (list.count() == 1);

    for (const auto &p : list) {
        if (!p.m_item)
            continue;

        // a set could have more than one instance of the same item in alternate sets
        QSet<const BrickLink::Item *> itemSet;

        const auto appearsvec = p.m_item->appearsIn(p.m_color);
        for (const AppearsInColor &vec : appearsvec) {
            for (const AppearsInItem &aii : vec) {
                if (singleItem)
                    m_entries.emplace_back(new Entry { aii.second, nullptr, aii.first });
                else
                    itemSet.insert(aii.second);
            }
        }
        if (firstItem) {
            firstItem = false;
            listSet = itemSet;
        } else {
            listSet.intersect(itemSet);
        }
    }

    if (!listSet.isEmpty()) {
        m_entries.resize(listSet.size());
        std::transform(listSet.cbegin(), listSet.cend(), m_entries.begin(),
                       [](const auto &i) { return new Entry { i, nullptr, -1 }; });
    }
}

void InternalInventoryModel::fillCanBuild(const QVector<SimpleLot> &lots)
{
    QVector<std::pair<quint32, qint32>> have;
    have.reserve(lots.size());
    for (const auto &lot : lots) {
        if (!lot.m_item || !lot.m_color || lot.m_quantity <= 0)
            continue;

        // squeeze the data as tightly as possible: this map/reduce is quite CPU intensive and
        // comparing 32 bits instead of 128 for each check helps a lot (plus it also keeps more
        // data in the cache, as each entry is only 64 bits instead of 192).
        have.append({ (quint32(lot.m_color->index()) << 20) | quint32(lot.m_item->index()),
                     lot.m_quantity });
    }

    static auto indexCompare = [](const std::pair<quint32, qint32> &a,
            const std::pair<quint32, qint32> &b) -> bool {
        return a.first < b.first;
    };

    // sort by colorindex | itemindex
    std::sort(have.begin(), have.end(), indexCompare);

    auto map = [=](const Item &set) -> const Item * {
        bool matched = false;

        if (set.hasInventory()) {
            const auto inv = set.consistsOf();

            // copy the have vector, as we need to modify it for counting down quantities
            auto checkHave = have;

            QBitArray alternatesMatched;

            for (const auto &co : inv) {
                if (co.isExtra() || co.isCounterPart())
                    continue;

                auto alternate = qsizetype(co.alternateId());
                if (alternate) {
                    if (alternatesMatched.size() < alternate)
                        alternatesMatched.resize(alternate);
                    else if (alternatesMatched.at(alternate - 1))
                        continue;
                }

                quint32 index = (co.colorIndex() << 20) | co.itemIndex();
                auto it = std::lower_bound(checkHave.begin(), checkHave.end(),
                                           std::make_pair(index, 0), indexCompare);
                if ((it != checkHave.end()) && (it->first == index)) {
                    it->second -= co.quantity();
                    if (it->second >= 0) {
                        matched = true;
                        if (alternate)
                            alternatesMatched.setBit(alternate - 1);
                        continue;
                    }
                }
                if (!alternate) {
                    matched = false;
                    break;
                }
            }

            // if we had alternatives, make sure all of them matched up
            if (matched && !alternatesMatched.isEmpty())
                matched = (alternatesMatched.count(true) == alternatesMatched.count());
        }
        return matched ? &set : nullptr;
    };

    const auto &sets = core()->items();
    QtConcurrent::mapped(sets.cbegin(), sets.cend(), map)
            .then(this, [this](QFuture<const BrickLink::Item *> future) {
        if (!m_entries.isEmpty() || (m_mode != Mode::CanBuild))
            return;

        beginResetModel();
        for (const auto *match : std::as_const(future)) {
            if (match)
                m_entries.emplace_back(new Entry { match, nullptr, -1 });
        }
        endResetModel();
    });
}

void InternalInventoryModel::fillRelationships(const QVector<SimpleLot> &lots)
{
    QSet<const RelationshipMatch *> allMatches;
    QSet<const Item *> items;
    bool first = true;

    for (const auto &p : lots) {
        if (!p.m_item)
            continue;
        items.insert(p.m_item);
        const auto matches = p.m_item->relationshipMatches();
        if (matches.isEmpty())
            continue;
        QSet<const RelationshipMatch *> matchSet(matches.cbegin(), matches.cend());

        if (first) {
            first = false;
            allMatches = matchSet;
        } else {
            allMatches.intersect(matchSet);
            if (allMatches.isEmpty())
                break;
        }
    }

    if (!allMatches.isEmpty()) {
        beginResetModel();

        for (const RelationshipMatch *match : std::as_const(allMatches)) {
            if (auto rel = core()->relationship(match->relationshipId())) {
                m_entries.emplace_back(new Entry { rel->name() });
                const auto matchItems = match->items();
                for (auto item : matchItems) {
                    if (!items.contains(item))
                        m_entries.emplace_back(new Entry { item, nullptr, -1 });
                }
            }
        }

        endResetModel();
    }
}

InternalInventoryModel::~InternalInventoryModel()
{
    qDeleteAll(m_entries);
}

QModelIndex InternalInventoryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid() && hasIndex(row, column, parent))
        return createIndex(row, column, m_entries.at(row));
    return { };
}

QModelIndex InternalInventoryModel::parent(const QModelIndex &) const
{
    return { };
}

const InternalInventoryModel::Entry *InternalInventoryModel::entry(const QModelIndex &idx) const
{
    return idx.isValid() && idx.constInternalPointer()
               ? static_cast<const Entry *>(idx.constInternalPointer()) : nullptr;
}

int InternalInventoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

int InternalInventoryModel::columnCount(const QModelIndex &) const
{
    return InventoryModel::ColumnCount;
}

QVariant InternalInventoryModel::data(const QModelIndex &index, int role) const
{
    const Entry *e = entry(index);
    if (!e)
        return { };

    if (e->isSection()) {
        if (role == Qt::DisplayRole && index.column() == 0)
            return e->m_sectionTitle;
        else if (role == IsSectionHeaderRole)
            return true;
        return { };
    }

    if (!e->m_item)
        return { };

    switch (role) {
    case IsSectionHeaderRole:
        return false;
    case Qt::DisplayRole:
        switch (index.column()) {
        case InventoryModel::QuantityColumn:
            return (e->m_quantity < 0) ? u"-"_qs : QString::number(e->m_quantity);
        case InventoryModel::ItemIdColumn:
            return QString(QChar::fromLatin1(e->m_item->itemTypeId()) + u' ' + QString::fromLatin1(e->m_item->id()));
        case InventoryModel::ItemNameColumn:
            return e->m_item->name();
        case InventoryModel::ColorColumn:
            return e->m_color ? e->m_color->name() : u"-"_qs;
        default: return { };
        }
    case Qt::DecorationRole:
        switch (index.column()) { //TODO: size
        case InventoryModel::ColorColumn:
            return e->m_color ? e->m_color->sampleImage(20, 20) : QImage { };
        default:
            return { };
        }
    case ItemPointerRole:
        return (index.column() != InventoryModel::ColorColumn)
                ? QVariant::fromValue(e->m_item) : QVariant { };
    case ColorPointerRole:
        return QVariant::fromValue((m_mode == Mode::ConsistsOf)
                                   ? e->m_color : e->m_item->defaultColor());
    case QuantityRole:
        return std::max(0, e->m_quantity);
    case NameRole:
        return e->m_item->name();
    case IdRole:
        return e->m_item->id();
    case ColorNameRole:
        return e->m_color ? e->m_color->name() : QString { };
    default:
        return { };
    }
}

QVariant InternalInventoryModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch (section) {
        case InventoryModel::QuantityColumn: return tr("Qty.");
        case InventoryModel::ColorColumn:    return tr("Color");
        case InventoryModel::ItemIdColumn:   return tr("Item Id");
        case InventoryModel::ItemNameColumn: return tr("Description");
        }
    }
    return { };
}

Qt::ItemFlags InternalInventoryModel::flags(const QModelIndex &index) const
{
    auto flags = QAbstractItemModel::flags(index);
    const Entry *e = entry(index);
    if (e && e->isSection()) {
        flags.setFlag(Qt::ItemIsSelectable, false);
        flags.setFlag(Qt::ItemIsEnabled, false);
    }
    return flags;
}

QHash<int, QByteArray> InternalInventoryModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { IdRole, "id" },
        { NameRole, "name" },
        { ColorNameRole, "colorName" },
        { QuantityRole, "quantity" },
        { ItemPointerRole, "itemPointer" },
        { ColorPointerRole, "colorPointer" },
        { IsSectionHeaderRole, "isSectionHeader" },
        { Qt::DisplayRole, "sectionTitle" },
    };
    return roles;
}


/////////////////////////////////////////////////////////////
// INVENTORY MODEL
/////////////////////////////////////////////////////////////

InventoryModel::InventoryModel(Mode mode, const QVector<SimpleLot> &simpleLots, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new InternalInventoryModel(mode, simpleLots, this));
    connect(this, &QAbstractItemModel::modelReset,
            this, [this]() { emit countChanged(count()); });
}

int InventoryModel::count() const
{
    return rowCount();
}

bool InventoryModel::hasSections() const
{
    return (mode() == Mode::Relationships);
}

InventoryModel::Mode InventoryModel::mode() const
{
    return static_cast<const InternalInventoryModel *>(sourceModel())->m_mode;
}

void InventoryModel::sort(int column, Qt::SortOrder order)
{
    if (mode() != Mode::Relationships)
        QSortFilterProxyModel::sort(column, order);
}

bool InventoryModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshaling
    const auto *iim = static_cast<const InternalInventoryModel *>(sourceModel());
    const auto e1 = iim->entry(left);
    const auto e2 = iim->entry(right);

    if (e1 == e2)
        return false;
    else if (!e1 || !e1->m_item)
        return true;
    else if (!e2 || !e2->m_item)
        return false;
    else {
        switch (left.column()) {
        default:
        case InventoryModel::QuantityColumn:
            return e1->m_quantity < e2->m_quantity;
        case InventoryModel::ItemIdColumn:
            return (Utility::naturalCompare(QLatin1StringView { e1->m_item->id() },
                                            QLatin1StringView { e2->m_item->id() }) < 0);
        case InventoryModel::ItemNameColumn:
            return (Utility::naturalCompare(e1->m_item->name(), e2->m_item->name()) < 0);
        case InventoryModel::ColorColumn:
            return !e1->m_color ? true : (!e2->m_color ? false : e1->m_color->name() < e2->m_color->name());
        }
    }
}

InternalInventoryModel::Entry::Entry(const Item *item, const Color *color, int quantity)
    : m_item(item)
    , m_color(color)
    , m_quantity(quantity)
{ }

InternalInventoryModel::Entry::~Entry()
{ }

InternalInventoryModel::Entry::Entry(const QString &sectionTitle)
    : m_sectionTitle(sectionTitle)
{ }

} // namespace BrickLink

#include "moc_model.cpp"
