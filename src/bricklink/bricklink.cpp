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
#include <QStringBuilder>

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

    case URL_CatalogInfo: {
        auto item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/catalogItem.asp";
            QUrlQuery query;
            query.addQueryItem(QChar(item->itemType()->id()), item->id());
            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("C", QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_PriceGuideInfo: {
        auto *item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/catalogPG.asp";
            QUrlQuery query;
            query.addQueryItem(QChar(item->itemType()->id()), item->id());
            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("colorID", QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_LotsForSale: {
        auto item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
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

            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("colorID", QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_AppearsInSets: {
        auto item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/catalogItemIn.asp";
            QUrlQuery query;
            query.addQueryItem(QChar(item->itemType()->id()), item->id());
            query.addQueryItem("in", "S");

            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("colorID", QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_ColorChangeLog:
        url = "https://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=R";
        break;

    case URL_ItemChangeLog: {
        url = "https://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=I";
        QUrlQuery query;
        if (opt)
            query.addQueryItem("q", static_cast<const char *>(opt));
        url.setQuery(query);
        break;
    }
    case URL_StoreItemDetail: {
        auto lotId = static_cast<const unsigned int *>(opt);
        if (lotId && *lotId) {
            url = "https://www.bricklink.com/inventory_detail.asp";
            QUrlQuery query;
            query.addQueryItem("invID", QString::number(*lotId));
            url.setQuery(query);
        }
        break;
    }
    case URL_StoreItemSearch: {
        const Item *item = static_cast<const Item *>(opt);
        const Color *color = static_cast<const Color *>(opt2);
        if (item && item->itemType() && color) {
            url = "https://www.bricklink.com/inventory_detail.asp?";
            QUrlQuery query;
            query.addQueryItem("catType", QChar(item->itemType()->id()));
            query.addQueryItem("q", color->name() % u' ' % item->id());
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

            p.setPen(QPen(i, w / 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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

QFile *Core::dataFile(QStringView fileName, QIODevice::OpenMode openMode,
                       const Item *item, const Color *color) const
{
    // Avoid huge directories with 1000s of entries.
    uchar hash = qHash(item->id(), 42) & 0xff; // sse4.2 is only used if a seed value is supplied

    QString p = m_datadir % QChar(item->itemType()->id()) % u'/' % (hash < 0x10 ? u"0" : u"")
            % QString::number(hash, 16) % u'/' % item->id() % u'/'
            % (color ? QString::number(color->id()) : QString()) % (color ? u"/" : u"")
            % fileName;

    if (openMode != QIODevice::ReadOnly) {
        if (!QDir(fileName.isEmpty() ? p : p.left(p.size() - fileName.size())).mkpath("."))
            return nullptr;
    }
    auto f = new QFile(p);
    f->open(openMode);
    return f;
}



Core *Core::s_inst = nullptr;

Core *Core::create(const QString &datadir, QString *errstring)
{
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

Core::Core(const QString &datadir)
    : m_datadir(QDir::cleanPath(QDir(datadir).absolutePath()) + u'/')
    , m_c_locale(QLocale::c())
    , m_corelock(QMutex::Recursive)
{
    m_pic_diskload.setMaxThreadCount(QThread::idealThreadCount() * 3);
    m_online = true;

    // cache size is physical memory * 0.25, at least 128MB, at most 1GB
    quint64 cachemem = qBound(128ULL *1024*1024, Utility::physicalMemory() / 4, 1024ULL *1024*1024);
    //quint64 cachemem = 1024*1024; // DEBUG

    m_pg_cache.setMaxCost(10000);            // each priceguide has a cost of 1
    m_pic_cache.setMaxCost(int(cachemem));   // each pic has roughly the cost of memory used
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
    m_db_update_iv = intervals["Database"];
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


const std::vector<const Color *> &Core::colors() const
{
    return m_colors;
}

const std::vector<const Category *> &Core::categories() const
{
    return m_categories;
}

const std::vector<const ItemType *> &Core::itemTypes() const
{
    return m_item_types;
}

const std::vector<const Item *> &Core::items() const
{
    return m_items;
}

const Category *Core::category(uint id) const
{
    auto it = std::lower_bound(m_categories.cbegin(), m_categories.cend(), id, &Category::lowerBound);
    if ((it != m_categories.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const Color *Core::color(uint id) const
{
    auto it = std::lower_bound(m_colors.cbegin(), m_colors.cend(), id, &Color::lowerBound);
    if ((it != m_colors.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const Color *Core::colorFromName(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;

    auto it = std::find_if(m_colors.cbegin(), m_colors.cend(), [name](const auto &color) {
        return !color->name().compare(name, Qt::CaseInsensitive);
    });
    if (it != m_colors.cend())
        return *it;
    return nullptr;
}


const Color *Core::colorFromLDrawId(int ldrawId) const
{
    auto it = std::find_if(m_colors.cbegin(), m_colors.cend(), [ldrawId](const auto &color) {
        return (color->ldrawId() == ldrawId);
    });
    if (it != m_colors.cend())
        return *it;
    return nullptr;
}


const ItemType *Core::itemType(char id) const
{
    auto it = std::lower_bound(m_item_types.cbegin(), m_item_types.cend(), id, &ItemType::lowerBound);
    if ((it != m_item_types.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const Item *Core::item(char tid, const QString &id) const
{
    auto needle = std::make_pair(tid, id);
    auto it = std::lower_bound(m_items.cbegin(), m_items.cend(), needle, Item::lowerBound);
    if ((it != m_items.cend()) && ((*it)->itemType()->id() == tid) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const PartColorCode *Core::partColorCode(uint id)
{
    auto it = std::lower_bound(m_pccs.cbegin(), m_pccs.cend(), id, &PartColorCode::lowerBound);
    if ((it != m_pccs.cend()) && ((*it)->id() == id))
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

QDateTime Core::databaseDate() const
{
    return m_databaseDate;
}

bool Core::isDatabaseUpdateNeeded() const
{
    return updateNeeded(m_databaseDate.isValid(), m_databaseDate, m_db_update_iv);
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
    qDeleteAll(m_pccs);

    m_colors.clear();
    m_item_types.clear();
    m_categories.clear();
    m_items.clear();
    m_pccs.clear();
    m_changelog.clear();
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

        QDateTime generationDate;
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
        bool gotChangeLog = false, gotPccs = false;

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
            case ChunkId('D','A','T','E') | 1ULL << 32: {
                ds >> generationDate;
                break;
            }
            case ChunkId('C','O','L',' ') | 1ULL << 32: {
                quint32 colc = 0;
                ds >> colc;
                check();

                for (quint32 i = colc; i; i--) {
                    auto *col = readColorFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_colors.emplace_back(col);
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
                    m_categories.emplace_back(cat);
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
                    m_item_types.emplace_back(itt);
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
                    m_items.emplace_back(item);
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
                    m_changelog.emplace_back(entry);
                }
                gotChangeLog = true;
                break;
            }
            case ChunkId('P','C','C',' ') | 1ULL << 32: {
                if (!gotItems || !gotColors)
                    throw Exception("found a 'PCC ' chunk before the 'ITEM' and 'COL ' chunks");

                quint32 pccc = 0;
                ds >> pccc;
                check();

                m_pccs.reserve(int(pccc));
                for (quint32 i = pccc; i; i--) {
                    auto *pcc = readPCCFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_pccs.emplace_back(pcc);
                }
                gotPccs = true;
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

        if (!gotColors || !gotCategories || !gotItemTypes || !gotItems || !gotChangeLog || !gotPccs) {
            throw Exception("not all required data chunks were found in the database (%1)")
                .arg(f.fileName());
        }

        qDebug() << "Loaded database from" << f.fileName() << endl
                 << "  Generated at:" << generationDate << endl
                 << "  Colors      :" << m_colors.size() << endl
                 << "  Item Types  :" << m_item_types.size() << endl
                 << "  Categories  :" << m_categories.size() << endl
                 << "  Items       :" << m_items.size() << endl
                 << "  PCCs        :" << m_pccs.size();

        m_databaseDate = generationDate;
        emit databaseDateChanged(generationDate);

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

        check(cw.startChunk(ChunkId('D','A','T','E'), 1));
        ds << QDateTime::currentDateTimeUtc();
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','O','L',' '), 1));
        ds << quint32(m_colors.size());
        for (const Color *col : m_colors)
            writeColorToDatabase(col, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','A','T',' '), 1));
        ds << quint32(m_categories.size());
        for (const Category *cat : m_categories)
            writeCategoryToDatabase(cat, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('T','Y','P','E'), 1));
        ds << quint32(m_item_types.size());
        for (const ItemType *itt : m_item_types)
            writeItemTypeToDatabase(itt, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('I','T','E','M'), 1));
        ds << quint32(m_items.size());
        for (const Item *item : m_items)
            writeItemToDatabase(item, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','H','G','L'), 1));
        ds << quint32(m_changelog.size());
        for (const QByteArray &cl : m_changelog)
            ds << cl;
        check(cw.endChunk());

        if (version >= DatabaseVersion::Version_3) {
            check(cw.startChunk(ChunkId('P','C','C',' '), 1));
            ds << quint32(m_pccs.size());
            for (const PartColorCode *pcc : m_pccs)
                writePCCToDatabase(pcc, ds, version);
            check(cw.endChunk());
        }

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
    itt->m_has_subconditions = flags & 0x10;

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


Item *Core::readItemFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<Item> item(new Item);

    qint8 ittid = 0;
    quint32 catid = 0;
    QByteArray id;
    QByteArray name;

    dataStream >> id >> name >> ittid >> catid;
    item->m_id = QString::fromUtf8(id);
    item->m_name = QString::fromUtf8(name);
    item->m_item_type = BrickLink::core()->itemType(ittid);
    item->m_category = BrickLink::core()->category(catid);

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

    quint32 known_colors_count;
    dataStream >> known_colors_count;
    item->m_known_colors.resize(int(known_colors_count));

    for (int i = 0; i < int(known_colors_count); i++) {
        quint32 colid = 0;
        dataStream >> colid;
        item->m_known_colors[i] = colid;
    }

    return item.take();
}

void Core::writeItemToDatabase(const Item *item, QDataStream &dataStream, DatabaseVersion v)
{
    dataStream << item->m_id.toUtf8() << item->m_name.toUtf8() << qint8(item->m_item_type->id());

    if (v <= DatabaseVersion::Version_2)
        dataStream << quint32(1); // this used to be a list of category ids
    dataStream << quint32(item->category()->id());

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

PartColorCode *Core::readPCCFromDatabase(QDataStream &dataStream, Core::DatabaseVersion) const
{
    QScopedPointer<PartColorCode> pcc(new PartColorCode);

    qint8 itemTypeId;
    QString itemId;
    uint colorId;

    dataStream >> pcc->m_id >> itemTypeId >> itemId >> colorId;

    pcc->m_item = item(itemTypeId, itemId);
    pcc->m_color = color(colorId);
    return pcc.take();
}

void Core::writePCCToDatabase(const PartColorCode *pcc,
                              QDataStream &dataStream, Core::DatabaseVersion)
{
    dataStream << pcc->id() << qint8(pcc->item()->itemType()->id()) << pcc->item()->id()
               << pcc->color()->id();
}


bool Core::applyChangeLogToItem(InvItem *item)
{
    const InvItem::Incomplete *incpl = item->isIncomplete();
    if (!incpl)
        return false;

    const Item *fixed_item = item->item();
    const Color *fixed_color = item->color();

    QString itemtypeid = incpl->m_itemtype_id;
    QString itemid     = incpl->m_item_id;
    QString colorid    = incpl->m_color_name;

    if (itemtypeid.isEmpty() && !incpl->m_itemtype_name.isEmpty())
        itemtypeid = incpl->m_itemtype_name.at(0).toUpper();

    for (int i = int(m_changelog.size()) - 1; i >= 0 && !(fixed_color && fixed_item); --i) {
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

    if (fixed_item && !item->item())
        item->setItem(fixed_item);
    if (fixed_color && !item->color())
        item->setColor(fixed_color);

    if (fixed_item && fixed_color) {
        item->setIncomplete(nullptr);
        return true;
    }
    return false;
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

int InvItem::quantitySoldLast6Months()
{
    if ((m_quantitySoldLast6Months < 0) && !m_priceGuideRequested) {
        auto priceGuide = BrickLink::core()->priceGuide(this->m_item, this->m_color);
        priceGuide->update(true);
        QObject::connect(BrickLink::core(), &BrickLink::Core::priceGuideUpdated, [this] (BrickLink::PriceGuide *pg) {
            if (pg->item()->id() == m_item->id() && pg->color() == m_color) {
                m_quantitySoldLast6Months = pg->quantity(Time::PastSix, Condition::New);
                m_lotsSoldLast6Months = pg->lots(Time::PastSix, Condition::New);
            }
        });
        m_priceGuideRequested = true;
    }
    return m_quantitySoldLast6Months;
}

int InvItem::lotsSoldLast6Months()
{
    if ((m_lotsSoldLast6Months < 0) && !m_priceGuideRequested) {
        auto priceGuide = BrickLink::core()->priceGuide(this->m_item, this->m_color);
        priceGuide->update(true);
        QObject::connect(BrickLink::core(), &BrickLink::Core::priceGuideUpdated, [this] (BrickLink::PriceGuide *pg) {
            if (pg->item()->id() == m_item->id() && pg->color() == m_color) {
                m_quantitySoldLast6Months = pg->quantity(Time::PastSix, Condition::New);
                m_lotsSoldLast6Months = pg->lots(Time::PastSix, Condition::New);
            }
        });
        m_priceGuideRequested = true;
    }
    return m_lotsSoldLast6Months;
}


} // namespace BrickLink

#include "moc_bricklink.cpp"

