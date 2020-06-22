/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QToolTip>
#include <QApplication>
#include <QLabel>
#include <QDesktopWidget>

#include "config.h"
#include "utility.h"
#include "stopwatch.h"
#include "bricklink.h"
#include "chunkreader.h"
#include "chunkwriter.h"
#include "qtemporaryresource.h"


QUrl BrickLink::Core::url(UrlList u, const void *opt, const void *opt2)
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
    default:
        break;
    }
    return url;
}


const QImage BrickLink::Core::noImage(const QSize &s) const
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

        QColor coltable [] = {
            QColor(0x00, 0x00, 0x00),
            QColor(0x3f, 0x3f, 0x3f),
            QColor(0xff, 0x7f, 0x7f)
        };

        for (const auto & i : coltable) {
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


const QImage BrickLink::Core::colorImage(const Color *col, int w, int h) const
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
            brush = QBrush(Utility::contrastColor(c, 0.25f), Qt::Dense6Pattern);
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

                    const BrickLink::Color *basec = colorFromName(basename);
                    const BrickLink::Color *speckc = colorFromName(speckname);

                    if (basec)
                        c = basec->color();
                    if (speckc)
                        c2 = speckc->color();
                }
            }
            if (c.isValid()) {
                if (!c2.isValid()) // fake
                    c2 = Utility::contrastColor(c, 0.20f);
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
        QDir::current().mkdir(p);
        fi.refresh();
    }
    return (fi.exists() && fi.isDir() && fi.isReadable() && fi.isWritable());
}

} // namespace

QString BrickLink::Core::dataPath() const
{
    return m_datadir;
}

QString BrickLink::Core::dataPath(const ItemType *item_type) const
{
    QString p = dataPath();
    p += item_type->id();
    p += QDir::separator();

    if (!check_and_create_path(p))
        return QString();

    return p;
}

QString BrickLink::Core::dataPath(const Item *item) const
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

QString BrickLink::Core::dataPath(const Item *item, const Color *color) const
{
    QString p = dataPath(item);
    p += QString::number(color->id());
    p += QDir::separator();

    if (!check_and_create_path(p))
        return QString();

    return p;
}



BrickLink::Core *BrickLink::Core::s_inst = nullptr;

BrickLink::Core *BrickLink::Core::create(const QString &datadir, QString *errstring)
{
    qDebug() << "Loading database from" << datadir;

    if (!s_inst) {
        s_inst = new Core(datadir);

        QString test = s_inst->dataPath();
        if (test.isNull() || !check_and_create_path(test)) {
            delete s_inst;
            s_inst = nullptr;

            if (errstring)
                *errstring = tr("Data directory \'%1\' is not both read- and writable.").arg(datadir);
        }
    }
    return s_inst;
}

BrickLink::Core::Core(const QString &datadir)
    : m_datadir(datadir), m_c_locale(QLocale::c()), m_corelock(QMutex::Recursive),
      m_transfer(nullptr), m_pg_update_iv(0), m_pic_update_iv(0),
      m_pic_diskload(QThread::idealThreadCount() * 3)
{
    if (m_datadir.isEmpty())
        m_datadir = QLatin1String("./");

    m_datadir = QDir::cleanPath(datadir);

    if (m_datadir.right(1) != QDir::separator())
        m_datadir += QDir::separator();

    m_online = true;

    // cache size is physical memory * 0.25, at least 64MB, at most 256MB
    quint64 cachemem = qBound(64ULL *1024*1024, Utility::physicalMemory() / 4, 256ULL *1024*1024);
    //quint64 cachemem = 1024*1024; // DEBUG

    QPixmapCache::setCacheLimit(20 * 1024);     // 80 x 60 x 32 (w x h x bpp) == 20kB ->room for ~1000 pixmaps
    m_pg_cache.setMaxCost(500);          // each priceguide has a cost of 1
    m_pic_cache.setMaxCost(cachemem);    // each pic has a cost of (w*h*d/8 + 1024)

    connect(&m_pic_diskload, &ThreadPool::finished, this, &Core::pictureLoaded);
}

BrickLink::Core::~Core()
{
    cancelPictureTransfers();
    cancelPriceGuideTransfers();
    delete m_transfer;
    s_inst = nullptr;
}

void BrickLink::Core::setTransfer(Transfer *trans)
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

Transfer *BrickLink::Core::transfer() const
{
    return m_transfer;
}

void BrickLink::Core::setUpdateIntervals(const QMap<QByteArray, int> &intervals)
{
    m_pic_update_iv = intervals["Picture"];
    m_pg_update_iv = intervals["PriceGuide"];
}

bool BrickLink::Core::updateNeeded(bool valid, const QDateTime &last, int iv)
{
    return (iv > 0) && (!valid || (last.secsTo(QDateTime::currentDateTime()) > iv));
}

void BrickLink::Core::setOnlineStatus(bool on)
{
    m_online = on;
}

bool BrickLink::Core::onlineStatus() const
{
    return m_online;
}


const QHash<int, const BrickLink::Color *> &BrickLink::Core::colors() const
{
    return m_colors;
}

const QHash<int, const BrickLink::Category *> &BrickLink::Core::categories() const
{
    return m_categories;
}

const QHash<int, const BrickLink::ItemType *> &BrickLink::Core::itemTypes() const
{
    return m_item_types;
}

const QVector<const BrickLink::Item *> &BrickLink::Core::items() const
{
    return m_items;
}

const BrickLink::Category *BrickLink::Core::category(uint id) const
{
    return m_categories.value(id);
}

const BrickLink::Color *BrickLink::Core::color(uint id) const
{
    return m_colors.value(id);
}

const BrickLink::Color *BrickLink::Core::colorFromName(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;

    for (const Color *col : m_colors) {
        if (!col->name().compare(name, Qt::CaseInsensitive))
            return col;
    }
    return nullptr;
}


const BrickLink::Color *BrickLink::Core::colorFromLDrawId(int ldraw_id) const
{
    for (const Color *col : m_colors) {
        if (col->ldrawId() == ldraw_id)
            return col;
    }
    return nullptr;
}


const BrickLink::ItemType *BrickLink::Core::itemType(char id) const
{
    return m_item_types.value(id);
}

const BrickLink::Item *BrickLink::Core::item(char tid, const QString &id) const
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

void BrickLink::Core::cancelPictureTransfers()
{
    QMutexLocker lock(&m_corelock);

    m_pic_diskload.abortAllJobs();
    m_transfer->abortAllJobs();
}

void BrickLink::Core::cancelPriceGuideTransfers()
{
    QMutexLocker lock(&m_corelock);

    m_transfer->abortAllJobs();
}

QString BrickLink::Core::defaultDatabaseName(DatabaseVersion version) const
{
    return QString("database-v%1").arg(version);
}

bool BrickLink::Core::readDatabase(QString *infoText, const QString &fname)
{
    QMutexLocker lock(&m_corelock);

    cancelPictureTransfers();
    cancelPriceGuideTransfers();

    m_pg_cache.clear();
    m_pic_cache.clear();
    QPixmapCache::clear();

    qDeleteAll(m_colors);
    qDeleteAll(m_item_types);
    qDeleteAll(m_categories);
    qDeleteAll(m_items);
    qDeleteAll(m_changelog);

    m_colors.clear();
    m_item_types.clear();
    m_categories.clear();
    m_items.clear();

    bool result = false;
    stopwatch *sw = nullptr; //new stopwatch("BrickLink::Core::readDatabase()");

    QString info;

    QFile f;
    if (!fname.isEmpty())
        f.setFileName(fname);
    else if (QFile::exists(dataPath() + defaultDatabaseName()))
        f.setFileName(dataPath() + defaultDatabaseName());
    else if (QFile::exists(dataPath() + defaultDatabaseName(BrickStore_1_1)))
        f.setFileName(dataPath() + defaultDatabaseName(BrickStore_1_1));

    if (f.open(QFile::ReadOnly)) {
        const char *data = reinterpret_cast<char *>(f.map(0, f.size()));

        if (data) {
            QByteArray ba = QByteArray::fromRawData(data, f.size());

            if (ba.size() >= 4 && data[0] == 'B' && data[1] == 'S' && data[2] == 'D' && data[3] == 'B') {
                QBuffer buf(&ba);
                buf.open(QIODevice::ReadOnly);

                ChunkReader cr(&buf, QDataStream::LittleEndian);
                QDataStream &ds = cr.dataStream();

                if (cr.startChunk() && cr.chunkId() == ChunkId('B','S','D','B') && cr.chunkVersion() == 1) {
                    while (cr.startChunk()) {
                        switch (cr.chunkId() | quint64(cr.chunkVersion()) << 32) {
                            case ChunkId('I','N','F','O') | 1ULL << 32: {
                                ds >> info;
                                break;
                            }
                            case ChunkId('C','O','L',' ') | 1ULL << 32: {
                                quint32 colc = 0;
                                ds >> colc;

                                for (quint32 i = colc; i; i--) {
                                    auto *col = new Color();
                                    ds >> col;
                                    m_colors.insert(col->id(), col);
                                }
                                break;
                            }
                            case ChunkId('C','A','T',' ') | 1ULL << 32: {
                                quint32 catc = 0;
                                ds >> catc;

                                for (quint32 i = catc; i; i--) {
                                    auto *cat = new Category();
                                    ds >> cat;
                                    m_categories.insert(cat->id(), cat);
                                }
                                break;
                            }
                            case ChunkId('T','Y','P','E') | 1ULL << 32: {
                                quint32 ittc = 0;
                                ds >> ittc;

                                for (quint32 i = ittc; i; i--) {
                                    auto *itt = new ItemType();
                                    ds >> itt;
                                    m_item_types.insert(itt->id(), itt);
                                }
                                break;
                            }
                            case ChunkId('I','T','E','M') | 1ULL << 32: {
                                quint32 itc = 0;
                                ds >> itc;

                                m_items.reserve(itc);
                                for (quint32 i = itc; i; i--) {
                                    Item *item = new Item();
                                    ds >> item;
                                    m_items.append(item);
                                }
                                break;
                            }
                            case ChunkId('C','H','G','L') | 1ULL << 32: {
                                quint32 clc = 0;
                                ds >> clc;
                                int xxx= 0;

                                m_changelog.reserve(clc);
                                for (quint32 i = clc; i; i--) {
                                    char *entry;
                                    ds >> entry;
                                    m_changelog.append(entry);
                                    xxx += (4+qstrlen(entry) + 1);
                                }
                                break;
                            }
                            default: {
                                if (!cr.skipChunk())
                                    goto out;
                                break;
                            }
                        }
                        if (!cr.endChunk())
                            goto out;
                    }
                    if (!cr.endChunk())
                        goto out;

                    result = true;
                }
            } else {
                qWarning("readDatabase(): Unknown database format!");
            }
        }
    }

out:
    if (result) {
        delete sw;

        qDebug("Color: %8u  (%11d bytes)", m_colors.count(),     int(m_colors.count()     * (sizeof(Color)    + 20)));
        qDebug("Types: %8u  (%11d bytes)", m_item_types.count(), int(m_item_types.count() * (sizeof(ItemType) + 20)));
        qDebug("Cats : %8u  (%11d bytes)", m_categories.count(), int(m_categories.count() * (sizeof(Category) + 20)));
        qDebug("Items: %8u  (%11d bytes)", m_items.count(),      int(m_items.count()      * (sizeof(Item)     + 20)));
        if (!info.isEmpty())
            qDebug() << "Info :" << info;
    }
    else {
        m_colors.clear();
        m_item_types.clear();
        m_categories.clear();
        m_items.clear();

        qWarning("Error reading databases!");
    }
    if (infoText)
        *infoText = info;
    return result;
}


BrickLink::Core::ParseItemListXMLResult BrickLink::Core::parseItemListXML(const QDomElement &root, ItemListXMLHint hint)
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
                    ii->setCondition(val == QLatin1String("N") ? New : Used);
                else if (tag == QLatin1String("SUBCONDITION")) {
                    ii->setSubCondition(val == QLatin1String("C") ? Complete : \
                                        val == QLatin1String("I") ? Incomplete : \
                                        val == QLatin1String("M") ? MISB : None);
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
                else if (tag == QLatin1String("STOCKROOM"))
                    ii->setStockroom(val == QLatin1String("Y"));
                else if (tag == QLatin1String("BUYERUSERNAME"))
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
                    ii->setStatus(Extra);
                else if (tag == QLatin1String("COUNTERPART"))
                     ii->setCounterPart((val == QLatin1String("Y")));
                else if (tag == QLatin1String("ALTERNATE"))
                     ii->setAlternate((val == QLatin1String("Y")));
                else if (tag == QLatin1String("MATCHID"))
                     ii->setAlternateId(c.toInt(val));
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
                    ii->setCondition(val == QLatin1String("N") ? New : Used);
                else if (tag == QLatin1String("SubCondition")) {
                    ii->setSubCondition(val == QLatin1String("C") ? Complete : \
                                        val == QLatin1String("I") ? Incomplete : \
                                        val == QLatin1String("M") ? MISB : None);
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
                    Status st = Include;

                    if (val == QLatin1String("X"))
                        st = Exclude;
                    else if (val == QLatin1String("I"))
                        st = Include;
                    else if (val == QLatin1String("E"))
                        st = Extra;
                    else if (val == QLatin1String("?"))
                        st = Unknown;

                    ii->setStatus(st);
                }
                else if (tag == QLatin1String("LotID"))
                    ii->setLotId(c.toUInt(val));
                else if (tag == QLatin1String("Retain"))
                    ii->setRetain(true);
                else if (tag == QLatin1String("Stockroom"))
                    ii->setStockroom(true);
                else if (tag == QLatin1String("Reserved"))
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

            ii->setColor(color(colorid.toInt()));

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
                inc->m_itemtype_name = (itemtypename.isEmpty() ? itemtypeid : itemtypename).toLatin1();
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



QDomElement BrickLink::Core::createItemListXML(QDomDocument doc, ItemListXMLHint hint, const InvItemList &items, const QString &currencyCode, QMap <QString, QString> *extra)
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

        if ((ii->status() == Exclude) && (hint != XMLHint_BrickStore))
            continue;

        if (hint == XMLHint_MassUpdate) {
            bool diffs = ((ii->quantity() != ii->origQuantity()) || (ii->price() != ii->origPrice()));

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

            if (pdiff != 0)
                item.appendChild(doc.createElement(QLatin1String("PRICE")).appendChild(doc.createTextNode(c.toString(ii->price()))).parentNode());
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
                    case Unknown: st = "?"; break;
                    case Extra  : st = "E"; break;
                    case Exclude: st = "X"; break;
                    case Include:
                    default     : st = "I"; break;
                }
                item.appendChild(doc.createElement(QLatin1String("Status")).appendChild(doc.createTextNode(QLatin1String(st))).parentNode());
            }

            item.appendChild(doc.createElement(QLatin1String("Qty")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("Price")).appendChild(doc.createTextNode(c.toString(ii->price()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("Condition")).appendChild(doc.createTextNode(QLatin1String((ii->condition() == New) ? "N" : "U"))).parentNode());

            if (ii->subCondition() != None) {
                const char *st;
                switch (ii->subCondition()) {
                    case Incomplete: st = "I"; break;
                    case Complete  : st = "C"; break;
                    case MISB      : st = "M"; break;
                    default        : st = "?"; break;
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
            if (ii->stockroom())
                item.appendChild(doc.createElement(QLatin1String("Stockroom")));
            if (!ii->reserved().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("Reserved")).appendChild(doc.createTextNode(ii->reserved())).parentNode());
            if (ii->lotId())
                item.appendChild(doc.createElement(QLatin1String("LotID")).appendChild(doc.createTextNode(c.toString(ii->lotId()))).parentNode());

            if (ii->tierQuantity(0)) {
                item.appendChild(doc.createElement(QLatin1String("TQ1")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(0)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP1")).appendChild(doc.createTextNode(c.toString(ii->tierPrice(0)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ2")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(1)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP2")).appendChild(doc.createTextNode(c.toString(ii->tierPrice(1)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ3")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(2)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP3")).appendChild(doc.createTextNode(c.toString(ii->tierPrice(2)))).parentNode());
            }

            if (ii->m_weight > 0)
                item.appendChild(doc.createElement(QLatin1String("TotalWeight")).appendChild(doc.createTextNode(c.toString(ii->weight(), 'f', 4))).parentNode());
            if (ii->origPrice() != ii->price())
                item.appendChild(doc.createElement(QLatin1String("OrigPrice")).appendChild(doc.createTextNode(c.toString(ii->origPrice()))).parentNode());
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
            item.appendChild(doc.createElement(QLatin1String("PRICE")).appendChild(doc.createTextNode(c.toString(ii->price(), 'f', 3))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("CONDITION")).appendChild(doc.createTextNode(QLatin1String((ii->condition() == New) ? "N" : "U"))).parentNode());

            if (ii->subCondition() != None) {
                const char *st;
                switch (ii->subCondition()) {
                    case Incomplete: st = "I"; break;
                    case Complete  : st = "C"; break;
                    case MISB      : st = "M"; break;
                    default        : st = "?"; break;
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
            if (ii->stockroom())
                item.appendChild(doc.createElement(QLatin1String("STOCKROOM")).appendChild(doc.createTextNode(QLatin1String("Y"))).parentNode());
            if (!ii->reserved().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("BUYERUSERNAME")).appendChild(doc.createTextNode(ii->reserved())).parentNode());

            if (ii->tierQuantity(0)) {
                item.appendChild(doc.createElement(QLatin1String("TQ1")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(0)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP1")).appendChild(doc.createTextNode(c.toString(ii->tierPrice(0)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ2")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(1)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP2")).appendChild(doc.createTextNode(c.toString(ii->tierPrice(1)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TQ3")).appendChild(doc.createTextNode(c.toString(ii->tierQuantity(2)))).parentNode());
                item.appendChild(doc.createElement(QLatin1String("TP3")).appendChild(doc.createTextNode(c.toString(ii->tierPrice(2)))).parentNode());
            }
        }

        // ### WANTED LIST ###
        else if (hint == XMLHint_WantedList) {
            item.appendChild(doc.createElement(QLatin1String("ITEMID")).appendChild(doc.createTextNode(QString(ii->item()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ITEMTYPE")).appendChild(doc.createTextNode(QChar(ii->itemType()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("COLOR")).appendChild(doc.createTextNode(c.toString(ii->color()->id()))).parentNode());

            if (ii->quantity())
                item.appendChild(doc.createElement(QLatin1String("MINQTY")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());
            if (ii->price() != 0)
                item.appendChild(doc.createElement(QLatin1String("MAXPRICE")).appendChild(doc.createTextNode(c.toString(ii->price()))).parentNode());
            if (!ii->remarks().isEmpty())
                item.appendChild(doc.createElement(QLatin1String("REMARKS")).appendChild(doc.createTextNode(ii->remarks())).parentNode());
            if (ii->condition() == New)
                item.appendChild(doc.createElement(QLatin1String("CONDITION")).appendChild(doc.createTextNode(QLatin1String("N"))).parentNode());
        }

        // ### INVENTORY REQUEST ###
        else if (hint == XMLHint_Inventory) {
            item.appendChild(doc.createElement(QLatin1String("ITEMID")).appendChild(doc.createTextNode(QString(ii->item()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("ITEMTYPE")).appendChild(doc.createTextNode(QChar(ii->itemType()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("COLOR")).appendChild(doc.createTextNode(c.toString(ii->color()->id()))).parentNode());
            item.appendChild(doc.createElement(QLatin1String("QTY")).appendChild(doc.createTextNode(c.toString(ii->quantity()))).parentNode());

            if (ii->status() == Extra)
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



bool BrickLink::Core::parseLDrawModel(QFile &f, InvItemList &items, uint *invalid_items)
{
    QHash<QString, InvItem *> mergehash;
    QStringList recursion_detection;

    return parseLDrawModelInternal(f, QString(), items, invalid_items, mergehash, recursion_detection);
}

bool BrickLink::Core::parseLDrawModelInternal(QFile &f, const QString &model_name, InvItemList &items, uint *invalid_items, QHash<QString, InvItem *> &mergehash, QStringList &recursion_detection)
{
    if (recursion_detection.contains(model_name))
        return false;
    recursion_detection.append(model_name);

    QStringList searchpath;
    QString ldrawdir = Config::inst()->lDrawDir();
    uint invalid = 0;
    int linecount = 0;

    bool is_mpd = false;
    int current_mpd_index = -1;
    QString current_mpd_model;
    bool is_mpd_model_found = false;


    searchpath.append(QFileInfo(f).dir().absolutePath());
    if (!ldrawdir.isEmpty()) {
        searchpath.append(ldrawdir + QLatin1String("/models"));
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



/*
 * Support routines to rebuild the DB from txt files
 */
void BrickLink::Core::setDatabase_ConsistsOf(const QHash<const Item *, InvItemList> &hash)
{
    QMutexLocker lock(&m_corelock);

    for (QHash<const Item *, InvItemList>::const_iterator it = hash.begin(); it != hash.end(); ++it)
        it.key()->setConsistsOf(it.value());
}

void BrickLink::Core::setDatabase_AppearsIn(const QHash<const Item *, AppearsIn> &hash)
{
    QMutexLocker lock(&m_corelock);

    for (QHash<const Item *, AppearsIn>::const_iterator it = hash.begin(); it != hash.end(); ++it)
        it.key()->setAppearsIn(it.value());
}

void BrickLink::Core::setDatabase_Basics(const QHash<int, const Color *> &colors,
        const QHash<int, const Category *> &categories,
        const QHash<int, const ItemType *> &item_types,
        const QVector<const Item *> &items)
{
    QMutexLocker lock(&m_corelock);

    cancelPictureTransfers();
    cancelPriceGuideTransfers();

    m_pg_cache.clear();
    m_pic_cache.clear();
    QPixmapCache::clear();

    m_colors.clear();
    m_item_types.clear();
    m_categories.clear();
    m_items.clear();

    m_colors     = colors;
    m_item_types = item_types;
    m_categories = categories;
    m_items      = items;
}

void BrickLink::Core::setDatabase_ChangeLog(const QVector<const char *> &changelog)
{
    QMutexLocker lock(&m_corelock);
    m_changelog = changelog;
}

bool BrickLink::Core::writeDatabase(const QString &filename, DatabaseVersion version,
                                    const QString &infoText) const
{
    if (filename.isEmpty() || (version != BrickStore_2_0))
        return false;

    QMutexLocker lock(&m_corelock);

    QFile f(filename + QLatin1String(".new"));
    if (f.open(QIODevice::WriteOnly)) {
        if (version == BrickStore_2_0) {
            ChunkWriter cw(&f, QDataStream::LittleEndian);
            QDataStream &ds = cw.dataStream();
            bool ok = true;

            ok &= cw.startChunk(ChunkId('B','S','D','B'), version);

            if (!infoText.isEmpty()) {
                ok &= cw.startChunk(ChunkId('I','N','F','O'), version);
                ds << infoText;
                ok &= cw.endChunk();
            }

            ok &= cw.startChunk(ChunkId('C','O','L',' '), version);
            ds << m_colors.count();
            for (const Color *col : m_colors)
                ds << col;
            ok &= cw.endChunk();

            ok &= cw.startChunk(ChunkId('C','A','T',' '), version);
            ds << m_categories.count();
            for (const Category *cat : m_categories)
                ds << cat;
            ok &= cw.endChunk();

            ok &= cw.startChunk(ChunkId('T','Y','P','E'), version);
            ds << m_item_types.count();
            for (const ItemType *itt : m_item_types)
                ds << itt;
            ok &= cw.endChunk();

            ok &= cw.startChunk(ChunkId('I','T','E','M'), version);
            quint32 itc = m_items.count();
            ds << itc;
            for (const Item *item : m_items)
                ds << item;
            ok &= cw.endChunk();

            ok &= cw.startChunk(ChunkId('C','H','G','L'), version);
            quint32 clc = m_changelog.count();
            ds << clc;
            for (const char *cl : m_changelog)
                ds << cl;
            ok &= cw.endChunk();

            ok &= cw.endChunk();

            if (ok && f.error() == QFile::NoError) {
                f.close();

                QString err = Utility::safeRename(filename);

                if (err.isNull())
                    return true;
            }
        }
    }
    if (f.isOpen())
        f.close();

    QFile::remove(filename + ".new");
    return false;
}

int BrickLink::Core::applyChangeLogToItems(InvItemList &bllist)
{
    int count = 0;

    for (InvItem *blitem : qAsConst(bllist)) {
        const InvItem::Incomplete *incpl = blitem->isIncomplete();
        if (incpl) {
            const Item *fixed_item = blitem->item();
            const Color *fixed_color = blitem->color();

            QString itemtypeid = incpl->m_itemtype_name;
            QString itemid     = incpl->m_item_id;
            QString colorid    = incpl->m_color_name;

            for (int i = m_changelog.count() - 1; i >= 0 && !(fixed_color && fixed_item); --i) {
                const ChangeLogEntry &cl = ChangeLogEntry(m_changelog.at(i));

                if (!fixed_item) {
                    if ((cl.type() == BrickLink::ChangeLogEntry::ItemId) ||
                        (cl.type() == BrickLink::ChangeLogEntry::ItemMerge) ||
                        (cl.type() == BrickLink::ChangeLogEntry::ItemType)) {
                        if ((itemtypeid == QLatin1String(cl.from(0))) &&
                            (itemid == QLatin1String(cl.from(1)))) {
                            itemtypeid = QLatin1String(cl.to(0));
                            itemid = QLatin1String(cl.to(1));

                            if (itemtypeid.length() == 1 && !itemid.isEmpty())
                                fixed_item = BrickLink::core()->item(itemtypeid[0].toLatin1(), itemid.toLatin1().constData());
                        }
                    }
                }
                if (!fixed_color) {
                    if (cl.type() == BrickLink::ChangeLogEntry::ColorMerge) {
                        if (colorid == QLatin1String(cl.from(0))) {
                            colorid = QLatin1String(cl.to(0));

                            bool ok;
                            int cid = colorid.toInt(&ok);
                            if (ok)
                                fixed_color = BrickLink::core()->color(cid);
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



