/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <QApplication>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QThreadStorage>
#include <QHelpEvent>
#include <QToolTip>
#include <QLabel>
#include <QAbstractItemView>

#include "qtemporaryresource.h"

#include "bricklink.h"

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
    : StaticPointerModel(parent), m_itemtype_filter(0)
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
    for (QMap<int, const Color *>::const_iterator it = core()->colors().constBegin(); it != core()->colors().constEnd(); ++it, ++index) {
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
        if (const QPixmap *pix = core()->colorImage(c, fm.height(), fm.height())) {
            QIcon ico;
            ico.addPixmap(*pix, QIcon::Normal, QIcon::Off);
            ico.addPixmap(*pix, QIcon::Selected, QIcon::Off);
            res.setValue(ico);
        }
    }
    else if (role == Qt::ToolTipRole) {
        res = QString("<table width=\"100%\" border=\"0\" bgcolor=\"%3\"><tr><td><br><br></td></tr></table><br />%1: %2").arg(tr("RGB"), c->color().name(), c->color().name()); //%
    }
    else if (role == ColorPointerRole) {
        res.setValue(c);
    }
    return res;
}

QVariant BrickLink::ColorModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Color");
    return QVariant();
}

bool BrickLink::ColorModel::isFiltered() const
{
    return m_itemtype_filter;
}

void BrickLink::ColorModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

bool BrickLink::ColorModel::lessThan(const void *p1, const void *p2, int /*column*/) const
{
    const Color *c1 = static_cast<const Color *>(p1);
    const Color *c2 = static_cast<const Color *>(p2);

    if (!c1)
        return true;
    else if (!c2)
        return false;
    else {
        if (sortOrder() == Qt::AscendingOrder) {
            return (strcmp(c1->name(), c2->name()) < 0);
        }
        else {
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
    const Color *color = static_cast<const Color *>(pointer);

    return !m_itemtype_filter || m_itemtype_filter->hasColors() || (color && color->id() == 0);
}



/////////////////////////////////////////////////////////////
// CATEGORYMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const BrickLink::Category *BrickLink::CategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);

BrickLink::CategoryModel::CategoryModel(QObject *parent)
    : StaticPointerModel(parent), m_itemtype_filter(0), m_all_filter(false)
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
        for (QMap<int, const Category *>::const_iterator it = core()->categories().constBegin(); it != core()->categories().constEnd(); ++it, ++index) {
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

    if (role == Qt::DisplayRole) {
        res = c != AllCategories ? c->name() : QString("[%1]").arg(tr("All Items"));
    }
    else if (role == CategoryPointerRole) {
        res.setValue(c);
    }
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
    const Category *c1 = static_cast<const Category *>(p1);
    const Category *c2 = static_cast<const Category *>(p2);

    if (!c1 || c1 == AllCategories)
        return true;
    else if (!c2 || c2 == AllCategories)
        return false;
    else
        return strcmp(c1->name(), c2->name()) < 0;
}

bool BrickLink::CategoryModel::filterAccepts(const void *pointer) const
{
    const Category *c = static_cast<const Category *>(pointer);

    if (m_itemtype_filter || m_all_filter) {
        if (!c)
            return false;
        else if (c == AllCategories)
            return !m_all_filter;
        else if (m_itemtype_filter) {
            for (const Category **cp = m_itemtype_filter->categories(); *cp; cp++) {
                if (c == *cp)
                    return true;
            }
            return false;
        }
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
    for (QMap<int, const ItemType *>::const_iterator it = core()->itemTypes().constBegin(); it != core()->itemTypes().constEnd(); ++it, ++index) {
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
    const ItemType *i1 = static_cast<const ItemType *>(p1);
    const ItemType *i2 = static_cast<const ItemType *>(p2);

    return !i1 ? true : (!i2 ? false : strcmp(i1->name(), i2->name()) < 0);
}

bool BrickLink::ItemTypeModel::filterAccepts(const void *pointer) const
{
    const ItemType *itemtype = static_cast<const ItemType *>(pointer);

    return !m_inv_filter || (itemtype && itemtype->hasInventories());
}



/////////////////////////////////////////////////////////////
// ITEMMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemModel::ItemModel(QObject *parent)
    : StaticPointerModel(parent), m_itemtype_filter(0), m_category_filter(0), m_inv_filter(false)
{
    MODELTEST_ATTACH(this)
    connect(core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(pictureUpdated(BrickLink::Picture *)));
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
        case 1: res = QString::fromLatin1(i->id()); break;
        case 2: res = QString::fromLatin1(i->name()); break;
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
            res = QString::fromLatin1(i->name());
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
    m_text_filter = str;
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

    return !i1 ? true : (!i2 ? false : qstricmp((column == 2) ? i1->name() : i1->id(),
                                                (column == 2) ? i2->name() : i2->id()) < 0);
}

namespace BrickLink {
// QRegExp is not thread-safe, so we need per-thread copies of the QRegExp object
QThreadStorage<QRegExp *> regexpCache;
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
            QRegExp *re = 0;
            if (!regexpCache.hasLocalData()) {
                re = new QRegExp(m_text_filter, Qt::CaseInsensitive, QRegExp::Wildcard);
                regexpCache.setLocalData(re);
            } else {
                re = regexpCache.localData();
                if (re->pattern() != m_text_filter) {
                    re->setPattern(m_text_filter);
                }
            }
            return ((re->indexIn(QLatin1String(item->id())) >= 0) ||
                    (re->indexIn(QLatin1String(item->name())) >= 0));

        }
    }
    return true;
}


/////////////////////////////////////////////////////////////
// APPEARSINMODEL
/////////////////////////////////////////////////////////////


BrickLink::InternalAppearsInModel::InternalAppearsInModel(const Item *item, const Color *color, QObject *parent)
    : QAbstractTableModel(parent), m_item(item), m_color(color)
{
    InvItemList list;
    InvItem invitem(color, item);
    list.append(&invitem);
    init(list);
}

BrickLink::InternalAppearsInModel::InternalAppearsInModel(const InvItemList &list, QObject *parent)
    : QAbstractTableModel(parent), m_item(0), m_color(0)
{
    init(list);
}

void BrickLink::InternalAppearsInModel::init(const InvItemList &list)
{
    MODELTEST_ATTACH(this)

    if (!list.isEmpty()) {
        QMap<const Item *, int> unique;
        bool first_item = true;

        foreach (InvItem *invitem, list) {
            if (!invitem->item())
                continue;

            foreach (const AppearsInColor &vec, invitem->item()->appearsIn(invitem->color())) {
                foreach (const AppearsInItem &item, vec) {
                    QMap<const Item *, int>::iterator it = unique.find(item.second);
                    if (it != unique.end())
                        ++it.value();
                    else if (first_item)
                        unique.insert(item.second, 1);
                }
            }
            first_item = false;
        }
        for (QMap<const Item *, int>::iterator it = unique.begin(); it != unique.end(); ++it) {
            if (it.value() >= list.count())
                m_items.append(new AppearsInItem(-1, it.key()));
        }
    }
}

BrickLink::InternalAppearsInModel::~InternalAppearsInModel()
{
    if (!m_item && !m_color && !m_items.isEmpty())
        qDeleteAll(m_items);
}

QModelIndex BrickLink::InternalAppearsInModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent) && !parent.isValid())
        return createIndex(row, column, m_items.at(row));
    return QModelIndex();
}

const BrickLink::AppearsInItem *BrickLink::InternalAppearsInModel::appearsIn(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<const AppearsInItem *>(idx.internalPointer()) : 0;
}

QModelIndex BrickLink::InternalAppearsInModel::index(const AppearsInItem *const_ai) const
{
    AppearsInItem *ai = const_cast<AppearsInItem *>(const_ai);

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
    InternalAppearsInModel *aim = static_cast<InternalAppearsInModel *>(sourceModel());

    if (aim && index.isValid())
        return aim->appearsIn(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::AppearsInModel::index(const AppearsInItem *item) const
{
    InternalAppearsInModel *aim = static_cast<InternalAppearsInModel *>(sourceModel());

    if (aim && item)
        return mapFromSource(aim->index(item));
    return QModelIndex();
}

bool BrickLink::AppearsInModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const InternalAppearsInModel *aim = static_cast<const InternalAppearsInModel *>(sourceModel());
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
        case  1: return (BrickLink::Item::compareId(ai1->second->id(), ai2->second->id()) < 0);
        case  2: return (qstrcmp(ai1->second->name(), ai2->second->name()) < 0 );
        }
    }
}


/////////////////////////////////////////////////////////////
// ITEMDELEGATE
/////////////////////////////////////////////////////////////



BrickLink::ItemDelegate::ItemDelegate(QObject *parent, Options options)
    : QStyledItemDelegate(parent), m_options(options), m_tooltip_pic(0)
{
    connect(BrickLink::core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(pictureUpdated(BrickLink::Picture *)));
}

void BrickLink::ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (m_options & AlwaysShowSelection) {
        QStyleOptionViewItemV4 myoption(option);
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
        if (BrickLink::Picture *pic = BrickLink::core()->picture(index.data(BrickLink::ItemPointerRole).value<const BrickLink::Item *>(),
                                                                 0 /*index.data(BrickLink::ColorPointerRole).value<const BrickLink::Color *>()*/, true)) {
            QTemporaryResource::registerResource("#/tooltip_picture.png", pic->valid() ? pic->image() : QImage());
            m_tooltip_pic = (pic->updateStatus() == BrickLink::Updating) ? pic : 0;

            // need to 'clear' to reset the image cache of the QTextDocument
            foreach (QWidget *w, QApplication::topLevelWidgets()) {
                if (w->inherits("QTipLabel")) {
                    qobject_cast<QLabel *>(w)->clear();
                    break;
                }
            }
            QString tt = createToolTip(pic->item(), pic);
            if (!tt.isEmpty()) {
                QToolTip::showText(event->globalPos(), tt, view);
                return true;
            }
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

QString BrickLink::ItemDelegate::createToolTip(const BrickLink::Item *item, BrickLink::Picture *pic) const
{
    QString str = QLatin1String("<div class=\"tooltip_picture\"><table><tr><td rowspan=\"2\">%1</td><td><b>%2</b></td></tr><tr><td>%3</td></tr></table></div>");
    QString img_left = QLatin1String("<img src=\"#/tooltip_picture.png\" />");
    QString note_left = QLatin1String("<i>") + BrickLink::ItemDelegate::tr("[Image is loading]") + QLatin1String("</i>");

    if (pic && (pic->updateStatus() == BrickLink::Updating))
        return str.arg(note_left).arg(item->id()).arg(item->name());
    else
        return str.arg(img_left).arg(item->id()).arg(item->name());
}

void BrickLink::ItemDelegate::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || pic != m_tooltip_pic)
        return;

    m_tooltip_pic = 0;

    if (QToolTip::isVisible() && QToolTip::text().startsWith("<div class=\"tooltip_picture\">")) {
        QTemporaryResource::registerResource("#/tooltip_picture.png", pic->image());

        foreach (QWidget *w, QApplication::topLevelWidgets()) {
            if (w->inherits("QTipLabel")) {
                QSize extra = w->size() - w->sizeHint();
                qobject_cast<QLabel *>(w)->clear();
                qobject_cast<QLabel *>(w)->setText(createToolTip(pic->item(), pic));
                w->resize(w->sizeHint() + extra);
                break;
            }
        }
    }
}