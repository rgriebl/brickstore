// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdio>
#include <cstdlib>

#include <QFile>
#include <QBuffer>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QScopeGuard>

#include "utility/stopwatch.h"
#include "utility/chunkreader.h"
#include "utility/chunkwriter.h"
#include "utility/exception.h"
#include "utility/transfer.h"

#include "bricklink/core.h"
#include "bricklink/database.h"

#include "lzma/bs_lzma.h"


namespace BrickLink {


Database::Database(const QString &updateUrl, QObject *parent)
    : QObject(parent)
    , m_updateUrl(updateUrl)
    , m_transfer(new Transfer(this))
{
    connect(m_transfer, &Transfer::started,
            this, [this](TransferJob *j) {
        if (j != m_job)
            return;
        emit updateStarted();
    });
    connect(m_transfer, &Transfer::progress,
            this, [this](TransferJob *j, int done, int total) {
        if (j != m_job)
            return;
        emit updateProgress(done, total);
    });

    connect(m_transfer, &Transfer::finished,
            this, [this](TransferJob *j) {
        if (j != m_job)
            return;

        auto *hhc = qobject_cast<HashHeaderCheckFilter *>(j->file());
        Q_ASSERT(hhc);
        auto *file = hhc->property("bsFile").value<QSaveFile *>();
        Q_ASSERT(file);

        hhc->close(); // does not close/commit the QSaveFile
        hhc->deleteLater();

        m_job = nullptr;

        try {
            if (!j->isFailed() && j->wasNotModified()) {
                emit updateFinished(true, tr("Already up-to-date."));
                setUpdateStatus(UpdateStatus::Ok);
            } else if (j->isFailed()) {
                throw Exception(tr("download and decompress failed") + u":\n" + j->errorString());
            } else if (!hhc->hasValidChecksum()) {
                throw Exception(tr("checksum mismatch after decompression"));
            } else if (!file->commit()) {
                throw Exception(tr("saving failed") + u":\n" + file->errorString());
            } else {
                read(file->fileName());

                m_etag = j->lastETag();
                QFile etagf(file->fileName() + u".etag");
                if (etagf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    etagf.write(m_etag.toUtf8());
                    etagf.close();
                }

                emit updateFinished(true, { });
                setUpdateStatus(UpdateStatus::Ok);
            }
            emit databaseReset();

        } catch (const Exception &e) {
            emit updateFinished(false, tr("Could not load the new database") + u":\n" + e.errorString());
            setUpdateStatus(UpdateStatus::UpdateFailed);
        }
    });
}

Database::~Database()
{
    clear();
}

void Database::setUpdateInterval(int interval)
{
    m_updateInterval = interval;
}

bool Database::isUpdateNeeded() const
{
    return (m_updateInterval > 0)
            && (!m_valid || (m_lastUpdated.secsTo(QDateTime::currentDateTime()) > m_updateInterval));
}

QString Database::defaultDatabaseName(Version version)
{
    return u"database-v" + QString::number(int(version));
}

void Database::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}

void Database::clear()
{
    m_colors.clear();
    m_ldrawExtraColors.clear();
    m_itemTypes.clear();
    m_categories.clear();
    m_items.clear();
    m_pccs.clear();
    m_itemChangelog.clear();
    m_colorChangelog.clear();
}

bool Database::startUpdate()
{
    return startUpdate(false);
}

bool Database::startUpdate(bool force)
{
    if (m_job || (updateStatus() == UpdateStatus::Updating))
        return false;

    QString dbName = defaultDatabaseName();
    QString remotefile = m_updateUrl + dbName + u".lzma";
    QString localfile = core()->dataPath() + dbName;

    if (!QFile::exists(localfile))
        force = true;

    if (m_etag.isEmpty()) {
        QString dbfile = core()->dataPath() + Database::defaultDatabaseName();
        QFile etagf(dbfile + u".etag");
        if (etagf.open(QIODevice::ReadOnly))
            m_etag = QString::fromUtf8(etagf.readAll());
    }

    auto file = new QSaveFile(localfile);
    auto lzma = new LZMA::DecompressFilter(file);
    auto hhc = new HashHeaderCheckFilter(lzma);
    lzma->setParent(hhc);
    file->setParent(lzma);
    hhc->setProperty("bsFile", QVariant::fromValue(file));

    if (hhc->open(QIODevice::WriteOnly)) {
        m_job = TransferJob::getIfDifferent(QUrl(remotefile), force ? QString { } : m_etag, hhc);
        m_transfer->retrieve(m_job);
    }
    if (!m_job) {
        delete hhc;
        return false;
    }

    setUpdateStatus(UpdateStatus::Updating);

    emit databaseAboutToBeReset();
    return true;
}

void Database::cancelUpdate()
{
    if (m_updateStatus == UpdateStatus::Updating)
        m_transfer->abortAllJobs();
}

void Database::read(const QString &fileName)
{
    try {
        auto *sw = new stopwatch("Loading database");

        QFile f(!fileName.isEmpty() ? fileName : core()->dataPath() + Database::defaultDatabaseName());

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

        if (cr.chunkVersion() != int(Version::Latest)) {
            throw Exception("invalid database version: expected %1, but got %2")
                .arg(int(Version::Latest)).arg(cr.chunkVersion());
        }

        bool gotColors = false, gotCategories = false, gotItemTypes = false, gotItems = false;
        bool gotChangeLog = false, gotPccs = false;

        auto check = [&ds, &f]() {
            if (ds.status() != QDataStream::Ok)
                throw Exception("failed to read from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        auto sizeCheck = [&f](uint s, uint max) {
            if (s > max)
                throw Exception("failed to read from database (%1) at position %2: size value %L3 is larger than expected maximum %L4")
                    .arg(f.fileName()).arg(f.pos()).arg(s).arg(max);
        };

        QDateTime                        generationDate;
        std::vector<Color>               colors;
        std::vector<Color>               ldrawExtraColors;
        std::vector<Category>            categories;
        std::vector<ItemType>            itemTypes;
        std::vector<Item>                items;
        std::vector<ItemChangeLogEntry>  itemChangelog;
        std::vector<ColorChangeLogEntry> colorChangelog;
        std::vector<PartColorCode>       pccs;
        uint                             latestChangelogId = 0;

        while (cr.startChunk()) {
            switch (cr.chunkId() | quint64(cr.chunkVersion()) << 32) {
            case ChunkId('D','A','T','E') | 1ULL << 32: {
                ds >> generationDate;
                break;
            }
            case ChunkId('C','O','L',' ') | 1ULL << 32: {
                quint32 colc = 0;
                ds >> colc;
                check();
                sizeCheck(colc, 1'000);

                colors.resize(colc);
                for (quint32 i = 0; i < colc; ++i) {
                    readColorFromDatabase(colors[i], ds, Version::Latest);
                    check();
                }
                gotColors = true;
                break;
            }
            case ChunkId('L', 'C','O','L') | 1ULL << 32: { // optional, can be missing or empty
                quint32 colc = 0;
                ds >> colc;
                check();
                sizeCheck(colc, 1'000);

                ldrawExtraColors.resize(colc);
                for (quint32 i = 0; i < colc; ++i) {
                    readColorFromDatabase(ldrawExtraColors[i], ds, Version::Latest);
                    check();
                }
                break;
            }
            case ChunkId('C','A','T',' ') | 1ULL << 32: {
                quint32 catc = 0;
                ds >> catc;
                check();
                sizeCheck(catc, 10'000);

                categories.resize(catc);
                for (quint32 i = 0; i < catc; ++i) {
                    readCategoryFromDatabase(categories[i], ds, Version::Latest);
                    check();
                }
                gotCategories = true;
                break;
            }
            case ChunkId('T','Y','P','E') | 1ULL << 32: {
                quint32 ittc = 0;
                ds >> ittc;
                check();
                sizeCheck(ittc, 20);

                itemTypes.resize(ittc);
                for (quint32 i = 0; i < ittc; ++i) {
                    readItemTypeFromDatabase(itemTypes[i], ds, Version::Latest);
                    check();
                }
                gotItemTypes = true;
                break;
            }
            case ChunkId('I','T','E','M') | 1ULL << 32: {
                quint32 itc = 0;
                ds >> itc;
                check();
                sizeCheck(itc, 1'000'000);

                items.resize(itc);
                for (quint32 i = 0; i < itc; ++i) {
                    readItemFromDatabase(items[i], ds, Version::Latest);
                    check();
                }
                items.shrink_to_fit();
                gotItems = true;
                break;
            }
            case ChunkId('C','H','G','L') | 2ULL << 32: {
                quint32 clid = 0, clic = 0, clcc = 0;
                ds >> clid >> clic >> clcc;
                check();
                sizeCheck(clic, 1'000'000);
                sizeCheck(clcc, 1'000);

                itemChangelog.resize(clic);
                for (quint32 i = 0; i < clic; ++i) {
                    readItemChangeLogFromDatabase(itemChangelog[i], ds, Version::Latest);
                    check();
                }
                colorChangelog.resize(clcc);
                for (quint32 i = 0; i < clcc; ++i) {
                    readColorChangeLogFromDatabase(colorChangelog[i], ds, Version::Latest);
                    check();
                }
                latestChangelogId = clid;
                gotChangeLog = true;
                break;
            }
            case ChunkId('P','C','C',' ') | 1ULL << 32: {
                quint32 pccc = 0;
                ds >> pccc;
                check();
                sizeCheck(pccc, 1'000'000);

                pccs.resize(pccc);
                for (quint32 i = 0; i < pccc; ++i) {
                    readPCCFromDatabase(pccs[i], ds, Version::Latest);
                    check();
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

        {
            QString out = u"Loaded database from " + f.fileName();
            QLocale loc = QLocale(QLocale::Swedish); // space as number group separator
            QVector<std::pair<QString, QString>> log = {
                { u"Generated at"_qs, generationDate.toString(Qt::RFC2822Date) },
                { u"Changelog Id"_qs, QString::number(latestChangelogId) },
                { u"  Items"_qs,      loc.toString(itemChangelog.size()).rightJustified(10) },
                { u"  Colors"_qs,     loc.toString(colorChangelog.size()).rightJustified(10) },
                { u"Item Types"_qs,   loc.toString(itemTypes.size()).rightJustified(10) },
                { u"Categories"_qs,   loc.toString(categories.size()).rightJustified(10) },
                { u"Colors"_qs,       loc.toString(colors.size()).rightJustified(10) },
                { u"LDraw Colors"_qs, loc.toString(ldrawExtraColors.size()).rightJustified(10) },
                { u"PCCs"_qs,         loc.toString(pccs.size()).rightJustified(10) },
                { u"Items"_qs,        loc.toString(items.size()).rightJustified(10) },
            };
#if defined(QT_DEBUG)
            std::vector<int> itemCount(itemTypes.size());
            for (const auto &item : std::as_const(items))
                ++itemCount[item.m_itemTypeIndex];
            for (size_t i = 0; i < itemTypes.size(); ++i) {
                log.append({ u"  "_qs + itemTypes.at(i).name(),
                            loc.toString(itemCount.at(i)).rightJustified(10) });
            }
#endif
            qsizetype leftSize = 0;
            for (const auto &logPair : std::as_const(log))
                leftSize = std::max(leftSize, logPair.first.length());
            for (const auto &logPair : std::as_const(log))
                out = out + u"\n  " + logPair.first.leftJustified(leftSize) + u": " + logPair.second;
            qInfo().noquote() << out;
        }

        m_colors = colors;
        m_ldrawExtraColors = ldrawExtraColors;
        m_categories = categories;
        m_itemTypes = itemTypes;
        m_items = items;
        m_itemChangelog = itemChangelog;
        m_colorChangelog = colorChangelog;
        m_pccs = pccs;
        m_latestChangelogId = latestChangelogId;

        Color::s_colorImageCache.clear();

        if (generationDate != m_lastUpdated) {
            m_lastUpdated = generationDate;
            emit lastUpdatedChanged(generationDate);
        }
        if (!m_valid) {
            m_valid = true;
            emit validChanged(m_valid);
        }
    } catch (const Exception &) {
        if (m_valid) {
            m_valid = false;
            m_etag.clear(); // better redownload the db in this case
            emit validChanged(m_valid);
        }
        throw;
    }
}

void Database::write(const QString &filename, Version version) const
{
    if (version <= Version::Invalid)
        throw Exception("version %1 is too old").arg(int(version));

    QString fn(!filename.isEmpty() ? filename : core()->dataPath() + defaultDatabaseName(version));

    QSaveFile f(fn);
    if (!f.open(QIODevice::WriteOnly))
        throw Exception(&f, "could not open database for writing");

    ChunkWriter cw(&f, QDataStream::LittleEndian);
    QDataStream &ds = cw.dataStream();

    auto check = [&ds, &f](bool ok) {
        if (!ok || (ds.status() != QDataStream::Ok))
            throw Exception("failed to write to database (%1) at position %2")
                .arg(f.fileName()).arg(f.pos());
    };

    check(cw.startChunk(ChunkId('B','S','D','B'), uint(version)));

    check(cw.startChunk(ChunkId('D','A','T','E'), 1));
    ds << QDateTime::currentDateTimeUtc();
    check(cw.endChunk());

    check(cw.startChunk(ChunkId('C','O','L',' '), 1));
    ds << quint32(m_colors.size());
    for (const Color &col : m_colors)
        writeColorToDatabase(col, ds, version);
    check(cw.endChunk());

    if ((version >= Version::V7) && !m_ldrawExtraColors.empty()) {
        check(cw.startChunk(ChunkId('L','C','O','L'), 1));
        ds << quint32(m_ldrawExtraColors.size());
        for (const Color &col : m_ldrawExtraColors)
            writeColorToDatabase(col, ds, version);
        check(cw.endChunk());
    }

    check(cw.startChunk(ChunkId('C','A','T',' '), 1));
    ds << quint32(m_categories.size());
    for (const Category &cat : m_categories)
        writeCategoryToDatabase(cat, ds, version);
    check(cw.endChunk());

    check(cw.startChunk(ChunkId('T','Y','P','E'), 1));
    ds << quint32(m_itemTypes.size());
    for (const ItemType &itt : m_itemTypes)
        writeItemTypeToDatabase(itt, ds, version);
    check(cw.endChunk());

    check(cw.startChunk(ChunkId('I','T','E','M'), 1));
    ds << quint32(m_items.size());
    for (const Item &item : m_items)
        writeItemToDatabase(item, ds, version);
    check(cw.endChunk());

    if (version >= Version::V9) {
        check(cw.startChunk(ChunkId('C','H','G','L'), 2));
        ds << quint32(m_latestChangelogId)
           << quint32(m_itemChangelog.size())
           << quint32(m_colorChangelog.size());
        for (const ItemChangeLogEntry &e : m_itemChangelog)
            writeItemChangeLogToDatabase(e, ds, version);
        for (const ColorChangeLogEntry &e : m_colorChangelog)
            writeColorChangeLogToDatabase(e, ds, version);
        check(cw.endChunk());
    } else if (version >= Version::V5) {
        check(cw.startChunk(ChunkId('I','C','H','G'), 1));
        ds << quint32(m_itemChangelog.size());
        for (const ItemChangeLogEntry &e : m_itemChangelog)
            writeItemChangeLogToDatabase(e, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','C','H','G'), 1));
        ds << quint32(m_colorChangelog.size());
        for (const ColorChangeLogEntry &e : m_colorChangelog)
            writeColorChangeLogToDatabase(e, ds, version);
        check(cw.endChunk());
    } else {
        check(cw.startChunk(ChunkId('C','H','G','L'), 1));
        ds << quint32(m_itemChangelog.size() + m_colorChangelog.size());
        for (const ItemChangeLogEntry &e : m_itemChangelog) {
            ds << static_cast<QByteArray>("\x03\t" + QByteArray(1, e.fromItemTypeId()) + '\t' + e.fromItemId()
                                          + '\t' + e.toItemTypeId() + '\t' + e.toItemId());
        }
        for (const ColorChangeLogEntry &e : m_colorChangelog) {
            ds << static_cast<QByteArray>("\x07\t" + QByteArray::number(e.fromColorId()) + "\tx\t"
                                          + QByteArray::number(e.toColorId()) + "\tx");
        }
        check(cw.endChunk());
    }

    check(cw.startChunk(ChunkId('P','C','C',' '), 1));
    ds << quint32(m_pccs.size());
    for (const PartColorCode &pcc : m_pccs)
        writePCCToDatabase(pcc, ds, version);
    check(cw.endChunk());

    check(cw.endChunk()); // BSDB root chunk

    if (!f.commit())
        throw Exception(f.errorString());
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


void Database::readColorFromDatabase(Color &col, QDataStream &dataStream, Version)
{
    quint32 flags;

    dataStream >> col.m_id >> col.m_name >> col.m_ldraw_id >> col.m_color >> flags
            >> col.m_popularity >> col.m_year_from >> col.m_year_to;
    col.m_type = static_cast<ColorType>(flags);

    dataStream >> col.m_ldraw_color >> col.m_ldraw_edge_color >> col.m_luminance
            >> col.m_particleMinSize >> col.m_particleMaxSize >> col.m_particleColor
            >> col.m_particleFraction >> col.m_particleVFraction;
}

void Database::writeColorToDatabase(const Color &col, QDataStream &dataStream, Version v) const
{
    dataStream << col.m_id << col.m_name << col.m_ldraw_id << col.m_color << quint32(col.m_type)
               << col.m_popularity << col.m_year_from << col.m_year_to;

    if (v >= Version::V7) {
        dataStream << col.m_ldraw_color << col.m_ldraw_edge_color << col.m_luminance
                   << col.m_particleMinSize << col.m_particleMaxSize << col.m_particleColor
                   << col.m_particleFraction << col.m_particleVFraction;
    }
}


void Database::readCategoryFromDatabase(Category &cat, QDataStream &dataStream, Version)
{
    dataStream >> cat.m_id >> cat.m_name >> cat.m_year_from >> cat.m_year_to >> cat.m_year_recency
            >> cat.m_has_inventories;
}

void Database::writeCategoryToDatabase(const Category &cat, QDataStream &dataStream, Version v) const
{
    dataStream << cat.m_id << cat.m_name;
    if (v >= Version::V6)
        dataStream << cat.m_year_from << cat.m_year_to << cat.m_year_recency;
    if (v >= Version::V8)
        dataStream << cat.m_has_inventories;
}


void Database::readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, Version)
{
    quint8 flags = 0;
    quint32 catSize = 0;
    dataStream >> reinterpret_cast<qint8 &>(itt.m_id) >> itt.m_name >> flags >> catSize;

    itt.m_categoryIndexes.reserve(catSize);
    while (catSize-- > 0) {
        quint16 catIdx = 0;
        dataStream >> catIdx;
        itt.m_categoryIndexes.emplace_back(catIdx);
    }

    itt.m_has_inventories   = flags & 0x01;
    itt.m_has_colors        = flags & 0x02;
    itt.m_has_weight        = flags & 0x04;
    itt.m_has_subconditions = flags & 0x10;
}

void Database::writeItemTypeToDatabase(const ItemType &itt, QDataStream &dataStream, Version v) const
{
    quint8 flags = 0;
    flags |= (itt.m_has_inventories   ? 0x01 : 0);
    flags |= (itt.m_has_colors        ? 0x02 : 0);
    flags |= (itt.m_has_weight        ? 0x04 : 0);
    flags |=                            0x08     ; // was: m_has_year
    flags |= (itt.m_has_subconditions ? 0x10 : 0);

    dataStream << qint8(itt.m_id);
    if (v < Version::V8)
        dataStream << qint8(itt.m_id);
    dataStream << itt.m_name << flags << quint32(itt.m_categoryIndexes.size());

    for (const quint16 catIdx : itt.m_categoryIndexes)
        dataStream << catIdx;
}


void Database::readItemFromDatabase(Item &item, QDataStream &dataStream, Version)
{
    dataStream >> item.m_id >> item.m_name >> item.m_itemTypeIndex >> item.m_categoryIndex
            >> item.m_defaultColorIndex >> reinterpret_cast<qint8 &>(item.m_itemTypeId)
            >> item.m_year_from >> item.m_lastInventoryUpdate >> item.m_weight >> item.m_year_to;

    quint32 appearsInSize = 0;
    dataStream >> appearsInSize;
    item.m_appears_in.clear();
    item.m_appears_in.reserve(appearsInSize);

    Item::AppearsInRecord air;
    while (appearsInSize-- > 0) {
        dataStream >> air.m_data;
        item.m_appears_in.push_back(air);
    }
    item.m_appears_in.shrink_to_fit();

    quint32 consistsOfSize = 0;
    dataStream >> consistsOfSize;
    item.m_consists_of.clear();
    item.m_consists_of.reserve(int(consistsOfSize));

    Item::ConsistsOf co;
    while (consistsOfSize-- > 0) {
        dataStream >> co.m_data;
        item.m_consists_of.push_back(co);
    }
    item.m_consists_of.shrink_to_fit();

    quint32 knownColorsSize;
    dataStream >> knownColorsSize;
    item.m_knownColorIndexes.clear();
    item.m_knownColorIndexes.reserve(knownColorsSize);

    quint16 colorIndex;
    while (knownColorsSize-- > 0) {
        dataStream >> colorIndex;
        item.m_knownColorIndexes.push_back(colorIndex);
    }
    item.m_knownColorIndexes.shrink_to_fit();

    quint32 additonalCategoriesSize;
    dataStream >> additonalCategoriesSize;
    item.m_additionalCategoryIndexes.clear();
    item.m_additionalCategoryIndexes.reserve(additonalCategoriesSize);

    qint16 catIndex;
    while (additonalCategoriesSize-- > 0) {
        dataStream >> catIndex;
        item.m_additionalCategoryIndexes.push_back(catIndex);
    }
    item.m_additionalCategoryIndexes.shrink_to_fit();
}

void Database::writeItemToDatabase(const Item &item, QDataStream &dataStream, Version v) const
{
    dataStream << item.m_id << item.m_name << item.m_itemTypeIndex << item.m_categoryIndex
               << item.m_defaultColorIndex << qint8(item.m_itemTypeId) << item.m_year_from
               << item.m_lastInventoryUpdate << item.m_weight;

    if (v >= Version::V6)
        dataStream << item.m_year_to;

    dataStream << quint32(item.m_appears_in.size());
    for (const auto &ai : item.m_appears_in)
        dataStream << ai.m_data;

    dataStream << quint32(item.m_consists_of.size());
    for (const auto &co : item.m_consists_of)
        dataStream << co.m_data;

    dataStream << quint32(item.m_knownColorIndexes.size());
    for (const quint16 colorIndex : item.m_knownColorIndexes)
        dataStream << colorIndex;

    if (v >= Version::V6) {
        dataStream << quint32(item.m_additionalCategoryIndexes.size());
        for (const qint16 categoryIndex : item.m_additionalCategoryIndexes)
            dataStream << categoryIndex;
    }
}

void Database::readPCCFromDatabase(PartColorCode &pcc, QDataStream &dataStream, Version)
{
    qint32 itemIndex, colorIndex;
    dataStream >> pcc.m_id >> itemIndex >> colorIndex;
    pcc.m_itemIndex = itemIndex;
    pcc.m_colorIndex = colorIndex;
}

void Database::writePCCToDatabase(const PartColorCode &pcc, QDataStream &dataStream, Version) const
{
    dataStream << pcc.m_id << qint32(pcc.m_itemIndex) << qint32(pcc.m_colorIndex);
}

void Database::readItemChangeLogFromDatabase(ItemChangeLogEntry &e, QDataStream &dataStream, Version)
{
    dataStream >> e.m_id >> e.m_julianDay >> e.m_fromTypeAndId >> e.m_toTypeAndId;
}

void Database::writeItemChangeLogToDatabase(const ItemChangeLogEntry &e, QDataStream &dataStream, Version v) const
{
    if (v >= Version::V9)
        dataStream << e.m_id << e.m_julianDay;
    dataStream << e.m_fromTypeAndId << e.m_toTypeAndId;
}

void Database::readColorChangeLogFromDatabase(ColorChangeLogEntry &e, QDataStream &dataStream, Version)
{
    dataStream >> e.m_id >> e.m_julianDay >> e.m_fromColorId >> e.m_toColorId;
}

void Database::writeColorChangeLogToDatabase(const ColorChangeLogEntry &e, QDataStream &dataStream, Version v) const
{
    if (v >= Version::V9)
        dataStream << e.m_id << e.m_julianDay;
    dataStream << e.m_fromColorId << e.m_toColorId;
}

} // namespace BrickLink

//#include "moc_database.cpp" // QTBUG-98845
