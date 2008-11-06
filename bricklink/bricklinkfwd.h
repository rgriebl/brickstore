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
#ifndef __BRICKLINK_FWD_H__
#define __BRICKLINK_FWD_H__

#include <QPair>
#include <QHash>
#include <QVector>
#include <QList>

class QDataStream;

namespace BrickLink {
class Color;
class Category;
class ItemType;
class Item;

class Picture;
class PriceGuide;
class InvItem;
class Order;
class Core;
class TextImport;
class InvItemMimeData;

class ColorModel;
class ColorProxyModel;
class CategoryModel;
class CategoryProxyModel;
class ItemTypeModel;
class ItemTypeProxyModel;
class ItemModel;
class ItemProxyModel;
class AppearsInModel;
class AppearsInProxyModel;

typedef QList<InvItem *>                      InvItemList;
typedef QPair<int, const Item *>              AppearsInItem;
typedef QVector<AppearsInItem>                AppearsInColor;
typedef QHash<const Color *, AppearsInColor>  AppearsIn;

QDataStream &operator << (QDataStream &ds, const InvItem &ii);
QDataStream &operator >> (QDataStream &ds, InvItem &ii);

QDataStream &operator << (QDataStream &ds, const Item *item);
QDataStream &operator >> (QDataStream &ds, Item *item);

QDataStream &operator << (QDataStream &ds, const ItemType *itt);
QDataStream &operator >> (QDataStream &ds, ItemType *itt);

QDataStream &operator << (QDataStream &ds, const Category *cat);
QDataStream &operator >> (QDataStream &ds, Category *cat);

QDataStream &operator << (QDataStream &ds, const Color *col);
QDataStream &operator >> (QDataStream &ds, Color *col);

enum Time      { AllTime, PastSix, Current, TimeCount };
enum Price     { Lowest, Average, WAverage, Highest, PriceCount };
enum Condition { New, Used, ConditionCount };
enum SubCondition { None, Complete, Incomplete, MISB, SubConditionCount };

enum Status    { Include, Exclude, Extra, Unknown };

enum UpdateStatus { Ok, Updating, UpdateFailed };

enum OrderType { Received, Placed, Any };

enum UrlList {
    URL_InventoryRequest,
    URL_WantedListUpload,
    URL_InventoryUpload,
    URL_InventoryUpdate,
    URL_CatalogInfo,
    URL_PriceGuideInfo,
    URL_ColorChangeLog,
    URL_ItemChangeLog,
    URL_LotsForSale,
    URL_AppearsInSets,
    URL_PeeronInfo,
    URL_StoreItemDetail
};

enum ItemListXMLHint {
    XMLHint_MassUpload,
    XMLHint_MassUpdate,
    XMLHint_Inventory,
    XMLHint_Order,
    XMLHint_WantedList,
    XMLHint_BrikTrak,
    XMLHint_BrickStore
};

enum ModelRoles {
    RoleBase = 0x05c136c8,  // printf "0x%08x\n" $(($RANDOM*$RANDOM))

    ColorPointerRole,
    CategoryPointerRole,
    ItemTypePointerRole,
    ItemPointerRole,
    AppearsInItemPointerRole,

    RoleMax
};

} // namespace BrickLink

#endif

