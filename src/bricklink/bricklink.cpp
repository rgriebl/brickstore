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
#include <cstdio>
#include <cstdlib>

#include <QFile>
#include <QBuffer>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QtXml/QDomDocument>
#include <QRegExp>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QThread>
#include <QUrlQuery>

#include "config.h"
#include "utility.h"
#include "stopwatch.h"
#include "bricklink.h"
#include "chunkreader.h"
#include "chunkwriter.h"
#include "exception.h"


namespace BrickLink {

QUrl Core::url(UrlList u, const void *opt, const void *opt2)
{
    QUrl url;

    switch (u) {
    case URL_InventoryRequest:
        url = "https://www.bricklink.com/catalogInvAdd.asp";
        break;

    case URL_WantedListUpload:
        url = "https://www.bricklink.com/wantedXML.asp";
        break;

    case URL_InventoryUpload:
        url = "https://www.bricklink.com/invXML.asp";
        break;

    case URL_InventoryUpdate:
        url = "https://www.bricklink.com/invXML.asp#update";
        break;

    case URL_CatalogInfo:
        if (opt) {
            const Item *item = static_cast <const Item *>(opt);

            url = "https://www.bricklink.com/catalogItem.asp";
            QUrlQuery query;
            query.addQueryItem(QChar(item->itemType()->id()), item->id());
            url.setQuery(query);
        }
        break;

    case URL_PriceGuideInfo:
        if (opt && opt2) {
            const Item *item = static_cast <const Item *>(opt);

            url = "https://www.bricklink.com/catalogPG.asp";
            QUrlQuery query;
            query.addQueryItem(QChar(item->itemType()->id()), item->id());
            if (item->itemType()->hasColors())
                query.addQueryItem("colorID", QString::number(static_cast <const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;

    case URL_LotsForSale:
        if (opt && opt2) {
            const Item *item = static_cast <const Item *>(opt);

            url = "https://www.bricklink.com/search.asp";
            QUrlQuery query;
            query.addQueryItem("viewFrom", "sa");
            query.addQueryItem("itemType", QChar(item->itemType()->id()));

            // workaround for BL not accepting the -X suffix for sets, instructions and boxes
            QString id = item->id();
            char itt = item->itemType()->id();

            if (itt == 'S' || itt == 'I' || itt == 'O') {
                int pos = id.lastIndexOf('-');
                if (pos >= 0)
                    id.truncate(pos);
            }
            query.addQueryItem("q", id);

            if (item->itemType()->hasColors())
                query.addQueryItem("colorID", QString::number(static_cast <const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;

    case URL_AppearsInSets:
        if (opt && opt2) {
            const Item *item = static_cast <const Item *>(opt);

            url = "https://www.bricklink.com/catalogItemIn.asp";
            QUrlQuery query;
            query.addQueryItem(QChar(item->itemType()->id()), item->id());
            query.addQueryItem("in", "S");

            if (item->itemType()->hasColors())
                query.addQueryItem("colorID", QString::number(static_cast <const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;

    case URL_ColorChangeLog:
        url = "https://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=R";
        break;

    case URL_ItemChangeLog: {
        url = "https://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=I";
        QUrlQuery query;
        if (opt)
            query.addQueryItem("q", static_cast <const char *>(opt));
        url.setQuery(query);
        break;
    }
    case URL_StoreItemDetail: {
        if (opt) {
            url = "https://www.bricklink.com/inventory_detail.asp";
            QUrlQuery query;
            query.addQueryItem("invID", QString::number(*static_cast <const unsigned int *>(opt)));
            url.setQuery(query);
        }
        break;
    }
    }
    return url;
}


const QImage Core::noImage(const QSize &s) const
{
    QString key = QString("%1x%2").arg(s.width()).arg(s.height());

    QMutexLocker lock(&m_corelock);

    QImage img = m_noimages.value(key);

    if (img.isNull()) {
        img = QImage(s, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing);

        int w = img.width();
        int h = img.height();
        bool high = (w < h);

        p.fillRect(0, 0, w, h, Qt::white);

        QRect r(high ? 0 : (w - h) / 2, high ? (w -h) / 2 : 0, high ? w : h, high ? w : h);
        int w4 = r.width() / 4;
        r.adjust(w4, w4, -w4, -w4);

        static const QColor coltable [] = {
            QColor(0x00, 0x00, 0x00),
            QColor(0x3f, 0x3f, 0x3f),
            QColor(0xff, 0x7f, 0x7f)
        };

        for (const auto &i : coltable) {
            r.adjust(-1, -1, -1, -1);

            p.setPen(QPen(i, 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(r.x(), r.y(), r.right(), r.bottom());
            p.drawLine(r.right(), r.y(), r.x(), r.bottom());
        }
        p.end();

        m_noimages.insert(key, img);
    }
    return img;
}


const QImage Core::colorImage(const Color *col, int w, int h) const
{
    if (!col || w <= 0 || h <= 0)
        return QImage();

    QString key = QString("%1:%2x%3").arg(col->id()).arg(w).arg(h);

    QMutexLocker lock(&m_corelock);

    QImage img = m_colimages.value(key);

    if (img.isNull()) {
        QColor c = col->color();

        img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing);
        QRect r = img.rect();

        QBrush brush;

        if (col->isGlitter()) {
            brush = QBrush(Utility::contrastColor(c, 0.25), Qt::Dense6Pattern);
        }
        else if (col->isSpeckle()) {
            // hack for speckled colors
            QColor c2;

            if (!c.isValid()) {
                QString name = col->name();
                int dash = name.indexOf('-');
                if (dash > 0) {
                    QString basename = name.mid(8, dash - 8);
                    if (basename.startsWith("DB"))
                        basename.replace(0, 2, "Dark Bluish ");
                    QString speckname = name.mid(dash + 1);

                    const Color *basec = colorFromName(basename);
                    const Color *speckc = colorFromName(speckname);

                    if (basec)
                        c = basec->color();
                    if (speckc)
                        c2 = speckc->color();
                }
            }
            if (c.isValid()) {
                if (!c2.isValid()) // fake
                    c2 = Utility::contrastColor(c, 0.2);
                brush = QBrush(c2, Qt::Dense7Pattern);
            }
        }
        else if (col->isMetallic()) {
            QColor c2 = Utility::gradientColor(c, Qt::black);

            QLinearGradient gradient(0, 0, 0, r.height());
            gradient.setColorAt(0,   c2);
            gradient.setColorAt(0.3, c);
            gradient.setColorAt(0.7, c);
            gradient.setColorAt(1,   c2);
            brush = gradient;
        }
        else if (col->isChrome()) {
            QColor c2 = Utility::gradientColor(c, Qt::black);

            QLinearGradient gradient(0, 0, r.width(), 0);
            gradient.setColorAt(0,   c2);
            gradient.setColorAt(0.3, c);
            gradient.setColorAt(0.7, c);
            gradient.setColorAt(1,   c2);
            brush = gradient;
        }

        if (c.isValid()) {
            p.fillRect(r, c);
            p.fillRect(r, brush);
        }
        else {
            p.fillRect(0, 0, w, h, Qt::white);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::darkGray);
            p.setBrush(QColor(255, 255, 255, 128));
            p.drawRect(r);
            p.drawLine(0, 0, w, h);
            p.drawLine(0, h, w, 0);
        }

        if (col->isTransparent()) {
            QLinearGradient gradient(0, 0, r.width(), r.height());
            gradient.setColorAt(0,   Qt::transparent);
            gradient.setColorAt(0.4, Qt::transparent);
            gradient.setColorAt(0.6, QColor(255, 255, 255, 100));
            gradient.setColorAt(1,   QColor(255, 255, 255, 100));

            p.fillRect(r, gradient);
        }
        p.end();

        m_colimages.insert(key, img);
    }
    return img;
}



namespace {

static bool check_and_create_path(const QString &p)
{
    QFileInfo fi(p);

    if (!fi.exists()) {
        QDir(p).mkpath(QStringLiteral("."));
        fi.refresh();
    }
    return (fi.exists() && fi.isDir() && fi.isReadable() && fi.isWritable());
}

} // namespace

QString Core::dataPath() const
{
    return m_datadir;
}

QString Core::dataPath(const ItemType *item_type) const
{
    QString p = dataPath();
    p += item_type->id();
    p += QDir::separator();

    if (!check_and_create_path(p))
        return QString();

    return p;
}

QString Core::dataPath(const Item *item) const
{
    QString p = dataPath(item->itemType());
    const QString id = item->id();

    // Avoid huge directories with 1000s of entries.
    uchar hash = qHash(id, 42) & 0xff; // sse4.2 is only used if a seed value is supplied
    QString hashStr = QString::number(hash, 16);
    if (hash < 0x10)
        hashStr.prepend(QLatin1Char('0'));

    p += hashStr;
    p += QDir::separator();

    if (!check_and_create_path(p))
        return QString();

    p += item->m_id;
    p += QDir::separator();

    if (!check_and_create_path(p))
        return QString();

    return p;
}

QString Core::dataPath(const Item *item, const Color *color) const
{
    QString p = dataPath(item);
    p += QString::number(color->id());
    p += QDir::separator();

    if (!check_and_create_path(p))
        return QString();

    return p;
}



Core *Core::s_inst = nullptr;

Core *Core::create(const QString &datadir, QString *errstring)
{
    if (!s_inst) {
        s_inst = new Core(datadir);
        QString test = s_inst->dataPath();

        qDebug() << "Loading database from" << test;

        if (test.isNull() || !check_and_create_path(test)) {
            delete s_inst;
            s_inst = nullptr;

            if (errstring)
                *errstring = tr("Data directory \'%1\' is not both read- and writable.").arg(datadir);
        }
    }
    return s_inst;
}

Core::Core(const QString &datadir)
    : m_datadir(QDir::cleanPath(QDir(datadir).absolutePath()) + QStringLiteral("/"))
    , m_c_locale(QLocale::c())
    , m_corelock(QMutex::Recursive)
{
    m_pic_diskload.setMaxThreadCount(QThread::idealThreadCount() * 3);
    m_online = true;

    // cache size is physical memory * 0.25, at least 128MB, at most 1GB
    quint64 cachemem = qBound(128ULL *1024*1024, Utility::physicalMemory() / 4, 1024ULL *1024*1024);
    //quint64 cachemem = 1024*1024; // DEBUG

    m_pg_cache.setMaxCost(500);          // each priceguide has a cost of 1
    m_pic_cache.setMaxCost(int(cachemem));    // each pic has a cost of (w*h*d/8 + 1024)
}

Core::~Core()
{
    clear();

    delete m_transfer;
    s_inst = nullptr;
}

void Core::setTransfer(Transfer *trans)
{
    Transfer *old = m_transfer;

    m_transfer = trans;

    if (old) { // disconnect
        disconnect(old, &Transfer::finished, this, &Core::priceGuideJobFinished);
        disconnect(old, &Transfer::finished, this, &Core::pictureJobFinished);

        disconnect(old, &Transfer::progress, this, &Core::transferJobProgress);
    }
    if (trans) { // connect
        connect(trans, &Transfer::finished, this, &Core::priceGuideJobFinished);
        connect(trans, &Transfer::finished, this, &Core::pictureJobFinished);

        connect(trans, &Transfer::progress, this, &Core::transferJobProgress);
    }
}

Transfer *Core::transfer() const
{
    return m_transfer;
}

void Core::setUpdateIntervals(const QMap<QByteArray, int> &intervals)
{
    m_pic_update_iv = intervals["Picture"];
    m_pg_update_iv = intervals["PriceGuide"];
}

bool Core::updateNeeded(bool valid, const QDateTime &last, int iv)
{
    return (iv > 0) && (!valid || (last.secsTo(QDateTime::currentDateTime()) > iv));
}

void Core::setOnlineStatus(bool on)
{
    m_online = on;
}

bool Core::onlineStatus() const
{
    return m_online;
}


const QHash<uint, const Color *> &Core::colors() const
{
    return m_colors;
}

const QHash<uint, const Category *> &Core::categories() const
{
    return m_categories;
}

const QHash<char, const ItemType *> &Core::itemTypes() const
{
    return m_item_types;
}

const QVector<const Item *> &Core::items() const
{
    return m_items;
}

const Category *Core::category(uint id) const
{
    return m_categories.value(id);
}

const Color *Core::color(uint id) const
{
    return m_colors.value(id);
}

const Color *Core::colorFromName(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;

    for (const Color *col : m_colors) {
        if (!col->name().compare(name, Qt::CaseInsensitive))
            return col;
    }
    return nullptr;
}


const Color *Core::colorFromLDrawId(int ldraw_id) const
{
    for (const Color *col : m_colors) {
        if (col->ldrawId() == ldraw_id)
            return col;
    }
    return nullptr;
}


const ItemType *Core::itemType(char id) const
{
    return m_item_types.value(id);
}

const Item *Core::item(char tid, const QString &id) const
{
    Item key;
    key.m_item_type = itemType(tid);
    key.m_id = id;

    if (!key.m_item_type || key.m_id.isEmpty())
        return nullptr;

    // Finds the lower bound in at most log(last - first) + 1 comparisons
    auto it = std::lower_bound(m_items.constBegin(), m_items.constEnd(), &key, Item::lessThan);
    if (it != m_items.constEnd() && !Item::lessThan(&key, *it))
        return *it;

    return nullptr;
}

void Core::cancelPictureTransfers()
{
    QMutexLocker lock(&m_corelock);

    m_pic_diskload.clear();
    m_pic_diskload.waitForDone();
    if (m_transfer)
        m_transfer->abortAllJobs();
}

void Core::cancelPriceGuideTransfers()
{
    QMutexLocker lock(&m_corelock);

    if (m_transfer)
        m_transfer->abortAllJobs();
}

QString Core::defaultDatabaseName(DatabaseVersion version) const
{
    return QString("database-v%1").arg(int(version));
}

void Core::clear()
{
    QMutexLocker lock(&m_corelock);

    cancelPictureTransfers();
    cancelPriceGuideTransfers();

    m_pg_cache.clear();
    m_pic_cache.clear();

    qDeleteAll(m_colors);
    qDeleteAll(m_item_types);
    qDeleteAll(m_categories);
    qDeleteAll(m_items);

    m_colors.clear();
    m_item_types.clear();
    m_categories.clear();
    m_items.clear();
    m_changelog.clear();
}



Core::ParseItemListXMLResult Core::parseItemListXML(const QDomElement &root, ItemListXMLHint hint)
{
    ParseItemListXMLResult result;
    QString roottag, itemtag;

    switch (hint) {
    case XMLHint_Order     : roottag = QLatin1String("ORDER"); itemtag = QLatin1String("ITEM"); break;
    case XMLHint_MassUpload:
    case XMLHint_MassUpdate:
    case XMLHint_WantedList:
    case XMLHint_Inventory : roottag = QLatin1String("INVENTORY"); itemtag = QLatin1String("ITEM"); break;
    case XMLHint_BrickStore: roottag = QLatin1String("Inventory"); itemtag = QLatin1String("Item"); break;
    }

    if (root.nodeName() != roottag)
        return result;

    if (hint == XMLHint_BrickStore)
        result.currencyCode = root.attribute(QLatin1String("Currency"));
    if (result.currencyCode.isEmpty())
        result.currencyCode = QLatin1String("USD");

    result.items = new InvItemList;
    bool multicurrency = false;
    QLocale c = QLocale::c();

    for (QDomNode itemn = root.firstChild(); !itemn.isNull(); itemn = itemn.nextSibling()) {
        if (itemn.nodeName() != itemtag)
            continue;

        auto *ii = new InvItem();

        QString itemid, itemname;
        QString itemtypeid, itemtypename;
        QString colorid, colorname;
        QString categoryid, categoryname;

        bool has_orig_price = false;
        bool has_orig_qty = false;

        for (QDomNode n = itemn.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            QString tag = n.toElement().tagName();
            QString val = n.toElement().text();

            // ### BrickLink XML ###
            if (hint != XMLHint_BrickStore) {
                if (tag == QLatin1String("ITEMID"))
                    itemid = val;
                else if (tag == QLatin1String("COLOR"))
                    colorid = val;
                else if (tag == QLatin1String("CATEGORY"))
                    categoryid = val;
                else if (tag == QLatin1String("ITEMTYPE"))
                    itemtypeid = val;
                else if (tag == QLatin1String("BASECURRENCYCODE")) {
                    if (result.items->isEmpty())
                        result.currencyCode = val;
                    else if (val != result.currencyCode)
                        multicurrency = true;
                }
                else if (tag == QLatin1String("PRICE"))
                    ii->setPrice(c.toDouble(val));
                else if (tag == QLatin1String( "BULK"))
                    ii->setBulkQuantity(c.toInt(val));
                else if (tag == QLatin1String( "QTY"))
                    ii->setQuantity(c.toInt(val));
                else if (tag == QLatin1String("SALE"))
                    ii->setSale(c.toInt(val));
                else if (tag == QLatin1String("CONDITION"))
                    ii->setCondition(val == QLatin1String("N") ? Condition::New : Condition::Used);
                else if (tag == QLatin1String("SUBCONDITION")) {
                    ii->setSubCondition(val == QLatin1String("C") ? SubCondition::Complete : \
                                        val == QLatin1String("I") ? SubCondition::Incomplete : \
                                        val == QLatin1String("M") ? SubCondition::Sealed : SubCondition::None);
                }
                else if (tag == QLatin1String("DESCRIPTION"))
                    ii->setComments(val);
                else if (tag == QLatin1String("REMARKS"))
                    ii->setRemarks(val);
                else if (tag == QLatin1String("TQ1"))
                    ii->setTierQuantity(0, c.toInt(val));
                else if (tag == QLatin1String("TQ2"))
                    ii->setTierQuantity(1, c.toInt(val));
                else if (tag == QLatin1String("TQ3"))
                    ii->setTierQuantity(2, c.toInt(val));
                else if (tag == QLatin1String("TP1"))
                    ii->setTierPrice(0, c.toDouble(val));
                else if (tag == QLatin1String("TP2"))
                    ii->setTierPrice(1, c.toDouble(val));
                else if (tag == QLatin1String("TP3"))
                    ii->setTierPrice(2, c.toDouble(val));
                else if (tag == QLatin1String("LOTID"))
                    ii->setLotId(c.toUInt(val));
                else if (tag == QLatin1String("RETAIN"))
                    ii->setRetain(val == QLatin1String("Y"));
                else if (tag == QLatin1String("STOCKROOM")) {
                    if (ii->stockroom() == Stockroom::None)
                        ii->setStockroom(val == QLatin1String("Y") ? Stockroom::A : Stockroom::None);
                } else if (tag == QLatin1String("STOCKROOMID")) {
                    ii->setStockroom(val == "A" ? Stockroom::A : \
                                     val == "B" ? Stockroom::B : \
                                     val == "C" ? Stockroom::C : Stockroom::None);
                } else if (tag == QLatin1String("BUYERUSERNAME"))
                    ii->setReserved(val);
            }

            // ### BrickLink Order (workaround for broken BL script) ###
            if (hint == XMLHint_Order) {
                // The remove(',') stuff is a workaround for the
                // broken Order XML generator: the XML contains , as
                // thousands-separator: 1,752 instead of 1752

                if (tag == QLatin1String("QTY"))
                    ii->setQuantity(val.remove(QLatin1Char(',')).toInt());
            }

            // ### Inventory Request ###
            else if (hint == XMLHint_Inventory) {
                if ((tag == QLatin1String("EXTRA")) && (val == QLatin1String("Y")))
                    ii->setStatus(Status::Extra);
                else if (tag == QLatin1String("COUNTERPART"))
                     ii->setCounterPart((val == QLatin1String("Y")));
                else if (tag == QLatin1String("ALTERNATE"))
                     ii->setAlternate((val == QLatin1String("Y")));
                else if (tag == QLatin1String("MATCHID"))
                     ii->setAlternateId(c.toUInt(val));
                else if (tag == QLatin1String("ITEMNAME"))    // BrickStore extension for Peeron inventories
                    itemname = val;
                else if (tag == QLatin1String("COLORNAME"))   // BrickStore extension for Peeron inventories
                    colorname = val;
            }

            // ### BrickStore BSX ###
            else if (hint == XMLHint_BrickStore) {
                if (tag == QLatin1String("ItemID"))
                    itemid = val;
                else if (tag == QLatin1String("ColorID"))
                    colorid = val;
                else if (tag == QLatin1String("CategoryID"))
                    categoryid = val;
                else if (tag == QLatin1String("ItemTypeID"))
                    itemtypeid = val;
                else if (tag == QLatin1String("ItemName"))
                    itemname = val;
                else if (tag == QLatin1String("ColorName"))
                    colorname = val;
                else if (tag == QLatin1String("CategoryName"))
                    categoryname = val;
                else if (tag == QLatin1String("ItemTypeName"))
                    itemtypename = val;
                else if (tag == QLatin1String("Price"))
                    ii->setPrice(c.toDouble(val));
                else if (tag == QLatin1String("Bulk"))
                    ii->setBulkQuantity(c.toInt(val));
                else if (tag == QLatin1String("Qty"))
                    ii->setQuantity(c.toInt(val));
                else if (tag == QLatin1String("Sale"))
                    ii->setSale(c.toInt(val));
                else if (tag == QLatin1String("Condition"))
                    ii->setCondition(val == QLatin1String("N") ? Condition::New : Condition::Used);
                else if (tag == QLatin1String("SubCondition")) {
                    ii->setSubCondition(val == QLatin1String("C") ? SubCondition::Complete : \
                                        val == QLatin1String("I") ? SubCondition::Incomplete : \
                                        val == QLatin1String("M") ? SubCondition::Sealed : SubCondition::None);
                }
                else if (tag == QLatin1String("Comments"))
                    ii->setComments(val);
                else if (tag == QLatin1String("Remarks"))
                    ii->setRemarks(val);
                else if (tag == QLatin1String("TQ1"))
                    ii->setTierQuantity(0, c.toInt(val));
                else if (tag == QLatin1String("TQ2"))
                    ii->setTierQuantity(1, c.toInt(val));
                else if (tag == QLatin1String("TQ3"))
                    ii->setTierQuantity(2, c.toInt(val));
                else if (tag == QLatin1String("TP1"))
                    ii->setTierPrice(0, c.toDouble(val));
                else if (tag == QLatin1String("TP2"))
                    ii->setTierPrice(1, c.toDouble(val));
                else if (tag == QLatin1String("TP3"))
                    ii->setTierPrice(2, c.toDouble(val));
                else if (tag == QLatin1String("Status")) {
                    Status st = Status::Include;

                    if (val == QLatin1String("X"))
                        st = Status::Exclude;
                    else if (val == QLatin1String("I"))
                        st = Status::Include;
                    else if (val == QLatin1String("E"))
                        st = Status::Extra;
                    else if (val == QLatin1String("?"))
                        st = Status::Unknown;

                    ii->setStatus(st);
                }
                else if (tag == QLatin1String("LotID"))
                    ii->setLotId(c.toUInt(val));
                else if (tag == QLatin1String("Retain"))
                    ii->setRetain(true);
                else if (tag == QLatin1String("Stockroom")) {
                    Stockroom st = Stockroom::None;
                    if (val.isEmpty() || val == "A")
                        st = Stockroom::A;
                    else if (val == "B")
                        st = Stockroom::B;
                    else if (val == "C")
                        st = Stockroom::C;
                    ii->setStockroom(st);
                } else if (tag == QLatin1String("Reserved"))
                    ii->setReserved(val);
                else if (tag == QLatin1String("TotalWeight"))
                    ii->setWeight(c.toDouble(val));
                else if (tag == QLatin1String("OrigPrice")) {
                    ii->setOrigPrice(c.toDouble(val));
                    has_orig_price = true;
                }
                else if (tag == QLatin1String("OrigQty")) {
                    ii->setOrigQuantity(c.toInt(val));
                    has_orig_qty = true;
                }
            }
        }

        bool ok = true;

        //qDebug ( "item (id=%s, color=%s, type=%s, cat=%s", itemid.latin1 ( ), colorid.latin1 ( ), itemtypeid.latin1 ( ), categoryid.latin1 ( ));

        if (!has_orig_price)
            ii->setOrigPrice(ii->price());
        if (!has_orig_qty)
            ii->setOrigQuantity(ii->quantity());

        if (!colorid.isEmpty() && !itemid.isEmpty() && !itemtypeid.isEmpty()) {
            ii->setItem(item(itemtypeid [0].toLatin1(), itemid.toLatin1()));

            if (!ii->item()) {
                qWarning() << "failed: invalid itemid (" << itemid << ") and/or itemtypeid (" << itemtypeid [0] << ")";
                ok = false;
            }

            ii->setColor(color(colorid.toUInt()));

            if (!ii->color()) {
                qWarning() << "failed: invalid color (" << colorid << ")";
                ok = false;
            }
        }
        else
            ok = false;

        if (!ok) {
            qWarning() << "failed: insufficient data (item=" << itemid << ", itemtype=" << itemtypeid [0] << ", category=" << categoryid << ", color=" << colorid << ")";

            auto *inc = new InvItem::Incomplete;

            if (!ii->item()) {
                inc->m_item_id = itemid.toLatin1();
                inc->m_item_name = itemname.toLatin1();
                inc->m_itemtype_id = itemtypeid;
                inc->m_itemtype_name = itemtypename;
                inc->m_category_name = (categoryname.isEmpty() ? categoryid : categoryname).toLatin1();
            }
            if (!ii->color())
                inc->m_color_name = (colorname.isEmpty() ? colorid : colorname).toLatin1();

            ii->setIncomplete(inc);

            ok = true;
            result.invalidItemCount++;
        }

        if (ok)
            result.items->append(ii);
        else
            delete ii;
    }

    if (result.invalidItemCount)
        qWarning() << "Parse XML (hint=" << int(hint) << "): " << result.invalidItemCount << " items have incomplete records";
    if (multicurrency)
        qWarning() << "Parse XML (hint=" << int(hint) << "): Multiple currencies within one XML file are not supported - everything will be parsed as " << result.currencyCode;

    return result;
}



QDomElement Core::createItemListXML(QDomDocument doc, ItemListXMLHint hint, const InvItemList &items, const QString &currencyCode, QMap <QString, QString> *extra)
{
    QString roottag, itemtag;

    switch (hint) {
    case XMLHint_MassUpload:
    case XMLHint_MassUpdate:
    case XMLHint_WantedList:
    case XMLHint_Inventory : roottag = QLatin1String("INVENTORY"); itemtag = QLatin1String("ITEM"); break;
    case XMLHint_BrickStore: roottag = QLatin1String("Inventory"); itemtag = QLatin1String("Item"); break;
    case XMLHint_Order     : break;
    }

    QDomElement root = doc.createElement(roottag);

    if (root.isNull() || roottag.isNull() || itemtag.isEmpty() || items.isEmpty())
        return root;

    if (hint == XMLHint_BrickStore)
        root.setAttribute(QLatin1String("Currency"), currencyCode);

    for (const InvItem *ii : items) {
        if (ii->isIncomplete())
            continue;

        if ((ii->status() == Status::Exclude) && (hint != XMLHint_BrickStore))
            continue;

        if (hint == XMLHint_MassUpdate) {
            bool diffs = ((ii->quantity() != ii->origQuantity()) || !qFuzzyCompare(ii->price(), ii->origPrice()));

            if (!ii->lotId() || !diffs)
                continue;
        }

        QDomElement item = doc.createElement(itemtag);
        root.appendChild(item);
        QLocale c = QLocale::c();

        // ### MASS UPDATE ###
        if (hint == XMLHint_MassUpdate) {
            item.appendChild(doc.createElement(QLatin1String("LOTID")).appendChild(doc.createTextNode(c.toString(ii->lotId()))).parentNode());

            int qdiff = ii->quantity() - ii->origQuantity();
            double pdiff = ii->price() - ii->origPrice();

            if (!qFuzzyIsNull(pdiff))
                item.appendChild(doc.createElement(QLatin1String("PRICE")).appendChild(doc.createTextNode(c.toCurrencyString(ii->price(), {}, 3))).parentNode());
            if (qdiff && (ii->quantity() > 0))
                item.appendChild(doc.createElement(QLatin1String("QTY")).appendChild(doc.createTextNode(QLatin1String(qdiff > 0 ? "+" : "") + c.toString(qdiff))).parentNode());
            else if (qdiff && (ii->quantity() <= 0))
                item.appendChild(doc.createElement(QLatin1String("DELETE")));
        }

        // ### BrickStore BSX ###
        else if (hint == XMLHint_BrickStore) {
            item.appendChild(doc.createElement(QLatin1String("ItemID")).appendChild(doc.createTextNode(QString(ii->item()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ItemTypeID")).appendChild(doc.createTextNode(QChar(ii->itemType()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ColorID")).appendChild(doc.createTextNode(c.toString(ii->color()->id()))).parentNode());

            // this extra information is useful, if the e.g.the color- or item-id
            // are no longer available after a database update
            item.appendChild(doc.createElement(QLatin1String("ItemName")).appendChild(doc.createTextNode(ii->item()->name())).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ItemTypeName")).appendChild(doc.createTextNode(ii->itemType()->name())).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ColorName")).appendChild(doc.createTextNode(ii->color()->name())).parentNode());
            item.appendChild(doc.createElement(QLatin1String("CategoryID")).appendChild(doc.createTextNode(c.toString(ii->category()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("CategoryName")).appendChild(doc.createTextNode(ii->category()->name())).parentNode());

            {
                const char *st;
                switch (ii->status()) {
                default             :
                case Status::Unknown: st = "?"; break;
                case Status::Extra  : st = "E"; break;
                case Status::Exclude: st = "X"; break;
                case Status::Include: st = "I"; break;
                }
                item.appendChild(doc.createElement(QLatin1String("Status")).appendChild(doc.createTextNode(QLatin1String(st))).parentNode());
            }

            item.appendChild(doc.createElement(QLatin1String("Qty")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("Price")).appendChild(doc.createTextNode(c.toCurrencyString(ii->price(), { }, 3))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("Condition")).appendChild(doc.createTextNode(QLatin1String((ii->condition() == Condition::New) ? "N" : "U"))).parentNode());

            if (ii->subCondition() != SubCondition::None) {
                const char *st;
                switch (ii->subCondition()) {
                case SubCondition::Incomplete: st = "I"; break;
                case SubCondition::Complete  : st = "C"; break;
                case SubCondition::Sealed    : st = "M"; break;
                default                      : st = "?"; break;
                }
                item.appendChild(doc.createElement(QLatin1String("SubCondition")).appendChild(doc.createTextNode(QLatin1String(st))).parentNode());
            }

            if (ii->bulkQuantity() != 1)
                item.appendChild(doc.createElement(QLatin1String("Bulk")).appendChild(doc.createTextNode(c.toString(ii->bulkQuantity()))).parentNode());
            if (ii->sale())
                item.appendChild(doc.createElement(QLatin1String("Sale")).appendChild(doc.createTextNode(c.toString(ii->sale()))).parentNode());
            if (!ii->comments().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("Comments")).appendChild(doc.createTextNode(ii->comments())).parentNode());
            if (!ii->remarks().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("Remarks")).appendChild(doc.createTextNode(ii->remarks())).parentNode());
            if (ii->retain())
                item.appendChild(doc.createElement(QLatin1String("Retain")));
            if (ii->stockroom() != Stockroom::None) {
                const char *st;
                switch (ii->stockroom()) {
                case Stockroom::A: st = "A"; break;
                case Stockroom::B: st = "B"; break;
                case Stockroom::C: st = "C"; break;
                default          : st = ""; break;
                }
                item.appendChild(doc.createElement(QLatin1String("Stockroom")).appendChild(doc.createTextNode(QLatin1String(st))).parentNode());
            }
            if (!ii->reserved().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("Reserved")).appendChild(doc.createTextNode(ii->reserved())).parentNode());
            if (ii->lotId())
                item.appendChild(doc.createElement(QLatin1String("LotID")).appendChild(doc.createTextNode(c.toString(ii->lotId()))).parentNode());

            if (ii->tierQuantity(0)) {
                item.appendChild(doc.createElement(QLatin1String("TQ1")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(0)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP1")).appendChild(doc.createTextNode(c.toCurrencyString(ii->tierPrice(0), { }, 3))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ2")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(1)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP2")).appendChild(doc.createTextNode(c.toCurrencyString(ii->tierPrice(1), { }, 3))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ3")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(2)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP3")).appendChild(doc.createTextNode(c.toCurrencyString(ii->tierPrice(2), { }, 3))).parentNode());
            }

            if (ii->m_weight > 0)
                item.appendChild(doc.createElement(QLatin1String("TotalWeight")).appendChild(doc.createTextNode(c.toString(ii->weight(), 'f', 4))).parentNode());
            if (!qFuzzyCompare(ii->origPrice(), ii->price()))
                item.appendChild(doc.createElement(QLatin1String("OrigPrice")).appendChild(doc.createTextNode(c.toCurrencyString(ii->origPrice(), { }, 3))).parentNode());
            if (ii->origQuantity() != ii->quantity())
                item.appendChild(doc.createElement(QLatin1String("OrigQty")).appendChild(doc.createTextNode(c.toString(ii->origQuantity()))).parentNode());
        }

        // ### MASS UPLOAD ###
        else if (hint == XMLHint_MassUpload) {
            item.appendChild(doc.createElement(QLatin1String("ITEMID")).appendChild(doc.createTextNode(QString(ii->item()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("COLOR")).appendChild(doc.createTextNode(c.toString(ii->color()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("CATEGORY")).appendChild(doc.createTextNode(c.toString(ii->category()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ITEMTYPE")).appendChild(doc.createTextNode(QChar(ii->itemType()->id()))).parentNode());

            item.appendChild(doc.createElement(QLatin1String("QTY")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("PRICE")).appendChild(doc.createTextNode(c.toCurrencyString(ii->price(), { }, 3))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("CONDITION")).appendChild(doc.createTextNode(QLatin1String((ii->condition() == Condition::New) ? "N" : "U"))).parentNode());

            if (ii->subCondition() != SubCondition::None) {
                const char *st;
                switch (ii->subCondition()) {
                    case SubCondition::Incomplete: st = "I"; break;
                    case SubCondition::Complete  : st = "C"; break;
                    case SubCondition::Sealed    : st = "M"; break;
                    default                      : st = "?"; break;
                }
                item.appendChild(doc.createElement(QLatin1String("SUBCONDITION")).appendChild(doc.createTextNode(QLatin1String(st))).parentNode());
            }

            if (ii->bulkQuantity() != 1)
                item.appendChild(doc.createElement(QLatin1String("BULK")).appendChild(doc.createTextNode(c.toString(ii->bulkQuantity()))).parentNode());
            if (ii->sale())
                item.appendChild(doc.createElement(QLatin1String("SALE")).appendChild(doc.createTextNode(c.toString(ii->sale()))).parentNode());
            if (!ii->comments().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("DESCRIPTION")).appendChild(doc.createTextNode(ii->comments())).parentNode());
            if (!ii->remarks().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("REMARKS")).appendChild(doc.createTextNode(ii->remarks())).parentNode());
            if (ii->retain())
                item.appendChild(doc.createElement(QLatin1String("RETAIN")).appendChild(doc.createTextNode(QLatin1String("Y"))).parentNode());
            if (ii->stockroom() != Stockroom::None) {
                const char *st;
                switch (ii->stockroom()) {
                case Stockroom::A: st = "A"; break;
                case Stockroom::B: st = "B"; break;
                case Stockroom::C: st = "C"; break;
                default          : st = nullptr; break;
                }
                item.appendChild(doc.createElement(QLatin1String("STOCKROOM")).appendChild(doc.createTextNode(QLatin1String("Y"))).parentNode());
                if (st)
                    item.appendChild(doc.createElement(QLatin1String("STOCKROOMID")).appendChild(doc.createTextNode(QLatin1String(st))).parentNode());
            }
            if (!ii->reserved().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("BUYERUSERNAME")).appendChild(doc.createTextNode(ii->reserved())).parentNode());

            if (ii->tierQuantity(0)) {
                item.appendChild(doc.createElement(QLatin1String("TQ1")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(0)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP1")).appendChild(doc.createTextNode(c.toCurrencyString(ii->tierPrice(0), { }, 3))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ2")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(1)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP2")).appendChild(doc.createTextNode(c.toCurrencyString(ii->tierPrice(1), { }, 3))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ3")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(2)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP3")).appendChild(doc.createTextNode(c.toCurrencyString(ii->tierPrice(2), { }, 3))).parentNode());
            }
        }

        // ### WANTED LIST ###
        else if (hint == XMLHint_WantedList) {
            item.appendChild(doc.createElement(QLatin1String("ITEMID")).appendChild(doc.createTextNode(QString(ii->item()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ITEMTYPE")).appendChild(doc.createTextNode(QChar(ii->itemType()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("COLOR")).appendChild(doc.createTextNode(c.toString(ii->color()->id()))).parentNode());

            if (ii->quantity())
                item.appendChild(doc.createElement(QLatin1String("MINQTY")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());
            if (!qFuzzyIsNull(ii->price()))
                item.appendChild(doc.createElement(QLatin1String("MAXPRICE")).appendChild(doc.createTextNode(c.toCurrencyString(ii->price(), { }, 3))).parentNode());
            if (!ii->remarks().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("REMARKS")).appendChild(doc.createTextNode(ii->remarks())).parentNode());
            if (ii->condition() == Condition::New)
                item.appendChild(doc.createElement(QLatin1String("CONDITION")).appendChild(doc.createTextNode(QLatin1String("N"))).parentNode());
        }

        // ### INVENTORY REQUEST ###
        else if (hint == XMLHint_Inventory) {
            item.appendChild(doc.createElement(QLatin1String("ITEMID")).appendChild(doc.createTextNode(QString(ii->item()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ITEMTYPE")).appendChild(doc.createTextNode(QChar(ii->itemType()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("COLOR")).appendChild(doc.createTextNode(c.toString(ii->color()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("QTY")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());

            if (ii->status() == Status::Extra)
                item.appendChild(doc.createElement(QLatin1String("EXTRA")).appendChild(doc.createTextNode(QLatin1String("Y"))).parentNode());
        }

        // optional: additonal tags
        if (extra) {
            for (QMap <QString, QString>::iterator it = extra->begin(); it != extra->end(); ++it)
                item.appendChild(doc.createElement(it.key()).appendChild(doc.createTextNode(it.value())).parentNode());
        }
    }

    return root;
}



bool Core::parseLDrawModel(QFile &f, InvItemList &items, uint *invalid_items)
{
    QHash<QString, InvItem *> mergehash;
    QStringList recursion_detection;

    return parseLDrawModelInternal(f, QString(), items, invalid_items, mergehash, recursion_detection);
}

bool Core::parseLDrawModelInternal(QFile &f, const QString &model_name, InvItemList &items, uint *invalid_items, QHash<QString, InvItem *> &mergehash, QStringList &recursion_detection)
{
    if (recursion_detection.contains(model_name))
        return false;
    recursion_detection.append(model_name);

    QStringList searchpath;
    uint invalid = 0;
    int linecount = 0;

    bool is_mpd = false;
    int current_mpd_index = -1;
    QString current_mpd_model;
    bool is_mpd_model_found = false;


    searchpath.append(QFileInfo(f).dir().absolutePath());
    if (!m_ldraw_datadir.isEmpty()) {
        searchpath.append(m_ldraw_datadir + QLatin1String("/models"));
    }

    if (f.isOpen()) {
        QTextStream in(&f);
        QString line;

        while (!(line = in.readLine()).isNull()) {
            linecount++;

            if (!line.isEmpty() && line [0] == QLatin1Char('0')) {
                QStringList sl = line.simplified().split(' ');

                if ((sl.count() >= 2) && (sl [1] == QLatin1String("FILE"))) {
                    is_mpd = true;
                    sl.removeFirst();
                    sl.removeFirst();
                    current_mpd_model = sl.join(QLatin1String(" ")).toLower();
                    current_mpd_index++;

                    if (is_mpd_model_found)
                        break;

                    if ((current_mpd_model == model_name.toLower()) || (model_name.isEmpty() && (current_mpd_index == 0)))
                        is_mpd_model_found = true;

                    if (f.isSequential())
                        return false; // we need to seek!
                }
            }
            else if (!line.isEmpty() && line [0] == QLatin1Char('1')) {
                if (is_mpd && !is_mpd_model_found)
                    continue;

                QStringList sl = line.simplified().split(QLatin1Char(' '));

                if (sl.count() >= 15) {
                    int colid = sl [1].toInt();
                    QString partname = sl [14].toLower();
                    for (int i = 15; i < sl.count(); ++i) {
                        partname.append(QLatin1Char(' '));
                        partname.append(sl [i].toLower());
                    }

                    QString partid = partname;
                    partid.truncate(partid.lastIndexOf(QLatin1Char('.')));

                    const Color *colp = colorFromLDrawId(colid);
                    const Item *itemp = item('P', partid.toLatin1());


                    if (!itemp) {
                        bool got_subfile = false;

                        if (is_mpd) {
                            uint sub_invalid_items = 0;

                            qint64 oldpos = f.pos();
                            f.seek(0);

                            got_subfile = parseLDrawModelInternal(f, partname, items, &sub_invalid_items, mergehash, recursion_detection);

                            invalid += sub_invalid_items;
                            f.seek(oldpos);
                        }

                        if (!got_subfile) {
                            for (const auto &path : qAsConst(searchpath)) {
                                QFile subf(path + QDir::separator() + partname);

                                if (subf.open(QIODevice::ReadOnly)) {
                                    uint sub_invalid_items = 0;

                                    (void) parseLDrawModelInternal(subf, partname, items, &sub_invalid_items, mergehash, recursion_detection);

                                    invalid += sub_invalid_items;
                                    got_subfile = true;
                                    break;
                                }
                            }
                        }
                        if (got_subfile)
                            continue;
                    }

                    QString key = QString("%1@%2").arg(partid).arg(colid);

                    InvItem *ii = mergehash.value(key);

                    if (ii) {
                        ii->m_quantity++;
                    }
                    else {
                        ii = new InvItem(colp, itemp);
                        ii->m_quantity = 1;

                        if (!colp || !itemp) {
                            auto *inc = new InvItem::Incomplete;

                            if (!itemp) {
                                inc->m_item_id = partid.toLatin1();
                                inc->m_item_name = QByteArray();
                                inc->m_itemtype_id = "P";
                                inc->m_itemtype_name = "Part";
                            }
                            if (!colp) {
                                inc->m_color_name = "LDraw #" + QByteArray::number(colid);
                            }
                            ii->setIncomplete(inc);
                            invalid++;
                        }

                        items.append(ii);
                        mergehash.insert(key, ii);
                    }
                }
            }
        }
    }

    if (invalid_items)
        *invalid_items = invalid;

    recursion_detection.removeLast();

    return is_mpd ? is_mpd_model_found : true;
}


//
// Database (de)serialization
//

bool Core::readDatabase(QString *infoText, const QString &filename)
{
    QMutexLocker lock(&m_corelock);

    try {
        clear();

        stopwatch *sw = nullptr; //new stopwatch("Core::readDatabase()");

        QString info;
        if (infoText)
            infoText->clear();

        QFile f(!filename.isEmpty() ? filename : dataPath() + defaultDatabaseName());

        if (!f.open(QFile::ReadOnly))
            throw Exception(&f, "could not open database for reading");

        const char *data = reinterpret_cast<char *>(f.map(0, f.size()));

        if (!data)
            throw Exception("could not memory map the database (%1)").arg(f.fileName());

        QByteArray ba = QByteArray::fromRawData(data, int(f.size()));
        QBuffer buf(&ba);
        buf.open(QIODevice::ReadOnly);
        ChunkReader cr(&buf, QDataStream::LittleEndian);
        QDataStream &ds = cr.dataStream();

        if (!cr.startChunk() || cr.chunkId() != ChunkId('B','S','D','B'))
            throw Exception("invalid database format - wrong magic (%1)").arg(f.fileName());

        if (cr.chunkVersion() != int(DatabaseVersion::Latest)) {
            throw Exception("invalid database version: expected %1, but got %2")
                .arg(int(DatabaseVersion::Latest)).arg(cr.chunkVersion());
        }

        bool gotColors = false, gotCategories = false, gotItemTypes = false, gotItems = false;
        bool gotChangeLog = false;

        auto check = [&ds, &f]() {
            if (ds.status() != QDataStream::Ok)
                throw Exception("failed to read from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        while (cr.startChunk()) {
            switch (cr.chunkId() | quint64(cr.chunkVersion()) << 32) {
            case ChunkId('I','N','F','O') | 1ULL << 32: {
                ds >> info;
                break;
            }
            case ChunkId('C','O','L',' ') | 1ULL << 32: {
                quint32 colc = 0;
                ds >> colc;
                check();

                for (quint32 i = colc; i; i--) {
                    auto *col = readColorFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_colors.insert(col->id(), col);
                }
                gotColors = true;
                break;
            }
            case ChunkId('C','A','T',' ') | 1ULL << 32: {
                quint32 catc = 0;
                ds >> catc;
                check();

                for (quint32 i = catc; i; i--) {
                    auto *cat = readCategoryFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_categories.insert(cat->id(), cat);
                }
                gotCategories = true;
                break;
            }
            case ChunkId('T','Y','P','E') | 1ULL << 32: {
                quint32 ittc = 0;
                ds >> ittc;
                check();

                for (quint32 i = ittc; i; i--) {
                    auto *itt = readItemTypeFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_item_types.insert(itt->id(), itt);
                }
                gotItemTypes = true;
                break;
            }
            case ChunkId('I','T','E','M') | 1ULL << 32: {
                quint32 itc = 0;
                ds >> itc;
                check();

                m_items.reserve(int(itc));
                for (quint32 i = itc; i; i--) {
                    auto *item = readItemFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_items.append(item);
                }
                gotItems = true;
                break;
            }
            case ChunkId('C','H','G','L') | 1ULL << 32: {
                quint32 clc = 0;
                ds >> clc;
                check();

                m_changelog.reserve(int(clc));
                for (quint32 i = clc; i; i--) {
                    QByteArray entry;
                    ds >> entry;
                    check();
                    m_changelog.append(entry);
                }
                gotChangeLog = true;
                break;
            }
            default: {
                cr.skipChunk();
                check();
                break;
            }
            }
            if (!cr.endChunk()) {
                throw Exception("missed the end of a chunk when reading from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
            }
        }
        if (!cr.endChunk()) {
            throw Exception("missed the end of the root chunk when reading from database (%1) at position %2")
                .arg(f.fileName()).arg(f.pos());
        }

        delete sw;

        if (!gotColors || !gotCategories || !gotItemTypes || !gotItems || !gotChangeLog) {
            throw Exception("not all required data chunks were found in the database (%1)")
                .arg(f.fileName());
        }

        qDebug("Color: %8u  (%11d bytes)", m_colors.count(),     m_colors.count()     * int(sizeof(Color)    + 20));
        qDebug("Types: %8u  (%11d bytes)", m_item_types.count(), m_item_types.count() * int(sizeof(ItemType) + 20));
        qDebug("Cats : %8u  (%11d bytes)", m_categories.count(), m_categories.count() * int(sizeof(Category) + 20));
        qDebug("Items: %8u  (%11d bytes)", m_items.count(),      m_items.count()      * int(sizeof(Item)     + 20));
        if (!info.isEmpty())
            qDebug() << "Info :" << info;
        if (infoText)
            *infoText = info;
        return true;

    } catch (const Exception &e) {
        qWarning() << "Error reading database:" << e.what();
        clear();
        return false;
    }
}


bool Core::writeDatabase(const QString &filename, DatabaseVersion version,
                         const QString &infoText) const
{
    if (version <= DatabaseVersion::Invalid)
        return false; // too old

    QMutexLocker lock(&m_corelock);

    QString fn(!filename.isEmpty() ? filename : dataPath() + defaultDatabaseName(version));

    try {
        QFile f(fn + QLatin1String(".new"));
        if (!f.open(QIODevice::WriteOnly))
            throw Exception(&f, "could not open database for writing");

        ChunkWriter cw(&f, QDataStream::LittleEndian);
        QDataStream &ds = cw.dataStream();

        auto check = [&ds, &f](bool ok) {
            if (!ok || (ds.status() != QDataStream::Ok))
                throw Exception("failed to write to database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        check(cw.startChunk(ChunkId('B','S','D','B'), int(version)));

        if (!infoText.isEmpty()) {
            check(cw.startChunk(ChunkId('I','N','F','O'), 1));
            ds << infoText;
            check(cw.endChunk());
        }

        check(cw.startChunk(ChunkId('C','O','L',' '), 1));
        ds << quint32(m_colors.count());
        for (const Color *col : m_colors)
            writeColorToDatabase(col, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','A','T',' '), 1));
        ds << quint32(m_categories.count());
        for (const Category *cat : m_categories)
            writeCategoryToDatabase(cat, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('T','Y','P','E'), 1));
        ds << quint32(m_item_types.count());
        for (const ItemType *itt : m_item_types)
            writeItemTypeToDatabase(itt, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('I','T','E','M'), 1));
        ds << quint32(m_items.count());
        for (const Item *item : m_items)
            writeItemToDatabase(item, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','H','G','L'), 1));
        ds << quint32(m_changelog.count());
        for (const QByteArray &cl : m_changelog)
            ds << cl;
        check(cw.endChunk());

        check(cw.endChunk()); // BSDB root chunk

        f.close();
        QString err = Utility::safeRename(filename);
        if (!err.isNull())
            throw Exception(err);

        return true;

    } catch (const Exception &e) {
        qWarning() << "Error writing database:" << e.what();
        QFile::remove(fn + ".new");
        return false;
    }
}


Color *Core::readColorFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<Color> col(new Color);

    quint32 flags;
    float popularity;
    QByteArray name;

    dataStream >> col->m_id >> name >> col->m_ldraw_id >> col->m_color >> flags >> popularity
            >> col->m_year_from >> col->m_year_to;

    col->m_name = QString::fromUtf8(name);
    col->m_type = static_cast<Color::Type>(flags);
    col->m_popularity = qreal(popularity);

    return col.take();
}

void Core::writeColorToDatabase(const Color *col, QDataStream &dataStream, DatabaseVersion)
{
    dataStream << col->m_id << col->m_name.toUtf8() << col->m_ldraw_id << col->m_color
               << quint32(col->m_type) << float(col->m_popularity)
               << col->m_year_from << col->m_year_to;
}


Category *Core::readCategoryFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<Category> cat(new Category);

    QByteArray name;
    quint32 id;
    dataStream >> id >> name;
    cat->m_id = id;
    cat->m_name = QString::fromUtf8(name);

    return cat.take();
}

void Core::writeCategoryToDatabase(const Category *cat, QDataStream &dataStream, DatabaseVersion)
{
    dataStream << quint32(cat->m_id) << cat->m_name.toUtf8();
}


ItemType *Core::readItemTypeFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<ItemType> itt(new ItemType);

    quint8 flags = 0;
    qint32 catcount = 0;
    qint8 id = 0, picid = 0;
    QByteArray name;
    dataStream >> id >> picid >> name >> flags >> catcount;

    itt->m_name = QString::fromUtf8(name);
    itt->m_id = id;
    itt->m_picture_id = id;

    itt->m_categories.resize(catcount);

    for (int i = 0; i < catcount; i++) {
        quint32 catid = 0;
        dataStream >> catid;
        itt->m_categories[i] = BrickLink::core()->category(catid);
    }

    itt->m_has_inventories   = flags & 0x01;
    itt->m_has_colors        = flags & 0x02;
    itt->m_has_weight        = flags & 0x04;
    itt->m_has_year          = flags & 0x08;
    itt->m_has_subconditions = (id == 'S'); //flags & 0x10;

    return itt.take();
}

void Core::writeItemTypeToDatabase(const ItemType *itt, QDataStream &dataStream, DatabaseVersion)
{
    quint8 flags = 0;
    flags |= (itt->m_has_inventories   ? 0x01 : 0);
    flags |= (itt->m_has_colors        ? 0x02 : 0);
    flags |= (itt->m_has_weight        ? 0x04 : 0);
    flags |= (itt->m_has_year          ? 0x08 : 0);
    flags |= (itt->m_has_subconditions ? 0x10 : 0);

    dataStream << qint8(itt->m_id) << qint8(itt->m_picture_id) << itt->m_name.toUtf8() << flags;

    dataStream << qint32(itt->m_categories.size());
    for (const BrickLink::Category *cat : itt->m_categories)
        dataStream << quint32(cat->id());
}


Item *Core::readItemFromDatabase(QDataStream &dataStream, DatabaseVersion v)
{
    QScopedPointer<Item> item(new Item);

    qint8 ittid = 0;
    quint32 catcount = 0;
    QByteArray id;
    QByteArray name;

    dataStream >> id >> name >> ittid >> catcount;
    item->m_id = QString::fromUtf8(id);
    item->m_name = QString::fromUtf8(name);
    item->m_item_type = BrickLink::core()->itemType(ittid);

    item->m_categories.resize(int(catcount));

    for (int i = 0; i < int(catcount); i++) {
        quint32 catid = 0;
        dataStream >> catid;
        item->m_categories[i] = BrickLink::core()->category(catid);
    }

    quint32 colorid = 0;
    quint32 index = 0, year = 0;
    qint64 invupd = 0;
    dataStream >> colorid >> invupd >> item->m_weight >> index >> year;
    item->m_color = BrickLink::core()->color(colorid == quint32(-1) ? 0 : colorid);
    item->m_index = index;
    item->m_year = year;
    item->m_last_inv_update = invupd;

    quint32 appears = 0, appears_size = 0;
    dataStream >> appears;
    if (appears)
        dataStream >> appears_size;

    if (appears && appears_size) {
        auto *ptr = new quint32 [appears_size];
        item->m_appears_in = ptr;

        *ptr++ = appears;
        *ptr++ = appears_size;

        for (quint32 i = 2; i < appears_size; i++)
            dataStream >> *ptr++;
    }
    else
        item->m_appears_in = nullptr;

    quint32 consists = 0;
    dataStream >> consists;

    if (consists) {
        auto *ptr = new quint64 [consists + 1];
        item->m_consists_of = ptr;

        *ptr++ = consists;

        for (quint32 i = 0; i < consists; i++)
            dataStream >> *ptr++;
    }
    else
        item->m_consists_of = nullptr;

    if (v >= DatabaseVersion::Version_2) {
        quint32 known_colors_count;
        dataStream >> known_colors_count;
        item->m_known_colors.resize(int(known_colors_count));

        for (int i = 0; i < int(known_colors_count); i++) {
            quint32 colid = 0;
            dataStream >> colid;
            item->m_known_colors[i] = colid;
        }
    }

    return item.take();
}

void Core::writeItemToDatabase(const Item *item, QDataStream &dataStream, DatabaseVersion v)
{
    dataStream << item->m_id.toUtf8() << item->m_name.toUtf8() << qint8(item->m_item_type->id());

    dataStream << quint32(item->m_categories.size());
    for (const BrickLink::Category *cat : item->m_categories)
        dataStream << quint32(cat->id());

    quint32 colorid = item->m_color ? item->m_color->id() : quint32(-1);
    dataStream << colorid << qint64(item->m_last_inv_update) << item->m_weight
       << quint32(item->m_index) << quint32(item->m_year);

    if (item->m_appears_in && item->m_appears_in [0] && item->m_appears_in [1]) {
        quint32 *ptr = item->m_appears_in;

        for (quint32 i = 0; i < item->m_appears_in [1]; i++)
            dataStream << *ptr++;
    }
    else
        dataStream << quint32(0);

    if (item->m_consists_of && item->m_consists_of [0]) {
        dataStream << quint32(item->m_consists_of [0]);

        quint64 *ptr = item->m_consists_of + 1;

        for (quint32 i = 0; i < quint32(item->m_consists_of [0]); i++)
            dataStream << *ptr++;
    }
    else
        dataStream << quint32(0);

    if (v >= DatabaseVersion::Version_2) {
        dataStream << quint32(item->m_known_colors.size());
        for (auto cid : qAsConst(item->m_known_colors))
            dataStream << quint32(cid);
    }
}


int Core::applyChangeLogToItems(InvItemList &bllist)
{
    int count = 0;

    for (InvItem *blitem : qAsConst(bllist)) {
        const InvItem::Incomplete *incpl = blitem->isIncomplete();
        if (incpl) {
            const Item *fixed_item = blitem->item();
            const Color *fixed_color = blitem->color();

            QString itemtypeid = incpl->m_itemtype_id;
            QString itemid     = incpl->m_item_id;
            QString colorid    = incpl->m_color_name;

            if (itemtypeid.isEmpty() && !incpl->m_itemtype_name.isEmpty())
                itemtypeid = incpl->m_itemtype_name.at(0).toUpper();

            for (int i = m_changelog.count() - 1; i >= 0 && !(fixed_color && fixed_item); --i) {
                const ChangeLogEntry &cl = ChangeLogEntry(m_changelog.at(i));

                if (!fixed_item) {
                    if ((cl.type() == ChangeLogEntry::ItemId) ||
                        (cl.type() == ChangeLogEntry::ItemMerge) ||
                        (cl.type() == ChangeLogEntry::ItemType)) {
                        if ((itemtypeid == QLatin1String(cl.from(0))) &&
                            (itemid == QLatin1String(cl.from(1)))) {
                            itemtypeid = QLatin1String(cl.to(0));
                            itemid = QLatin1String(cl.to(1));

                            if (itemtypeid.length() == 1 && !itemid.isEmpty())
                                fixed_item = core()->item(itemtypeid[0].toLatin1(), itemid.toLatin1().constData());
                        }
                    }
                }
                if (!fixed_color) {
                    if (cl.type() == ChangeLogEntry::ColorMerge) {
                        if (colorid == QLatin1String(cl.from(0))) {
                            colorid = QLatin1String(cl.to(0));

                            bool ok;
                            uint cid = colorid.toUInt(&ok);
                            if (ok)
                                fixed_color = core()->color(cid);
                        }
                    }
                }
            }

            if (fixed_item && !blitem->item())
                blitem->setItem(fixed_item);
            if (fixed_color && !blitem->color())
                blitem->setColor(fixed_color);

            if (fixed_item && fixed_color) {
                blitem->setIncomplete(nullptr);
                count++;
            }
        }
    }
    return count;
}

qreal Core::itemImageScaleFactor() const
{
    return m_item_image_scale_factor;
}

void Core::setItemImageScaleFactor(qreal f)
{
    if (!qFuzzyCompare(f, m_item_image_scale_factor)) {
        m_item_image_scale_factor = f;
        m_noimages.clear();
        m_colimages.clear();
        emit itemImageScaleFactorChanged(f);
    }
}

bool Core::isLDrawEnabled() const
{
    return !m_ldraw_datadir.isEmpty();
}

QString Core::ldrawDataPath() const
{
    return m_ldraw_datadir;
}

void Core::setLDrawDataPath(const QString &ldrawDataPath)
{
    m_ldraw_datadir = ldrawDataPath;
}

const QVector<const Color *> Item::knownColors() const
{
    QVector<const Color *> result;
    for (auto colId : m_known_colors) {
        auto color = core()->color(colId);
        if (color)
            result << color;
    }
    return result;
}

} // namespace BrickLink

#include "moc_bricklink.cpp"

