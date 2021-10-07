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

#include <QPainter>
#include <QQmlEngine>
#include <QQmlContext>
#include "qqmlinfo.h"

#include "utility/utility.h"
#include "utility/currency.h"
#include "bricklink/picture.h"
#include "bricklink/priceguide.h"
#include "bricklink/order.h"
#include "bricklink/store.h"
#include "bricklink_wrapper.h"
#include "common/documentmodel.h"


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
    qmlRegisterUncreatableType<BrickLink::Store>("BrickStore", 1, 0, "Store",
                                                 cannotCreate.arg("Store"_l1));

    qmlRegisterType<QmlImageItem>("BrickStore", 1, 0, "QImageItem");
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

QmlItem QmlBrickLink::noItem() const
{
    return QmlItem { };
}

QmlColor QmlBrickLink::noColor() const
{
    return QmlColor { };
}

QmlColor QmlBrickLink::color(const QVariant &v) const
{
    if (v.userType() == qMetaTypeId<const BrickLink::Color *>())
        return v.value<const BrickLink::Color *>();
    else if (v.userType() == QMetaType::QString)
        return d->colorFromName(v.toString());
    else
        return d->color(v.value<uint>());
}

QmlColor QmlBrickLink::colorFromLDrawId(int ldrawId) const
{
    return d->colorFromLDrawId(ldrawId);
}

QmlCategory QmlBrickLink::category(int id) const
{
    return d->category(uint(id));
}

QmlItemType QmlBrickLink::itemType(const QString &itemTypeId) const
{
    return d->itemType(firstCharInString(itemTypeId));
}

QmlItem QmlBrickLink::item(const QString &itemTypeId, const QString &itemId) const
{
    return d->item(firstCharInString(itemTypeId), itemId.toLatin1());
}

QmlPriceGuide QmlBrickLink::priceGuide(QmlItem item, QmlColor color, bool highPriority)
{
    return QmlPriceGuide { d->priceGuide(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

QmlPicture QmlBrickLink::picture(QmlItem item, QmlColor color, bool highPriority)
{
    return QmlPicture { d->picture(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

QmlPicture QmlBrickLink::largePicture(QmlItem item, bool highPriority)
{
    return QmlPicture { d->largePicture(item.wrappedObject(), highPriority) };
}

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


char QmlBrickLink::firstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlCategory::QmlCategory(const BrickLink::Category *cat)
    : QmlWrapperBase(cat)
{ }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


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


QmlLot::QmlLot(BrickLink::Lot *lot, DocumentModel *document)
    : QmlWrapperBase(lot)
    , document(document)
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
    if (!m_lot->document) {
        qmlWarning(nullptr) << "Cannot modify a const Lot";
        return;
    }

    if (m_lot && (*m_lot->wrapped != m_to)) {
        if (m_lot->document)
            m_lot->document->changeLot(m_lot->wrapped, m_to);
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

#include "moc_bricklink_wrapper.cpp"
