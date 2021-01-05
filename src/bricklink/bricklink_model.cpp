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
#include <QThreadStorage>
#include <QHelpEvent>
#include <QAbstractItemView>
#include <QApplication>
#include <QRegularExpression>
#include <QToolTip>
#include <QLabel>
#include <QDesktopWidget>
#include <QBuffer>

#include "bricklink.h"
#include "bricklink_model.h"
#include "utility.h"

#if defined(MODELTEST)
#  include "modeltest.h"
#  define MODELTEST_ATTACH(x)   { (void) new ModelTest(x, x); }
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
    return core()->colors().count();
}

const void *BrickLink::ColorModel::pointerAt(int index) const
{
    return *(core()->colors().constBegin() + index);
}

int BrickLink::ColorModel::pointerIndexOf(const void *pointer) const
{
    int index = 0;
    for (auto it = core()->colors().constBegin(); it != core()->colors().constEnd(); ++it, ++index) {
        if (*it == static_cast<const Color *>(pointer))
            return index;
    }
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
        if (!img.isNull())
            res = img;
    }
    else if (role == Qt::ToolTipRole) {
        res = QString(R"(<table width="100%" border="0" bgcolor="%3"><tr><td><br><br></td></tr></table><br />%1: %2)").arg(tr("RGB"), c->color().name(), c->color().name()); //%
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
    return m_itemtype_filter || m_type_filter || !qFuzzyIsNull(m_popularity_filter);
}

void BrickLink::ColorModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::ColorModel::setFilterType(Color::Type type)
{
    if (type == m_type_filter)
        return;
    m_type_filter = type;
    invalidateFilter();
}

void BrickLink::ColorModel::unsetFilterType()
{
    if (!m_type_filter)
        return;
    m_type_filter = Color::Type();
    invalidateFilter();
}

void BrickLink::ColorModel::setFilterPopularity(qreal p)
{
    if (qFuzzyCompare(p, m_popularity_filter))
        return;
    m_popularity_filter = p;
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
    return core()->categories().count() + 1;
}

const void *BrickLink::CategoryModel::pointerAt(int index) const
{
    return index == 0 ? AllCategories : *(core()->categories().constBegin() + index - 1);
}

int BrickLink::CategoryModel::pointerIndexOf(const void *pointer) const
{
    if (pointer == AllCategories) {
        return 0;
    } else {
        int index = 1;
        for (auto it = core()->categories().constBegin(); it != core()->categories().constEnd(); ++it, ++index) {
            if (*it == static_cast<const Category *>(pointer))
                return index;
        }
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
        res = c != AllCategories ? c->name() : QString("[%1]").arg(tr("All Items"));
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

    if (!c1 || c1 == AllCategories)
        return true;
    else if (!c2 || c2 == AllCategories)
        return false;
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
    return core()->itemTypes().count();
}

const void *BrickLink::ItemTypeModel::pointerAt(int index) const
{
    return *(core()->itemTypes().constBegin() + index);
}

int BrickLink::ItemTypeModel::pointerIndexOf(const void *pointer) const
{
    int index = 0;
    for (auto it = core()->itemTypes().constBegin(); it != core()->itemTypes().constEnd(); ++it, ++index) {
        if (*it == static_cast<const ItemType *>(pointer))
            return index;
    }
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
    return core()->items().count();
}

const void *BrickLink::ItemModel::pointerAt(int index) const
{
    return core()->items().at(index);
}

int BrickLink::ItemModel::pointerIndexOf(const void *pointer) const
{
    return core()->items().indexOf(static_cast<const Item *>(pointer));
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
    else if (role == Qt::DecorationRole) {
        if (index.column() == 0) {
            Picture *pic = core()->picture(i, i->defaultColor());

            if (pic && pic->valid())
                res = pic->image();
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

void BrickLink::ItemModel::setFilterText(const QString &str)
{
    if (str == m_text_filter)
        return;
    if (str.contains('?') || str.contains('*')) {
        m_text_filter_is_regexp = true;
        m_text_filter = QRegularExpression::wildcardToRegularExpression(str);
    } else {
        m_text_filter_is_regexp = false;
        m_text_filter = str;
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

namespace BrickLink {
// QRegularExpression is not thread-safe, so we need per-thread copies of the QRegularExpression object
static QThreadStorage<QRegularExpression *> regexpCache;
}

bool BrickLink::ItemModel::filterAccepts(const void *pointer) const
{
    const Item *item = static_cast<const Item *>(pointer);

    if (!item)
        return false;
    else if (m_itemtype_filter && item->itemType() != m_itemtype_filter)
        return false;
    else if (m_category_filter && (m_category_filter != BrickLink::CategoryModel::AllCategories) && !item->hasCategory(m_category_filter))
        return false;
    else if (m_inv_filter && !item->hasInventory())
        return false;
    else {
        if (!m_text_filter.isEmpty()) {
            if (!m_text_filter_is_regexp) {
                return item->id().startsWith(m_text_filter, Qt::CaseInsensitive)
                        || item->name().startsWith(m_text_filter, Qt::CaseInsensitive);
            } else {
                QRegularExpression *re = nullptr;
                if (!regexpCache.hasLocalData()) {
                    re = new QRegularExpression(QString(), QRegularExpression::CaseInsensitiveOption);
                    regexpCache.setLocalData(re);
                }
                re = regexpCache.localData();
                if (re->pattern() != m_text_filter)
                    re->setPattern(m_text_filter);
                return (re->match(item->id()).hasMatch() ||
                        re->match(item->name()).hasMatch());
            }
        }
    }
    return true;
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
        case 0: return tr("#");
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
    if (m_options & AlwaysShowSelection) {
        QStyleOptionViewItem myoption(option);
        myoption.state |= QStyle::State_Active;
        QStyledItemDelegate::paint(painter, myoption, index);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize BrickLink::ItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index) + QSize(0, 2);
}

bool BrickLink::ItemDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::ToolTip && index.isValid()) {
        ToolTip::inst()->show(index.data(BrickLink::ItemPointerRole).value<const BrickLink::Item *>(),
                              index.data(BrickLink::ColorPointerRole).value<const BrickLink::Color *>(),
                              event->globalPos(), view);
        return true;
    } else {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
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
    QString str = QLatin1String(R"(<div class="tooltip_picture"><table><tr><td>%2</td></tr><tr><td><b>%3</b></td></tr><tr><td>%1</td></tr></table></div>)");
    QString img_left = QLatin1String(R"(<img src="data:image/png;base64,%1" />)");
    QString note_left = QLatin1String("<i>") + BrickLink::ItemDelegate::tr("[Image is loading]") + QLatin1String("</i>");

    if (pic && (pic->updateStatus() == UpdateStatus::Updating)) {
        return str.arg(note_left, item->id(), item->name());
    } else {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        pic->image().save(&buffer, "PNG");

        return str.arg(img_left.arg(QString::fromLatin1(ba.toBase64())), item->id(), item->name());
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
                QRect desktop = QApplication::desktop()->screenGeometry(w);
                r.translate(r.right() > desktop.right() ? desktop.right() - r.right() : 0,
                            r.bottom() > desktop.bottom() ? desktop.bottom() - r.bottom() : 0);
                w->setGeometry(r);
                break;
            }
        }
    }
}
