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
#pragma once

#include <QMetaType>

namespace BrickLink {

Q_NAMESPACE

enum class Time          { PastSix, Current, Count };
enum class Price         { Lowest, Average, WAverage, Highest, Count };
enum class Condition     { New, Used, Count };
enum class SubCondition  { None, Complete, Incomplete, Sealed, Count };
enum class Stockroom     { None, A, B, C, Count };
enum class Status        { Include, Exclude, Extra, Count };
enum class UpdateStatus  { Ok, Loading, Updating, UpdateFailed };

enum class OrderType     { Received, Placed, Any };
enum class OrderStatus   { Unknown, Pending, Updated, Processing, Ready, Paid, Packed, Shipped,
                           Received, Completed, OCR, NPB, NPX, NRS, NSS, Cancelled, Count };

enum class Url {
    InventoryRequest,
    WantedListUpload,
    InventoryUpload,
    InventoryUpdate,
    CatalogInfo,
    PriceGuideInfo,
    ColorChangeLog,
    ItemChangeLog,
    LotsForSale,
    AppearsInSets,
    StoreItemDetail,
    StoreItemSearch,
    OrderDetails,
    ShoppingCart,
    WantedList,
};

Q_ENUM_NS(Time)
Q_ENUM_NS(Price)
Q_ENUM_NS(Condition)
Q_ENUM_NS(SubCondition)
Q_ENUM_NS(Stockroom)
Q_ENUM_NS(Status)
Q_ENUM_NS(UpdateStatus)
Q_ENUM_NS(OrderType)
Q_ENUM_NS(OrderStatus)
Q_ENUM_NS(Url)

enum ModelRoles {
    RoleBase = 0x05c136c8,  // printf "0x%08x\n" $(($RANDOM*$RANDOM))

    ColorPointerRole,
    CategoryPointerRole,
    ItemTypePointerRole,
    ItemPointerRole,
    IdRole,
    NameRole,
    ColorNameRole,
    QuantityRole,

    RoleMax
};

class Color;
class Category;
class ItemType;
class Item;
class PartColorCode;

class Picture;
class PriceGuide;
class Order;
class Orders;
class Cart;
class Carts;
class Incomplete;
class Core;
class Database;
class TextImport;
class Store;
class WantedList;
class WantedLists;
class Lot;
class ItemChangeLogEntry;
class ColorChangeLogEntry;

class ColorModel;
class InternalColorModel;
class CategoryModel;
class ItemTypeModel;
class ItemModel;
class InventoryModel;
class InternalInventoryModel;
class ItemDelegate;

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Time)
Q_DECLARE_METATYPE(BrickLink::Price)
Q_DECLARE_METATYPE(BrickLink::Condition)
Q_DECLARE_METATYPE(BrickLink::SubCondition)
Q_DECLARE_METATYPE(BrickLink::Stockroom)
Q_DECLARE_METATYPE(BrickLink::Status)
Q_DECLARE_METATYPE(BrickLink::OrderType)
Q_DECLARE_METATYPE(BrickLink::OrderStatus)
Q_DECLARE_METATYPE(BrickLink::Url)
