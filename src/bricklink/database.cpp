// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdio>
#include <cstdlib>

#include <QFile>
#include <QBuffer>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
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
    m_itemChangelog.clear();
    m_colorChangelog.clear();
    m_pool.reset();
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
    QString remotefile = u"https://" + m_updateUrl + u'/' + dbName + u".lzma";
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
        m_job = TransferJob::get(remotefile);
        if (!force)
            m_job->setOnlyIfDifferent(m_etag);
        m_job->setOutputDevice(hhc);
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

        ds.startTransaction();
        cr.startChunk();

        if (cr.chunkId() != ChunkId("BSDB"))
            throw Exception("invalid database format - wrong magic (%1)").arg(f.fileName());

        if (cr.chunkVersion() != int(Version::Latest)) {
            throw Exception("invalid database version: expected %1, but got %2")
                .arg(int(Version::Latest)).arg(cr.chunkVersion());
        }

        bool gotColors = false, gotCategories = false, gotItemTypes = false, gotItems = false;
        bool gotChangeLog = false, gotRelationships = false, gotRelationshipMatches = false;
        bool gotApiKeys = false, gotApiQuirks = false;

        auto check = [&ds, &f]() {
            if (ds.status() != QDataStream::Ok)
                throw Exception("failed to read from database (%1) at position %2")
                    .arg(f.fileName()).arg(ds.device()->pos());
        };

        auto sizeCheck = [&f](uint s, uint max) {
            if (s > max)
                throw Exception("failed to read from database (%1) at position %2: size value %L3 is larger than expected maximum %L4")
                    .arg(f.fileName()).arg(f.pos()).arg(s).arg(max);
        };

        // This is the new pool. We need to keep the old alive till the scope end
        std::unique_ptr<MemoryResource> pool(new DatabaseMonotonicMemoryResource(1024*1024));

        QDateTime                        generationDate;
        std::vector<Color>               colors;
        std::vector<Color>               ldrawExtraColors;
        std::vector<Category>            categories;
        std::vector<ItemType>            itemTypes;
        std::vector<Item>                items;
        std::vector<ItemChangeLogEntry>  itemChangelog;
        std::vector<ColorChangeLogEntry> colorChangelog;
        std::vector<Relationship>        relationships;
        std::vector<RelationshipMatch>   relationshipMatches;
        uint                             latestChangelogId = 0;
        QHash<QByteArray, QString>       apiKeys;
        QSet<ApiQuirk>                   apiQuirks;

        while (cr.startChunk()) {
            switch (cr.chunkIdAndVersion()) {
            case ChunkIdAndVersion("DATE", 1): {
                ds >> generationDate;
                break;
            }
            case ChunkIdAndVersion("COL ", 1): {
                quint32 colc = 0;
                ds >> colc;
                check();
                sizeCheck(colc, 1'000);

                colors.resize(colc);
                for (quint32 i = 0; i < colc; ++i) {
                    readColorFromDatabase(colors[i], ds, pool.get());
                    check();
                }
                gotColors = true;
                break;
            }
            case ChunkIdAndVersion("LCOL", 1): { // optional, can be missing or empty
                quint32 colc = 0;
                ds >> colc;
                check();
                sizeCheck(colc, 1'000);

                ldrawExtraColors.resize(colc);
                for (quint32 i = 0; i < colc; ++i) {
                    readColorFromDatabase(ldrawExtraColors[i], ds, pool.get());
                    check();
                }
                break;
            }
            case ChunkIdAndVersion("CAT ", 1): {
                quint32 catc = 0;
                ds >> catc;
                check();
                sizeCheck(catc, 10'000);

                categories.resize(catc);
                for (quint32 i = 0; i < catc; ++i) {
                    readCategoryFromDatabase(categories[i], ds, pool.get());
                    check();
                }
                gotCategories = true;
                break;
            }
            case ChunkIdAndVersion("TYPE", 1): {
                quint32 ittc = 0;
                ds >> ittc;
                check();
                sizeCheck(ittc, 20);

                itemTypes.resize(ittc);
                for (quint32 i = 0; i < ittc; ++i) {
                    readItemTypeFromDatabase(itemTypes[i], ds, pool.get());
                    check();
                }
                gotItemTypes = true;
                break;
            }
            case ChunkIdAndVersion("ITEM", 1): {
                quint32 itc = 0;
                ds >> itc;
                check();
                sizeCheck(itc, 1'000'000);

                items.resize(itc);
                for (quint32 i = 0; i < itc; ++i) {
                    readItemFromDatabase(items[i], ds, pool.get());
                    check();
                }
                gotItems = true;
                break;
            }
            case ChunkIdAndVersion("CHGL", 2): {
                quint32 clid = 0, clic = 0, clcc = 0;
                ds >> clid >> clic >> clcc;
                check();
                sizeCheck(clic, 1'000'000);
                sizeCheck(clcc, 1'000);

                itemChangelog.resize(clic);
                for (quint32 i = 0; i < clic; ++i) {
                    readItemChangeLogFromDatabase(itemChangelog[i], ds, pool.get());
                    check();
                }
                colorChangelog.resize(clcc);
                for (quint32 i = 0; i < clcc; ++i) {
                    readColorChangeLogFromDatabase(colorChangelog[i], ds, pool.get());
                    check();
                }
                latestChangelogId = clid;
                gotChangeLog = true;
                break;
            }
            case ChunkIdAndVersion("REL ", 1): {
                quint32 relc = 0;
                ds >> relc;
                check();
                sizeCheck(relc, 1'000);

                relationships.resize(relc);
                for (quint32 i = 0; i < relc; ++i) {
                    readRelationshipFromDatabase(relationships[i], ds, pool.get());
                    check();
                }
                gotRelationships = true;
                break;
            }
            case ChunkIdAndVersion("RELM", 1): {
                quint32 matchc = 0;
                ds >> matchc;
                check();
                sizeCheck(matchc, 1'000'000);

                relationshipMatches.resize(matchc);
                for (quint32 i = 0; i < matchc; ++i) {
                    readRelationshipMatchFromDatabase(relationshipMatches[i], ds, pool.get());
                    check();
                }
                gotRelationshipMatches = true;
                break;
            }
            case ChunkIdAndVersion("AKEY", 1): {
                quint32 akeyc = 0;
                ds >> akeyc;
                check();
                sizeCheck(akeyc, 100);

                for (quint32 i = 0; i < akeyc; ++i) {
                    QByteArray id;
                    QString key;
                    readApiKeyFromDatabase(id, key, ds, pool.get());
                    check();
                    apiKeys.insert(id, key);
                }
                gotApiKeys = true;
                break;
            }
            case ChunkIdAndVersion("QIRK", 1): {
                quint32 qkeyc = 0;
                ds >> qkeyc;
                check();
                sizeCheck(qkeyc, 100);

                for (quint32 i = 0; i < qkeyc; ++i) {
                    quint32 quirk;
                    ds >> quirk;
                    check();
                    apiQuirks.insert(static_cast<BrickLink::ApiQuirk>(quirk));
                }
                gotApiQuirks = true;
                break;
            }
            default: {
                cr.skipChunk();
                check();
                break;
            }
            }
            cr.endChunk();
        }
        cr.endChunk();
        ds.commitTransaction();

        delete sw;

        if (!gotColors || !gotCategories || !gotItemTypes || !gotItems || !gotChangeLog
            || !gotRelationships || !gotRelationshipMatches || !gotApiKeys) {
            throw Exception("not all required data chunks were found in the database (%1)")
                .arg(f.fileName());
        }

        m_colors = std::move(colors);
        m_ldrawExtraColors = std::move(ldrawExtraColors);
        m_categories = std::move(categories);
        m_itemTypes = std::move(itemTypes);
        m_items = std::move(items);
        m_itemChangelog = std::move(itemChangelog);
        m_colorChangelog = std::move(colorChangelog);
        m_relationships = std::move(relationships);
        m_relationshipMatches = std::move(relationshipMatches);
        m_latestChangelogId = latestChangelogId;
        m_apiKeys = apiKeys;
        const auto oldQuirks = m_apiQuirks;
        m_apiQuirks = gotApiQuirks ? apiQuirks.intersect(Core::knownApiQuirks())
                                   : Core::knownApiQuirks();

        m_pool.swap(pool);

        Color::s_colorImageCache.clear();

        if (generationDate != m_lastUpdated) {
            m_lastUpdated = generationDate;
            emit lastUpdatedChanged(generationDate);
        }
        if (!m_valid) {
            m_valid = true;
            emit validChanged(m_valid);
        }

        bool itemTypeInfo = false;
#if defined(QT_DEBUG)
        itemTypeInfo = true;
#endif
        bool apiQuirksInfo = (m_apiQuirks != oldQuirks);

        qInfo().noquote() << dumpDatabaseInformation(u"Loaded database from " + f.fileName(),
                                                     itemTypeInfo, apiQuirksInfo);
    } catch (const Exception &e) {
        if (m_valid) {
            m_valid = false;
            m_etag.clear(); // better re-download the db in this case
            emit validChanged(m_valid);
        }
        qWarning() << "Loading database failed:" << e.errorString();
        throw;
    }
}

QString Database::dumpDatabaseInformation(const QString &title, bool itemTypeInfo, bool apiQuirksInfo) const
{
    QVector<std::pair<QString, QString>> log = {
        { u"Generated at"_qs, m_lastUpdated.toString(u"dd MMM yyyy HH:mm:ss t") },
        { u"Changelog Id"_qs, QString::number(m_latestChangelogId).rightJustified(10) },
        { u"  Items"_qs,      QString::number(m_itemChangelog.size()).rightJustified(10) },
        { u"  Colors"_qs,     QString::number(m_colorChangelog.size()).rightJustified(10) },
        { u"Relationships"_qs,QString::number(m_relationships.size()).rightJustified(10) },
        { u"  Matches"_qs,    QString::number(m_relationshipMatches.size()).rightJustified(10) },
        { u"Item Types"_qs,   QString::number(m_itemTypes.size()).rightJustified(10) },
        { u"Categories"_qs,   QString::number(m_categories.size()).rightJustified(10) },
        { u"Colors"_qs,       QString::number(m_colors.size()).rightJustified(10) },
        { u"LDraw Colors"_qs, QString::number(m_ldrawExtraColors.size()).rightJustified(10) },
        { u"Items"_qs,        QString::number(m_items.size()).rightJustified(10) },
    };

    if (itemTypeInfo) {
        std::vector<int> itemCount(m_itemTypes.size());
        for (const auto &item : std::as_const(m_items))
            ++itemCount[item.m_itemTypeIndex];
        for (size_t i = 0; i < m_itemTypes.size(); ++i) {
            log.append({ u"  "_qs + m_itemTypes.at(i).name(),
                        QString::number(itemCount.at(i)).rightJustified(10) });
        }
    }

    QString out = title;
    qsizetype leftSize = 0;
    for (const auto &logPair : std::as_const(log))
        leftSize = std::max(leftSize, logPair.first.length());
    for (const auto &logPair : std::as_const(log))
        out = out + u"\n  " + logPair.first.leftJustified(leftSize) + u": " + logPair.second;

    if (apiQuirksInfo) {
        QStringList output;
        output.reserve(m_apiQuirks.size());
        for (auto apiQuirk : std::as_const(m_apiQuirks))
            output << Core::apiQuirkDescription(apiQuirk);
        output.sort();
        if (output.isEmpty())
            output << u"(None)"_qs;
        out = out + u"\n\nCurrently active BrickLink API quirks:\n  " + output.join(u"\n  ");
    }
    return out;
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

    cw.startChunk("BSDB", uint(version));

    cw.startChunk("DATE", 1);
    ds << QDateTime::currentDateTimeUtc();
    cw.endChunk();

    cw.startChunk("COL ", 1);
    ds << quint32(m_colors.size());
    for (const Color &col : m_colors)
        writeColorToDatabase(col, ds, version);
    cw.endChunk();

    if ((version >= Version::V7) && !m_ldrawExtraColors.empty()) {
        cw.startChunk("LCOL", 1);
        ds << quint32(m_ldrawExtraColors.size());
        for (const Color &col : m_ldrawExtraColors)
            writeColorToDatabase(col, ds, version);
        cw.endChunk();
    }

    cw.startChunk("CAT ", 1);
    ds << quint32(m_categories.size());
    for (const Category &cat : m_categories)
        writeCategoryToDatabase(cat, ds, version);
    cw.endChunk();

    cw.startChunk("TYPE", 1);
    ds << quint32(m_itemTypes.size());
    for (const ItemType &itt : m_itemTypes)
        writeItemTypeToDatabase(itt, ds, version);
    cw.endChunk();

    cw.startChunk("ITEM", 1);
    ds << quint32(m_items.size());
    for (const Item &item : m_items)
        writeItemToDatabase(item, ds, version);
    cw.endChunk();

    if (version >= Version::V9) {
        cw.startChunk("CHGL", 2);
        ds << quint32(m_latestChangelogId)
           << quint32(m_itemChangelog.size())
           << quint32(m_colorChangelog.size());
        for (const ItemChangeLogEntry &e : m_itemChangelog)
            writeItemChangeLogToDatabase(e, ds, version);
        for (const ColorChangeLogEntry &e : m_colorChangelog)
            writeColorChangeLogToDatabase(e, ds, version);
        cw.endChunk();
    } else {
        cw.startChunk("ICHG", 1);
        ds << quint32(m_itemChangelog.size());
        for (const ItemChangeLogEntry &e : m_itemChangelog)
            writeItemChangeLogToDatabase(e, ds, version);
        cw.endChunk();

        cw.startChunk("CCHG", 1);
        ds << quint32(m_colorChangelog.size());
        for (const ColorChangeLogEntry &e : m_colorChangelog)
            writeColorChangeLogToDatabase(e, ds, version);
        cw.endChunk();
    }

    if (version < Version::V11) {
        // we need to recalculate the pcc list used in older BS versions from the items
        std::vector<PartColorCode> oldPccs;
        for (const auto &item : m_items) {
            for (const auto &newPcc : item.pccs()) {
                PartColorCode oldPcc;
                oldPcc.m_id = newPcc.pcc();
                oldPcc.m_itemIndex = item.index();
                oldPcc.m_colorIndex = newPcc.m_colorIndex;
                oldPccs.push_back(oldPcc);
            }
        }
        std::sort(oldPccs.begin(), oldPccs.end());

        cw.startChunk("PCC ", 1);
        ds << quint32(oldPccs.size());
        for (const PartColorCode &pcc : std::as_const(oldPccs))
            writePCCToDatabase(pcc, ds, version);
        cw.endChunk();
    }

    if (version >= Version::V9) {
        cw.startChunk("REL ", 1);
        ds << quint32(m_relationships.size());
        for (const Relationship &rel : m_relationships)
            writeRelationshipToDatabase(rel, ds, version);
        cw.endChunk();

        cw.startChunk("RELM", 1);
        ds << quint32(m_relationshipMatches.size());
        for (const RelationshipMatch &match : m_relationshipMatches)
            writeRelationshipMatchToDatabase(match, ds, version);
        cw.endChunk();
    }

    if (version >= Version::V11) {
        cw.startChunk("AKEY", 1);
        ds << quint32(m_apiKeys.size());
        for (auto it = m_apiKeys.cbegin(); it != m_apiKeys.cend(); ++it)
            writeApiKeyToDatabase(it.key(), it.value(), ds, version);
        cw.endChunk();
    }

    if (version >= Version::V12) {
        cw.startChunk("QIRK", 1);
        ds << quint32(m_apiQuirks.size());
        for (const ApiQuirk apiQuirk : m_apiQuirks)
            ds << quint32(apiQuirk);
        cw.endChunk();
    }

    cw.endChunk(); // BSDB root chunk

    if (!f.commit())
        throw Exception(f.errorString());
}

void Database::remove()
{
    QString dbDir = core()->dataPath();
    QString dbNameFilter = defaultDatabaseName(Version::Invalid);
    dbNameFilter[dbNameFilter.length() - 1] = u'*';

    QDirIterator dit(dbDir, { dbNameFilter }, QDir::Files);
    while (dit.hasNext())
        QFile::remove(dit.next());
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


void Database::readColorFromDatabase(Color &col, QDataStream &dataStream, MemoryResource *pool)
{
    quint32 flags;

    dataStream >> col.m_id >> col.m_name.deserialize(pool) >> col.m_ldraw_id >> col.m_color
        >> flags >> col.m_popularity >> col.m_year_from >> col.m_year_to;
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


void Database::readCategoryFromDatabase(Category &cat, QDataStream &dataStream, MemoryResource *pool)
{
    dataStream >> cat.m_id >> cat.m_name.deserialize(pool) >> cat.m_year_from >> cat.m_year_to
        >> cat.m_year_recency >> cat.m_has_inventories;
}

void Database::writeCategoryToDatabase(const Category &cat, QDataStream &dataStream, Version v) const
{
    dataStream << cat.m_id << cat.m_name << cat.m_year_from << cat.m_year_to << cat.m_year_recency;
    if (v >= Version::V8)
        dataStream << cat.m_has_inventories;
}


void Database::readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, MemoryResource *pool)
{
    quint8 flags = 0;
    dataStream >> reinterpret_cast<qint8 &>(itt.m_id) >> itt.m_name.deserialize(pool) >> flags
        >> itt.m_categoryIndexes.deserialize(pool);

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
    dataStream << itt.name() << flags << itt.m_categoryIndexes;
}


void Database::readItemFromDatabase(Item &item, QDataStream &dataStream, MemoryResource *pool)
{
    quint16 itemTypeIndex;
    quint16 defaultColorIndex;

    dataStream
        >> item.m_id.deserialize(pool)
        >> item.m_name.deserialize(pool)
        >> itemTypeIndex
        >> defaultColorIndex
        >> item.m_year_from
        >> item.m_year_to
        >> item.m_weight;

    item.m_itemTypeIndex = itemTypeIndex;
    item.m_defaultColorIndex = defaultColorIndex;

    dataStream >> item.m_appears_in.deserialize(pool);
    dataStream >> item.m_consists_of.deserialize(pool);
    dataStream >> item.m_knownColorIndexes.deserialize(pool);
    dataStream >> item.m_categoryIndexes.deserialize(pool);
    dataStream >> item.m_relationshipMatchIds.deserialize(pool);
    dataStream >> item.m_dimensions.deserialize(pool);
    dataStream >> item.m_pccs.deserialize(pool);
    dataStream >> item.m_alternateIds.deserialize(pool);
}

void Database::writeItemToDatabase(const Item &item, QDataStream &dataStream, Version v) const
{
    dataStream << item.m_id
               << item.m_name
               << quint16(item.m_itemTypeIndex);

    if (v < Version::V10) {
        dataStream << item.m_categoryIndexes[0]
                   << quint16(item.m_defaultColorIndex)
                   << qint8(m_itemTypes[item.m_itemTypeIndex].id())
                   << item.m_year_from
                   << qint64(item.m_consists_of.isEmpty() ? 0 : 1577833200) // 01.01.2020
                   << item.m_weight << item.m_year_to
                   << item.m_appears_in
                   << item.m_consists_of
                   << item.m_knownColorIndexes;

        quint32 categoriesSize = quint32(std::max(qsizetype(1), item.m_categoryIndexes.size()) - 1);
        dataStream << categoriesSize;
        for (quint32 i = 0; i < categoriesSize; ++i)
            dataStream << item.m_categoryIndexes[i + 1];

    } else {
        dataStream << quint16(item.m_defaultColorIndex)
                   << item.m_year_from
                   << item.m_year_to
                   << item.m_weight
                   << item.m_appears_in
                   << item.m_consists_of
                   << item.m_knownColorIndexes
                   << item.m_categoryIndexes;
    }

    if (v >= Version::V9)
        dataStream << item.m_relationshipMatchIds;

    if (v >= Version::V10)
        dataStream << item.m_dimensions;

    if (v >= Version::V11)
        dataStream << item.m_pccs;

    if (v >= Version::V12)
        dataStream << item.m_alternateIds;
}

void Database::writePCCToDatabase(const PartColorCode &pcc, QDataStream &dataStream, Version) const
{
    dataStream << pcc.m_id << qint32(pcc.m_itemIndex) << qint32(pcc.m_colorIndex);
}

void Database::readItemChangeLogFromDatabase(ItemChangeLogEntry &e, QDataStream &dataStream, MemoryResource *pool)
{
    dataStream >> e.m_id >> e.m_julianDay >> e.m_fromTypeAndId.deserialize(pool)
        >> e.m_toTypeAndId.deserialize(pool);
}

void Database::writeItemChangeLogToDatabase(const ItemChangeLogEntry &e, QDataStream &dataStream, Version v) const
{
    if (v >= Version::V9)
        dataStream << e.m_id << e.m_julianDay;
    dataStream << e.m_fromTypeAndId << e.m_toTypeAndId;
}

void Database::readColorChangeLogFromDatabase(ColorChangeLogEntry &e, QDataStream &dataStream, MemoryResource *)
{
    dataStream >> e.m_id >> e.m_julianDay >> e.m_fromColorId >> e.m_toColorId;
}

void Database::writeColorChangeLogToDatabase(const ColorChangeLogEntry &e, QDataStream &dataStream, Version v) const
{
    if (v >= Version::V9)
        dataStream << e.m_id << e.m_julianDay;
    dataStream << e.m_fromColorId << e.m_toColorId;
}

void Database::readRelationshipFromDatabase(Relationship &e, QDataStream &dataStream, MemoryResource *pool)
{
    dataStream >> e.m_id >> e.m_name.deserialize(pool) >> e.m_count;
}

void Database::writeRelationshipToDatabase(const Relationship &e, QDataStream &dataStream, Version) const
{
    dataStream << e.m_id << e.m_name << e.m_count;
}

void Database::readRelationshipMatchFromDatabase(RelationshipMatch &match, QDataStream &dataStream, MemoryResource *pool)
{
    dataStream >> match.m_id >> match.m_relationshipId >> match.m_itemIndexes.deserialize(pool);
}

void Database::writeRelationshipMatchToDatabase(const RelationshipMatch &match, QDataStream &dataStream, Version) const
{
    dataStream << match.m_id << match.m_relationshipId << match.m_itemIndexes;
}

// This is minimal protection, but it's still better than storing the keys in plain text.
static QByteArray scramble(const QByteArray &array, Database::Version v)
{
    auto xorByteArray = [](const QByteArray &data, const QByteArray &key) -> QByteArray
    {
        QByteArray result;
        result.resize(data.size());
        for (int i = 0; i < data.size(); ++i)
            result[i] = data[i] ^ key[i % key.size()];
        return result;
    };

    // cache the scramble keys for faster DB reloading
    static QHash<Database::Version, QByteArray> scrambleKeys;
    QByteArray scrambleKey = scrambleKeys.value(v);
    if (scrambleKey.isEmpty()) {
        scrambleKey = "DBv" + QByteArray::number(int(v));
        scrambleKey = QCryptographicHash::hash(scrambleKey, QCryptographicHash::Blake2s_128);
        scrambleKeys.insert(v, scrambleKey);
    }
    return xorByteArray(array, scrambleKey);
}

void Database::readApiKeyFromDatabase(QByteArray &id, QString &key, QDataStream &dataStream,  MemoryResource *)
{
    QByteArray idScrambled, keyScrambled;
    dataStream >> idScrambled >> keyScrambled;

    id = scramble(idScrambled, Version::Latest);
    key = QString::fromUtf8(scramble(keyScrambled, Version::Latest));
}

void Database::writeApiKeyToDatabase(const QByteArray &id, const QString &key, QDataStream &dataStream, Version v) const
{
    QByteArray idScrambled = scramble(id, v);
    QByteArray keyScrambled = scramble(key.toUtf8(), v);

    dataStream << idScrambled << keyScrambled;
}

} // namespace BrickLink

//#include "moc_database.cpp" // QTBUG-98845
