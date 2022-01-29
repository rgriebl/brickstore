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

#include <QPainter>
#include <QQmlEngine>
#include <QQmlContext>
#include "qqmlinfo.h"

#include "utility/utility.h"
#include "utility/currency.h"
#include "bricklink/cart.h"
#include "bricklink/picture.h"
#include "bricklink/priceguide.h"
#include "bricklink/order.h"
#include "bricklink/store.h"
#include "bricklink_wrapper.h"
#include "common/document.h"


QmlImageItem::QmlImageItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{ }

QImage QmlImageItem::image() const
{
   return m_image;
}

void QmlImageItem::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void QmlImageItem::paint(QPainter *painter)
{
    if (m_image.isNull())
        return;

    QRect br = boundingRect().toRect();
    QSize itemSize = br.size();
    QSize imageSize = m_image.size();

    imageSize.scale(itemSize, Qt::KeepAspectRatio);

    br.setSize(imageSize);
    br.translate((itemSize.width() - imageSize.width()) / 2,
                 (itemSize.height() - imageSize.height()) / 2);

    painter->drawImage(br, m_image);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype BrickLink
    \inherits QtObject
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief The core singleton managing the data transfer from and to BrickLink.

    This singleton is responsible for handling all communications with the BrickLink servers, as
    well as giving access to the BrickLink item catalog, which is available through these types:
    \list
    \li ItemType
    \li \l Category
    \li \l Color
    \li \l Item
    \li \l Picture
    \li PriceGuide
    \endlist
*/

QmlBrickLink *QmlBrickLink::s_inst = nullptr;

void QmlBrickLink::registerTypes()
{
    qRegisterMetaType<QmlColor>("Color");
    qRegisterMetaType<QmlItemType>("ItemType");
    qRegisterMetaType<QmlCategory>("Category");
    qRegisterMetaType<QmlItem>("Item");
    qRegisterMetaType<QmlLot>("Lot");
    qRegisterMetaType<QmlPriceGuide>("PriceGuide");
    qRegisterMetaType<QmlPicture>("Picture");

    s_inst = new QmlBrickLink(BrickLink::core());

    QQmlEngine::setObjectOwnership(s_inst, QQmlEngine::CppOwnership);
    qmlRegisterSingletonType<QmlBrickLink>("BrickStore", 1, 0, "BrickLink",
                                           [](QQmlEngine *, QJSEngine *) -> QObject * {
        return s_inst;
    });

    QString cannotCreate = tr("Cannot create objects of type %1");
    qmlRegisterUncreatableType<BrickLink::Order>("BrickStore", 1, 0, "Order",
                                                 cannotCreate.arg("Order"_l1));
    qmlRegisterUncreatableType<BrickLink::Cart>("BrickStore", 1, 0, "Cart",
                                                cannotCreate.arg("Cart"_l1));
    qmlRegisterUncreatableType<BrickLink::Store>("BrickStore", 1, 0, "Store",
                                                 cannotCreate.arg("Store"_l1));
    qmlRegisterUncreatableType<QmlLots>("BrickStore", 1, 0, "Lots",
                                        cannotCreate.arg("Lots"_l1));

    qmlRegisterType<QmlImageItem>("BrickStore", 1, 0, "QImageItem");

    Document::setQmlLotsFactory([](Document *doc) -> QObject * {
        return new QmlLots(doc->model());
    });
}

QmlBrickLink::QmlBrickLink(BrickLink::Core *core)
    : d(core)
{
    setObjectName("BrickLink"_l1);

    connect(core, &BrickLink::Core::priceGuideUpdated,
            this, [this](::BrickLink::PriceGuide *pg) {
        emit priceGuideUpdated(QmlPriceGuide(pg));
    });
    connect(core, &BrickLink::Core::pictureUpdated,
            this, [this](::BrickLink::Picture *pic) {
        emit pictureUpdated(QmlPicture(pic));
    });
}

/*! \qmlproperty Item BrickLink::noItem
    \readonly
    A special Item object denoting an invalid item. The object's Item::isNull returns \c true.
    Used as a return value for functions that can fail.
*/
QmlItem QmlBrickLink::noItem() const
{
    return QmlItem { };
}

/*! \qmlproperty Color BrickLink::noColor
    \readonly
    A special \l Color object denoting an invalid color. The object's Color::isNull returns \c true.
    Used as a return value for functions that can fail.
*/
QmlColor QmlBrickLink::noColor() const
{
    return QmlColor { };
}


/*! \qmlmethod Color BrickLink::color(var color)
    Create a JavaScript Color wrapper for a C++ \c{BrickLink::Color *} \a color obtained from a
    data model.
*/
/*! \qmlmethod Color BrickLink::color(string colorName)
    Returns a Color object corresponding to the given BrickLink \a colorName. If there is no match,
    the returned object is noColor.
*/
/*! \qmlmethod Color BrickLink::color(uint colorId)
    Returns a Color object corresponding to the given BrickLink \a colorId. If there is no match,
    the returned object is noColor.
*/
QmlColor QmlBrickLink::color(const QVariant &v) const
{
    if (v.userType() == qMetaTypeId<const BrickLink::Color *>())
        return v.value<const BrickLink::Color *>();
    else if (v.userType() == QMetaType::QString)
        return d->colorFromName(v.toString());
    else
        return d->color(v.value<uint>());
}

/*! \qmlmethod Color BrickLink::colorFromLDrawId(int colorId)
    Returns a Color object corresponding to the given LDraw \a colorId. If there is no match (or
    if a LDraw installation isn't available), the returned object is noColor.
*/
QmlColor QmlBrickLink::colorFromLDrawId(int ldrawId) const
{
    return d->colorFromLDrawId(ldrawId);
}

/*! \qmlmethod Category BrickLink::category(int categoryId)
    Returns a Category object corresponding to the given BrickLink \a categoryId. If there is no
    match, the returned object is noCategory.
*/
QmlCategory QmlBrickLink::category(int id) const
{
    return d->category(uint(id));
}

/*! \qmlmethod ItemType BrickLink::itemType(string itemTypeId)
    Returns an ItemType object corresponding to the given BrickLink \a itemTypeId. If there is no
    match, the returned object is noItemType.
    \note The id is a single letter string and has to be one of \c{PCGIMOPS}.
*/
QmlItemType QmlBrickLink::itemType(const QString &itemTypeId) const
{
    return d->itemType(firstCharInString(itemTypeId));
}

/*! \qmlmethod Item BrickLink::item(string itemTypeId, string itemId)
    Returns an \l Item object corresponding to the given BrickLink \a itemTypeId and \a itemId. If
    there is no match, the returned object is noItem.
*/
QmlItem QmlBrickLink::item(const QString &itemTypeId, const QString &itemId) const
{
    return d->item(firstCharInString(itemTypeId), itemId.toLatin1());
}

/*! \qmlsignal BrickLink::priceGuideUpdated(PriceGuide priceGuide)
    This signal is emitted everytime the state of the \a priceGuide object changes. Receiving this
    signal doesn't mean the preice guide data is available: you have to check the object's
    properties to see what has changed.
*/
/*! \qmlmethod PriceGuide BrickLink::priceGuide(Item item, Color color, bool highPriority = false)
    Creates a PriceGuide object that asynchronously loads (or downloads) the price guide data for
    the given \a item and \a color combination. If you set \a highPriority to \c true the
    load/download request will be pre-prended to the work queue instead of appended.
    You need to connect to the signal BrickLink::priceGuideUpdated() to know when the data has
    been loaded.
    \sa PriceGuide
*/
QmlPriceGuide QmlBrickLink::priceGuide(QmlItem item, QmlColor color, bool highPriority)
{
    return QmlPriceGuide { d->priceGuide(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

/*! \qmlsignal BrickLink::pictureUpdated(Picture picture)
    This signal is emitted everytime the state of the \a picture object changes. Receiving this
    signal doesn't mean the picture is available: you have to check the object's properties
    to see what has changed.
*/
/*! \qmlmethod Picture BrickLink::picture(Item item, Color color, bool highPriority = false)
    Creates a \l Picture object that asynchronously loads (or downloads) the picture for the given
    \a item and \a color combination. If you set \a highPriority to \c true the load/download
    request will be pre-prended to the work queue instead of appended.
    You need to connect to the signal BrickLink::pictureUpdated() to know when the data has
    been loaded.
    \sa Picture
*/
QmlPicture QmlBrickLink::picture(QmlItem item, QmlColor color, bool highPriority)
{
    return QmlPicture { d->picture(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

/*! \qmlmethod Picture BrickLink::largePicture(Item item, bool highPriority = false)
    Creates a Picture object that asynchronously loads (or downloads) the large picture for the
    given \a item. If you set \a highPriority to \c true the load/download request will be
    pre-prended to the work queue instead of appended.
    You need to connect to the signal BrickLink::pictureUpdated() to know when the data has
    been loaded.
    \sa Picture
*/
QmlPicture QmlBrickLink::largePicture(QmlItem item, bool highPriority)
{
    return QmlPicture { d->largePicture(item.wrappedObject(), highPriority) };
}

/*! \qmlmethod Lot BrickLink::lot(var lot)
    Create a JavaScript Lot wrapper for a C++ \c{BrickLink::Lot *} \a lot obtained from a data model.
*/
QmlLot QmlBrickLink::lot(const QVariant &v) const
{
    if (v.userType() == qMetaTypeId<const BrickLink::Lot *>())
        return QmlLot { const_cast<BrickLink::Lot *>(v.value<const BrickLink::Lot *>()), nullptr };
    else
        return v.value<BrickLink::Lot *>();
}

void QmlBrickLink::cacheStat() const
{
    auto pic = BrickLink::core()->pictureCacheStats();
    auto pg = BrickLink::core()->priceGuideCacheStats();

    QByteArray picBar(int(double(pic.first) / pic.second * 16), '=');
    picBar.append(16 - picBar.length(), ' ');
    QByteArray pgBar(int(double(pg.first) / pg.second * 16), '=');
    pgBar.append(16 - pgBar.length(), ' ');

    qmlDebug(this) << "Cache stats:\n"
                   << "Pictures    : [" << picBar.constData() << "] " << (pic.first / 1000)
                   << " / " << (pic.second / 1000) << " MB\n"
                   << "Price guides: [" << pgBar.constData() << "] " << (pg.first)
                   << " / " << pg.second << " entries";
}

BrickLink::Store *QmlBrickLink::store() const
{
    return BrickLink::core()->store();
}

BrickLink::Orders *QmlBrickLink::orders() const
{
    return BrickLink::core()->orders();
}

BrickLink::Carts *QmlBrickLink::carts() const
{
    return BrickLink::core()->carts();
}


char QmlBrickLink::firstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Category
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink item category.

    Each category in the BrickLink catalog is available as a Category object.

    You cannot create Category objects yourself, but you can retrieve a Category object given the
    id via BrickLink::category().
    Each Item also has a read-only property Item::category.

    See \l https://www.bricklink.com/catalogCategory.asp
*/
/*! \qmlproperty bool Category::isNull
    \readonly
    Returns whether this Category is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int Category::id
    \readonly
    The BrickLink id of this category.
*/
/*! \qmlproperty string Category::name
    \readonly
    The BrickLink name of this category.
*/

QmlCategory::QmlCategory(const BrickLink::Category *cat)
    : QmlWrapperBase(cat)
{ }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype ItemType
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink item type.

    Each item type in the BrickLink catalog is available as an ItemType object.

    You cannot create ItemType objects yourself, but you can retrieve an ItemType object given the
    id via BrickLink::itemType().
    Each Item also has a read-only property Item::itemType.

    The currently available item types are
    \table
    \header
      \li Id
      \li Name
    \row
      \li \c B
      \li Book
    \row
      \li \c C
      \li Catalog
    \row
      \li \c G
      \li Gear
    \row
      \li \c I
      \li Instruction
    \row
      \li \c M
      \li Minifigure
    \row
      \li \c O
      \li Original Box
    \row
      \li \c P
      \li Part
    \row
      \li \c S
      \li Set
    \endtable
*/
/*! \qmlproperty bool ItemType::isNull
    \readonly
    Returns whether this ItemType is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int ItemType::id
    \readonly
    The BrickLink id of this item type.
*/
/*! \qmlproperty string ItemType::name
    \readonly
    The BrickLink name of this item type.
*/
/*! \qmlproperty list<Category> ItemType::categories
    \readonly
    A list of \l Category objects describing all the categories that are referencing at least one
    item of the given item type.
*/
/*! \qmlproperty bool ItemType::hasInventories
    \readonly
    Returns \c true if items under this type can have inventories, or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasColors
    \readonly
    Returns \c true if items under this type can have colors, or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasYearReleased
    \readonly
    Returns \c true if items under this type have data about the year the item was release,
    or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasWeight
    \readonly
    Returns \c true if items under this type can have weights, or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasSubConditions
    \readonly
    Returns \c true if items under this type can have sub-conditions, or \c false otherwise.
*/
/*! \qmlproperty int ItemType::pictureId
    \readonly
    Item types may not have their own pictures, but use the pictures of another item type: in this
    case you can get the id of the item type providing the pictures.
*/
/*! \qmlproperty size ItemType::pictureSize
    \readonly
    The default size and aspect ratio for item pictures of this type.
*/

QmlItemType::QmlItemType(const BrickLink::ItemType *itt)
    : QmlWrapperBase(itt)
{ }

QVariantList QmlItemType::categories() const
{
    auto cats = wrapped->categories();
    QVariantList result;
    for (auto cat : cats)
        result.append(QVariant::fromValue(QmlCategory { cat }));
    return result;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype Color
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink color.

    Each color in the BrickLink catalog is available as a Color object.

    You cannot create Color objects yourself, but you can retrieve a Color object given the
    id via the various BrickLink::color() overloads and BrickLink::colorFromLDrawId().

    See \l https://www.bricklink.com/catalogColors.asp
*/
/*! \qmlproperty bool Color::isNull
    \readonly
    Returns whether this Color is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int Color::id
    \readonly
    The BrickLink id of this color.
*/
/*! \qmlproperty string Color::name
    \readonly
    The BrickLink name of this color.
*/
/*! \qmlproperty color Color::color
    \readonly
    Returns the RGB value of this BrickLink color as a basic QML color type.
*/
/*! \qmlproperty int Color::ldrawId
    \readonly
    The LDraw id of this color, or \c -1 if there is no match or if a LDraw installation isn't
    available.
*/
/*! \qmlproperty bool Color::solid
    \readonly
    Returns \c true if this color is a solid color, or \c false otherwise.
*/
/*! \qmlproperty bool Color::transparent
    \readonly
    Returns \c true if this color is transparent, or \c false otherwise.
*/
/*! \qmlproperty bool Color::glitter
    \readonly
    Returns \c true if this color is glittery, or \c false otherwise.
*/
/*! \qmlproperty bool Color::speckle
    \readonly
    Returns \c true if this color is speckled, or \c false otherwise.
*/
/*! \qmlproperty bool Color::metallic
    \readonly
    Returns \c true if this color is metallic, or \c false otherwise.
*/
/*! \qmlproperty bool Color::chrome
    \readonly
    Returns \c true if this color is chrome-like, or \c false otherwise.
*/
/*! \qmlproperty bool Color::milky
    \readonly
    Returns \c true if this color is milky, or \c false otherwise.
*/
/*! \qmlproperty bool Color::modulex
    \readonly
    Returns \c true if this color is a Modulex color, or \c false otherwise.
*/
/*! \qmlproperty real Color::popularity
    \readonly
    Returns the popularity of this color, normalized to the range \c{[0 .. 1]}.
    The raw popularity value is derived from summing up the counts of the \e Parts, \e{In Sets},
    \e Wanted and \e{For Sale} columns in the \l{https://www.bricklink.com/catalogColors.asp}
    {BrickLink Color Guide table}.
*/
/*! \qmlproperty image Color::image
    \readonly
    Returns a 20x20 pixel image of this color.
*/

QmlColor::QmlColor(const BrickLink::Color *col)
    : QmlWrapperBase(col)
{ }

QImage QmlColor::image() const
{
    return ::BrickLink::core()->colorImage(wrapped, 20, 20);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Item
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink item.

    Each item in the BrickLink catalog is available as an Item object.

    You cannot create Item objects yourself, but you can retrieve an Item object given the
    id via BrickLink::item().

    See \l https://www.bricklink.com/catalog.asp
*/
/*! \qmlproperty bool Item::isNull
    \readonly
    Returns whether this Item is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int Item::id
    \readonly
    The BrickLink id of this item.
*/
/*! \qmlproperty string Item::name
    \readonly
    The BrickLink name of this item.
*/
/*! \qmlproperty ItemType Item::itemType
    \readonly
    The BrickLink item type of this item.
*/
/*! \qmlproperty Category Item::category
    \readonly
    The BrickLink category of this item.
*/
/*! \qmlproperty bool Item::hasInventory
    \readonly
    Returns \c true if a valid inventory exists for this item, or \c false otherwise.
*/
/*! \qmlproperty date Item::inventoryUpdated
    \readonly
    Holds the time stamp of the last successful update of the item's inventory.
*/
/*! \qmlproperty Color Item::defaultColor
    \readonly
    Returns the default color used by BrickLink to display a large picture for this item.
*/
/*! \qmlproperty real Item::weight
    \readonly
    Returns the weight of this item in gram.
*/
/*! \qmlproperty date Item::yearReleased
    \readonly
    Returns the year this item was first released.
*/
/*! \qmlproperty list<Color> Item::knownColors
    \readonly
    Returns a list of \l Color objects, containing all the colors the item is known to exist in.
    \note An item might still exist in more colors than returned here: BrickStore is deriving this
          data by looking at all the known inventories and PCCs (part-color-codes).
*/
/*! \qmlmethod bool Item::hasKnownColor(Color color)
    Returns \c true if this item is known to exist in the given \a color, or \c false otherwise.
    \sa knownColors
*/

QmlItem::QmlItem(const BrickLink::Item *item)
    : QmlWrapperBase(item)
{ }

QString QmlItem::id() const
{
    return QString::fromLatin1(wrapped->id());
}

bool QmlItem::hasKnownColor(QmlColor color) const
{
    return wrapped->hasKnownColor(color.wrappedObject());
}

QVariantList QmlItem::knownColors() const
{
    auto known = wrapped->knownColors();
    QVariantList result;
    for (auto c : known)
        result.append(QVariant::fromValue(QmlColor { c }));
    return result;
}

QVariantList QmlItem::consistsOf() const
{
//    const auto consists = wrapped->consistsOf();
//    QVariantList result;
//    for (auto auto &co : consists)
//        result << QVariant::fromValue(Lot { nullptr, nullptr });
//    return result;
    return { };
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Picture
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a picture of a BrickLink item.

    Each picture of an item in the BrickLink catalog is available as a Picture object.

    You cannot create Picture objects yourself, but you can retrieve a Picture object given the
    item and color id via BrickLink::picture() or BrickLink::largePicture().

    \note Pictures aren't readily available, but need to be asynchronously loaded (or even
          downloaded) at runtime. You need to connect to the signal BrickLink::pictureUpdated()
          to know when the data has been loaded.
*/
/*! \qmlproperty bool Picture::isNull
    \readonly
    Returns whether this Picture is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item Picture::item
    \readonly
    The BrickLink item reference this picture is requested for.
*/
/*! \qmlproperty Color Picture::color
    \readonly
    The BrickLink color reference this picture is requested for.
*/
/*! \qmlproperty date Picture::lastUpdated
    \readonly
    Holds the time stamp of the last successful update of this picture.
*/
/*! \qmlproperty UpdateStatus Picture::updateStatus
    \readonly
    Returns the current update status. The available values are:
    \value BrickLink::Ok            The last picture load (or download) was successful.
    \value BrickLink::Loading       BrickStore is currently loading the picture from the local cache.
    \value BrickLink::Updating      BrickStore is currently downloading the picture from BrickLink.
    \value BrickLink::UpdateFailed  The last download from BrickLink failed. isValid might still be
                                    \c true, if there was a valid picture available before the
                                    failed update!
*/
/*! \qmlproperty bool Picture::isValid
    \readonly
    Returns whether the image property currently holds a valid image.
*/
/*! \qmlproperty image Picture::image
    \readonly
    Returns the image if the Picture object isValid, or a null image otherwise.
*/
/*! \qmlmethod Picture::update(bool highPriority = false)
    Tries to re-download the picture from the BrickLink server. If you set \a highPriority to \c
    true the load/download request will be pre-prended to the work queue instead of appended.
*/

QmlPicture::QmlPicture(BrickLink::Picture *pic)
    : QmlWrapperBase(pic)
{
    if (!isNull())
        wrappedObject()->addRef();
}

QmlPicture::QmlPicture(const QmlPicture &copy)
    : QmlPicture(copy.wrappedObject())
{ }

QmlPicture &QmlPicture::operator=(const QmlPicture &assign)
{
    this->~QmlPicture();
    QmlWrapperBase::operator=(assign);
    return *new (this) QmlPicture(assign.wrappedObject());
}

QmlPicture::~QmlPicture()
{
    if (!isNull())
        wrappedObject()->release();
}

QmlBrickLink::UpdateStatus QmlPicture::updateStatus() const
{
    return static_cast<QmlBrickLink::UpdateStatus>(wrapped->updateStatus());
}

void QmlPicture::update(bool highPriority)
{
    if (!isNull())
        wrappedObject()->update(highPriority);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype PriceGuide
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents the price guide for a BrickLink item.

    Each price guide of an item in the BrickLink catalog is available as a PriceGuide object.

    You cannot create PriceGuide objects yourself, but you can retrieve a PriceGuide object given the
    item and color id via BrickLink::priceGuide().

    \note PriceGuides aren't readily available, but need to be asynchronously loaded (or even
          downloaded) at runtime. You need to connect to the signal BrickLink::priceGuideUpdated()
          to know when the data has been loaded.

    The following three enumerations are used to retrieve the price guide data from this object:

    \b Time
    \value BrickLink.PastSix   The sales in the last six months.
    \value BrickLink.Current   The items currently for sale.

    \b Condition
    \value BrickLink.New       Only items in new condition.
    \value BrickLink.Used      Only items in used condition.

    \b Price
    \value BrickLink.Lowest    The lowest price.
    \value BrickLink.Average   The average price.
    \value BrickLink.WAverage  The weighted average price.
    \value BrickLink.Highest   The highest price.

*/
/*! \qmlproperty bool PriceGuide::isNull
    \readonly
    Returns whether this PriceGuide is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item PriceGuide::item
    \readonly
    The BrickLink item reference this price guide is requested for.
*/
/*! \qmlproperty Color PriceGuide::color
    \readonly
    The BrickLink color reference this price guide is requested for.
*/
/*! \qmlproperty date PriceGuide::lastUpdated
    \readonly
    Holds the time stamp of the last successful update of this price guide.
*/
/*! \qmlproperty UpdateStatus PriceGuide::updateStatus
    \readonly
    Returns the current update status. The available values are:
    \value BrickLink.Ok            The last picture load (or download) was successful.
    \value BrickLink.Loading       BrickStore is currently loading the picture from the local cache.
    \value BrickLink.Updating      BrickStore is currently downloading the picture from BrickLink.
    \value BrickLink.UpdateFailed  The last download from BrickLink failed. isValid might still be
                                   \c true, if there was a valid picture available before the failed
                                   update!
*/
/*! \qmlproperty bool PriceGuide::isValid
    \readonly
    Returns whether this object currently holds valid price guide data.
*/
/*! \qmlmethod PriceGuide::update(bool highPriority = false)
    Tries to re-download the price guide from the BrickLink server. If you set \a highPriority to \c
    true the load/download request will be pre-prended to the work queue instead of appended.
*/
/*! \qmlmethod int PriceGuide::quantity(Time time, Condition condition)
    Returns the number of items for sale (or item that have been sold) given the \a time frame and
    \a condition. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time and
    Condition enumerations.
*/
/*! \qmlmethod int PriceGuide::lots(Time time, Condition condition)
    Returns the number of lots for sale (or lots that have been sold) given the \a time frame and
    \a condition. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time and
    Condition enumerations.
*/
/*! \qmlmethod real PriceGuide::price(Time time, Condition condition, Price price)
    Returns the price of items for sale (or item that have been sold) given the \a time frame,
    \a condition and \a price type. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time,
    Condition and Price enumerations.
*/

QmlPriceGuide::QmlPriceGuide(BrickLink::PriceGuide *pg)
    : QmlWrapperBase(pg)
{
    if (!isNull())
        wrappedObject()->addRef();
}

QmlPriceGuide::QmlPriceGuide(const QmlPriceGuide &copy)
    : QmlPriceGuide(copy.wrappedObject())
{ }

QmlPriceGuide &QmlPriceGuide::operator=(const QmlPriceGuide &assign)
{
    this->~QmlPriceGuide();
    QmlWrapperBase::operator=(assign);
    return *new (this) QmlPriceGuide(assign.wrappedObject());
}

QmlPriceGuide::~QmlPriceGuide()
{
    if (!isNull())
        wrappedObject()->release();
}

QmlBrickLink::UpdateStatus QmlPriceGuide::updateStatus() const
{
    return static_cast<QmlBrickLink::UpdateStatus>(wrapped->updateStatus());
}

void QmlPriceGuide::update(bool highPriority)
{
    if (!isNull())
        wrappedObject()->update(highPriority);
}

int QmlPriceGuide::quantity(QmlBrickLink::Time time, QmlBrickLink::Condition condition) const
{
    return wrapped->quantity(static_cast<BrickLink::Time>(time),
                             static_cast<BrickLink::Condition>(condition));
}

int QmlPriceGuide::lots(QmlBrickLink::Time time, QmlBrickLink::Condition condition) const
{
    return wrapped->lots(static_cast<BrickLink::Time>(time),
                         static_cast<BrickLink::Condition>(condition));
}

double QmlPriceGuide::price(QmlBrickLink::Time time, QmlBrickLink::Condition condition,
                            QmlBrickLink::Price price) const
{
    return wrapped->price(static_cast<BrickLink::Time>(time),
                          static_cast<BrickLink::Condition>(condition),
                          static_cast<BrickLink::Price>(price));
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype Lot
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represent a lot in a document.

    A Lot corresponds to a row in a BrickStore document.
*/
/*! \qmlproperty bool Lot::isNull
    \readonly
    Returns whether this Lot is \c null. Since this type is a value wrapper around a C++ object, we
    cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item Lot::item
    The \l Item represented by this lot. Can be a BrickLink::noItem, if there's no item set.
*/
/*! \qmlproperty Color Lot::color
    The \l Color selected for this lot. Can be BrickLink::noColor, if there's no color set.
*/
/*! \qmlproperty Category Lot::category
    \readonly
    The \l Category of the lot's item or BrickLink::noCategory, if the lot's item is not valid.
*/
/*! \qmlproperty ItemType Lot::itemType
    \readonly
    The ItemType of the lot's item or BrickLink::noItemType, if the lot's item is not valid.
*/
/*! \qmlproperty string Lot::itemId
    \readonly
    The id of the lot's item. The same as \c{item.id}, but you don't have to check for isNull on
    \c item.
*/
/*! \qmlproperty string Lot::id
    \readonly
    Obsolete. Please use itemId instead.
*/
/*! \qmlproperty string Lot::itemName
    \readonly
    The name of the lot's item. The same as \c{item.name}, but you don't have to
    check for isNull on \c item.
*/
/*! \qmlproperty string Lot::name
    \readonly
    Obsolete. Please use itemName instead.
*/
/*! \qmlproperty string Lot::colorName
    \readonly
    The color name of the lot's item. The same as \c{color.name}, but you don't have to check for
    isNull on \c color.
*/
/*! \qmlproperty string Lot::categoryName
    \readonly
    The category name of the lot's item. The same as \c{item.category.name}, but you don't have to
    check for isNull on \c item.
*/
/*! \qmlproperty string Lot::itemTypeName
    \readonly
    The item-type name of the lot's item. The same as \c{item.itemType.name}, but you don't have to
    check for isNull on \c item.
*/
/*! \qmlproperty int Lot::itemYearReleased
    \readonly
    The year the lot's item was first released.
*/

/*! \qmlproperty Status Lot::status
    Represents the status of this lot. The Status enumeration has these values:
    \value BrickLink.Include   The green checkmark in the UI.
    \value BrickLink.Exclude   The red stop sign in the UI.
    \value BrickLink.Extra     The blue plus sign in the UI.
*/
/*! \qmlproperty Condition Lot::condition
    Describes the condition of this lot. The Condition enumeration has these values:
    \value BrickLink.New    The items in this lot are new.
    \value BrickLink.Used   The items in this lot are used.
*/
/*! \qmlproperty SubCondition Lot::subCondition
    Describes the sub-condition of this lot, if it represents a set. The SubCondition enumeration
    has these values:
    \value BrickLink.None         No sub-condition is set.
    \value BrickLink.Complete     The set is complete.
    \value BrickLink.Incomplete   The set is not complete.
    \value BrickLink.Sealed       The set is still sealed.
*/

/*! \qmlproperty string Lot::comments
    The comment or description for this lot. This is the public text that a buyer can see.
*/
/*! \qmlproperty string Lot::remarks
    The remark is the private text that only the seller can see.
*/

/*! \qmlproperty int Lot::quantity
    The quantity of the item.
*/
/*! \qmlproperty int Lot::bulkQuantity
    The bulk quantity. This lot can only be sold in multiple of this.
*/
/*! \qmlproperty int Lot::tier1Quantity
    The tier-1 quantity: if a buyer buys this quantity or more, the price will be tier1Price
    instead of price.
*/
/*! \qmlproperty int Lot::tier2Quantity
    The tier-2 quantity: if a buyer buys this quantity or more, the price will be tier2Price
    instead of tier1Price.
    \note This value needs to be larger than tier1Quantity.
*/
/*! \qmlproperty int Lot::tier3Quantity
    The tier-3 quantity: if a buyer buys this quantity or more, the price will be tier3Price
    instead of tier2Price.
    \note This value needs to be larger than tier2Quantity.
*/

/*! \qmlproperty real Lot::price
    The unit price of the item.
*/
/*! \qmlproperty real Lot::tier1Price
    The tier-3 price: this will be the price, if a buyer buys tier1Quantity or more parts.
    \note This value needs to be smaller than price.
*/
/*! \qmlproperty real Lot::tier2Price
    The tier-3 price: this will be the price, if a buyer buys tier2Quantity or more parts.
    \note This value needs to be smaller than tier2Price.
*/
/*! \qmlproperty real Lot::tier3Price
    The tier-3 price: this will be the price, if a buyer buys tier3Quantity or more parts.
    \note This value needs to be smaller than tier2Price.
*/

/*! \qmlproperty int Lot::sale
    The optional sale on this lots in percent. \c{[0 .. 100]}
*/
/*! \qmlproperty real Lot::total
    \readonly
    A convenience value, return price times quantity.
*/

/*! \qmlproperty uint Lot::lotId
    The BrickLink lot-id, which uniquely identifies a lot for sale on BrickLink.
*/
/*! \qmlproperty bool Lot::retain
    A boolean flag indicating whether the lot should be retained in the store's stockroom if the
    last item has been sold.
*/
/*! \qmlproperty Stockroom Lot::stockroom
    Describes if and in which stockroom this lot is located. The Stockroom enumeration has these
    values:
    \value BrickLink.None  Not in a stockroom.
    \value BrickLink.A     In stockroom \c A.
    \value BrickLink.B     In stockroom \c B.
    \value BrickLink.C     In stockroom \c C.
*/

/*! \qmlproperty real Lot::totalWeight
    The weight of the complete lot, i.e. quantity times the weight of a single item.
*/
/*! \qmlproperty string Lot::reserved
    The name of the buyer this item is reserved for or an empty string.
*/
/*! \qmlproperty bool Lot::alternate
    A boolean flag denoting this lot as an \e alternate in a set inventory.
    \note This value does not get saved.
*/
/*! \qmlproperty uint Lot::alternateId
    If this lot is an \e alternate in a set inventory, this property holds the \e{alternate id}.
    \note This value does not get saved.
*/
/*! \qmlproperty bool Lot::counterPart
    A boolean flag denoting this lot as a \e{counter part} in a set inventory.
    \note This value does not get saved.
*/

/*! \qmlproperty bool Lot::incomplete
    \readonly
    Returns \c false if this lot has a valid item and color, or \c true otherwise.
*/

/*! \qmlproperty image Lot::image
    \readonly
    The item's image in the lot's color; can be \c null.
    \note The image isn't readily available, but needs to be asynchronously loaded (or even
          downloaded) at runtime. See the Picture type for more information.
*/

QmlLot::QmlLot(BrickLink::Lot *lot, DocumentModel *model)
    : QmlWrapperBase(lot)
    , m_model(model)
{ }

QImage QmlLot::image() const
{
    static QImage dummy;
    auto pic = ::BrickLink::core()->picture(get()->item(), get()->color(), true);
    return pic ? pic->image() : dummy;
}

QmlLot::Setter::Setter(QmlLot *lot)
    : m_lot((lot && !lot->isNull()) ? lot : nullptr)
{
    if (m_lot)
        m_to = *m_lot->wrapped;
}

BrickLink::Lot *QmlLot::Setter::to()
{
    return &m_to;
}

QmlLot::Setter::~Setter()
{
    if (!m_lot->m_model) {
        qmlWarning(nullptr) << "Cannot modify a const Lot";
        return;
    }

    if (m_lot && (*m_lot->wrapped != m_to)) {
        if (m_lot->m_model)
            m_lot->m_model->changeLot(m_lot->wrapped, m_to);
        else
            *m_lot->wrapped = m_to;
    }
}

QmlLot::Setter QmlLot::set()
{
    return Setter(this);
}

BrickLink::Lot *QmlLot::get() const
{
    return wrapped;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlLots::QmlLots(DocumentModel *model)
    : QObject(model)
    , m_model(model)
{ }

int QmlLots::add(QmlItem item, QmlColor color)
{
    auto lot = new Lot();
    lot->setItem(item.wrappedObject());
    lot->setColor(color.wrappedObject());
    m_model->appendLot(std::move(lot));
    return m_model->lots().indexOf(lot);
}

void QmlLots::remove(QmlLot lot)
{
    if (!lot.isNull() && m_model && (lot.m_model == m_model))
        m_model->removeLot(lot.wrappedObject());
}

void QmlLots::removeAt(int index)
{
    if ((index >= 0) && (index < m_model->lotCount())) {
        Lot *lot = m_model->lots().at(index);
        m_model->removeLot(lot);
    }
}

QmlLot QmlLots::at(int index)
{
    if (index < 0 || index >= m_model->lotCount())
        return QmlLot { };
    return QmlLot(m_model->lots().at(index), m_model);
}

#include "moc_bricklink_wrapper.cpp"

