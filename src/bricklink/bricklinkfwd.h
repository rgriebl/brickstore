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
#pragma once

#include <QPair>
#include <QHash>
#include <QVector>
#include <QMetaType>

QT_FORWARD_DECLARE_CLASS(QDataStream)

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
class InternalColorModel;
class CategoryModel;
class ItemTypeModel;
class ItemModel;
class AppearsInModel;
class InternalAppearsInModel;
class ItemDelegate;

typedef QVector<InvItem *>                    InvItemList;
typedef QPair<int, const Item *>              AppearsInItem;
typedef QVector<AppearsInItem>                AppearsInColor;
typedef QHash<const Color *, AppearsInColor>  AppearsIn;

QDataStream &operator << (QDataStream &ds, const Item *item);
QDataStream &operator >> (QDataStream &ds, Item *item);

QDataStream &operator << (QDataStream &ds, const ItemType *itt);
QDataStream &operator >> (QDataStream &ds, ItemType *itt);

QDataStream &operator << (QDataStream &ds, const Category *cat);
QDataStream &operator >> (QDataStream &ds, Category *cat);

QDataStream &operator << (QDataStream &ds, const Color *col);
QDataStream &operator >> (QDataStream &ds, Color *col);

enum class Time      { PastSix, Current, Count };
enum class Price     { Lowest, Average, WAverage, Highest, Count };
enum class Condition { New, Used, Count };
enum class SubCondition  { None, Complete, Incomplete, Sealed, Count };
enum class Stockroom { None, A, B, C };
enum class Status    { Include, Exclude, Extra, Unknown };

enum class UpdateStatus  { Ok, Updating, UpdateFailed };

enum class OrderType { Received, Placed, Any };

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
    URL_StoreItemDetail
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

Q_DECLARE_METATYPE(BrickLink::Time)
Q_DECLARE_METATYPE(BrickLink::Price)
