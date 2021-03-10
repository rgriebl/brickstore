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
class PartColorCode;

class Picture;
class PriceGuide;
class Order;
class Cart;
class Incomplete;
class Core;
class TextImport;

class ColorModel;
class InternalColorModel;
class CategoryModel;
class ItemTypeModel;
class ItemModel;
class AppearsInModel;
class InternalAppearsInModel;
class ItemDelegate;

typedef QPair<int, const Item *>              AppearsInItem;
typedef QVector<AppearsInItem>                AppearsInColor;
typedef QHash<const Color *, AppearsInColor>  AppearsIn;

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
    URL_StoreItemDetail,
    URL_StoreItemSearch,
    URL_OrderDetails,
    URL_ShoppingCart,
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
Q_DECLARE_METATYPE(BrickLink::Condition)
Q_DECLARE_METATYPE(BrickLink::SubCondition)
Q_DECLARE_METATYPE(BrickLink::Stockroom)
Q_DECLARE_METATYPE(BrickLink::Status)
Q_DECLARE_METATYPE(BrickLink::OrderType)
Q_DECLARE_METATYPE(BrickLink::OrderStatus)
