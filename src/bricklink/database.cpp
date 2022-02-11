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
#include <cstdio>
#include <cstdlib>

#include <QFile>
#include <QBuffer>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QStringBuilder>
#include <QScopeGuard>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#  include <qhashfunctions.h>
#  define q5Hash qHash
#else
#  include "utility/q5hashfunctions.h"
#endif
#include "utility/utility.h"
#include "utility/stopwatch.h"
#include "utility/chunkreader.h"
#include "utility/chunkwriter.h"
#include "utility/exception.h"
#include "utility/transfer.h"
#include "version.h"

#include "bricklink/core.h"
#include "bricklink/database.h"

#include "lzma/bs_lzma.h"


namespace BrickLink {


Database::Database(QObject *parent)
    : QObject(parent)
    , m_transfer(new Transfer(this))
{ }

Database::~Database()
{
    clear();
}

bool Database::isUpdateNeeded() const
{
    return (m_updateInterval > 0)
            && (!m_valid || (m_lastUpdated.secsTo(QDateTime::currentDateTime()) > m_updateInterval));
}

QString Database::defaultDatabaseName(Version version)
{
    return u"database-v" % QString::number(int(version));
}

void Database::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}

void Database::updateSuccessfull(const QDateTime &dt)
{
    if (dt != m_lastUpdated) {
        m_lastUpdated = dt;
        emit lastUpdatedChanged(dt);
    }
    if (!m_valid) {
        m_valid = true;
        emit validChanged(m_valid);
    }
}

void Database::updateFailed()
{
    if (m_valid) {
        m_valid = false;
        emit validChanged(m_valid);
    }
}

void Database::clear()
{
    m_colors.clear();
    m_itemTypes.clear();
    m_categories.clear();
    m_items.clear();
    m_pccs.clear();
    m_itemChangelog.clear();
    m_colorChangelog.clear();
}

bool Database::startUpdate()
{
    return startUpdate(true);
}

bool Database::startUpdate(bool force)
{
    if (updateStatus() == UpdateStatus::Updating)
        return false;

    QString dbName = defaultDatabaseName();
    QString remotefile = BRICKSTORE_DATABASE_URL ""_l1 % dbName % u".lzma";
    QString localfile = core()->dataPath() % dbName;

    QDateTime dt;
    if (!force && QFile::exists(localfile))
        dt = m_lastUpdated.addSecs(60 * 5);

    auto file = new QSaveFile(localfile);
    auto lzma = new LZMA::DecompressFilter(file);
    auto hhc = new HashHeaderCheckFilter(lzma);
    lzma->setParent(hhc);
    file->setParent(lzma);

    TransferJob *job = nullptr;
    if (hhc->open(QIODevice::WriteOnly)) {
        job = TransferJob::getIfNewer(QUrl(remotefile), dt, hhc);
        m_transfer->retrieve(job);
    }
    if (!job) {
        delete hhc;
        return false;
    }

    setUpdateStatus(UpdateStatus::Updating);

    emit databaseAboutToBeReset();

    connect(m_transfer, &Transfer::started,
            this, [this, job](TransferJob *j) {
        if (j != job)
            return;
        emit updateStarted();
    });
    connect(m_transfer, &Transfer::progress,
            this, [this, job](TransferJob *j, int done, int total) {
        if (j != job)
            return;
        emit updateProgress(done, total);
    });
    connect(m_transfer, &Transfer::finished,
            this, [this, job, hhc, file](TransferJob *j) {
        if (j != job)
            return;

        hhc->close(); // does not close/commit the QSaveFile
        hhc->deleteLater();

        try {
            if (!job->isFailed() && job->wasNotModifiedSince()) {
                emit updateFinished(true, tr("Already up-to-date."));
                setUpdateStatus(UpdateStatus::Ok);
            } else if (job->isFailed()) {
                throw Exception(tr("Failed to download and decompress the database") % u": "
                                % job->errorString());
            } else if (!hhc->hasValidChecksum()) {
                throw Exception(tr("Checksum mismatch after decompression"));
            } else if (!file->commit()) {
                throw Exception(tr("Could not save the database") % u": " % file->errorString());
            } else {
                read(file->fileName());

                emit updateFinished(true, { });
                setUpdateStatus(UpdateStatus::Ok);
            }
            emit databaseReset();

        } catch (const Exception &e) {
            emit updateFinished(false, tr("Could not load the new database:") % u"\n\n" % e.error());
            setUpdateStatus(UpdateStatus::UpdateFailed);
        }
    });
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
        stopwatch *sw = nullptr; //new stopwatch("Database::read()");

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
        bool gotItemChangeLog = false, gotColorChangeLog = false, gotPccs = false;

        auto check = [&ds, &f]() {
            if (ds.status() != QDataStream::Ok)
                throw Exception("failed to read from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        auto sizeCheck = [&f](int s, int max) {
            if (s > max)
                throw Exception("failed to read from database (%1) at position %2: size value %L3 is larger than expected maximum %L4")
                    .arg(f.fileName()).arg(f.pos()).arg(s).arg(max);
        };

        QDateTime                        generationDate;
        std::vector<Color>               colors;
        std::vector<Category>            categories;
        std::vector<ItemType>            itemTypes;
        std::vector<Item>                items;
        std::vector<ItemChangeLogEntry>  itemChangelog;
        std::vector<ColorChangeLogEntry> colorChangelog;
        std::vector<PartColorCode>       pccs;

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
            case ChunkId('I','C','H','G') | 1ULL << 32: {
                quint32 clc = 0;
                ds >> clc;
                check();
                sizeCheck(clc, 1'000'000);

                itemChangelog.resize(clc);
                for (quint32 i = 0; i < clc; ++i) {
                    readItemChangeLogFromDatabase(itemChangelog[i], ds, Version::Latest);
                    check();
                }
                gotItemChangeLog = true;
                break;
            }
            case ChunkId('C','C','H','G') | 1ULL << 32: {
                quint32 clc = 0;
                ds >> clc;
                check();
                sizeCheck(clc, 1'000);

                colorChangelog.resize(clc);
                for (quint32 i = 0; i < clc; ++i) {
                    readColorChangeLogFromDatabase(colorChangelog[i], ds, Version::Latest);
                    check();
                }
                gotColorChangeLog = true;
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

        if (!gotColors || !gotCategories || !gotItemTypes || !gotItems || !gotItemChangeLog
                || !gotColorChangeLog || !gotPccs) {
            throw Exception("not all required data chunks were found in the database (%1)")
                .arg(f.fileName());
        }

        qInfo().noquote() << "Loaded database from" << f.fileName()
                          << "\n  Generated at:" << generationDate.toString(Qt::RFC2822Date)
                          << "\n  Colors      :" << colors.size()
                          << "\n  Item Types  :" << itemTypes.size()
                          << "\n  Categories  :" << categories.size()
                          << "\n  Items       :" << items.size()
                          << "\n  PCCs        :" << pccs.size()
                          << "\n  ChangeLog I :" << itemChangelog.size()
                          << "\n  ChangeLog C :" << colorChangelog.size();

        m_colors = colors;
        m_categories = categories;
        m_itemTypes = itemTypes;
        m_items = items;
        m_itemChangelog = itemChangelog;
        m_colorChangelog = colorChangelog;
        m_pccs = pccs;

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
            emit validChanged(m_valid);
        }
        throw;
    }
}

void Database::write(const QString &filename, Version version) const
{
    if (version <= Version::Invalid)
        throw Exception(tr("Version %1 is too old").arg(int(version)));

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

    if (version >= Version::V5) {
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
            ds << static_cast<QByteArray>("\x03\t" % QByteArray(1, e.fromItemTypeId()) % '\t' % e.fromItemId()
                                          % '\t' % e.toItemTypeId() % '\t' % e.toItemId());
        }
        for (const ColorChangeLogEntry &e : m_colorChangelog) {
            ds << static_cast<QByteArray>("\x07\t" % QByteArray::number(e.fromColorId()) % "\tx\t"
                                          % QByteArray::number(e.toColorId()) % "\tx");
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
    col.m_type = static_cast<Color::Type>(flags);
}

void Database::writeColorToDatabase(const Color &col, QDataStream &dataStream, Version) const
{
    dataStream << col.m_id << col.m_name << col.m_ldraw_id << col.m_color << quint32(col.m_type)
               << col.m_popularity << col.m_year_from << col.m_year_to;
}


void Database::readCategoryFromDatabase(Category &cat, QDataStream &dataStream, Version)
{
    dataStream >> cat.m_id >> cat.m_name >> cat.m_year_from >> cat.m_year_to >> cat.m_year_recency;
}

void Database::writeCategoryToDatabase(const Category &cat, QDataStream &dataStream, Version v) const
{
    dataStream << cat.m_id << cat.m_name;
    if (v >= Version::V6)
        dataStream << cat.m_year_from << cat.m_year_to << cat.m_year_recency;
}


void Database::readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, Version)
{
    qint8 dummy = 0; //TODO: legacy, remove in one of the next DB upgrades
    quint8 flags = 0;
    quint32 catSize = 0;
    dataStream >> (qint8 &) itt.m_id >> dummy >> itt.m_name >> flags >> catSize;

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

void Database::writeItemTypeToDatabase(const ItemType &itt, QDataStream &dataStream, Version) const
{
    quint8 flags = 0;
    flags |= (itt.m_has_inventories   ? 0x01 : 0);
    flags |= (itt.m_has_colors        ? 0x02 : 0);
    flags |= (itt.m_has_weight        ? 0x04 : 0);
    flags |= (true                    ? 0x08 : 0); // was: m_has_year
    flags |= (itt.m_has_subconditions ? 0x10 : 0);

    dataStream << qint8(itt.m_id) << qint8(itt.m_id) << itt.m_name << flags
               << quint32(itt.m_categoryIndexes.size());

    for (const quint16 catIdx : itt.m_categoryIndexes)
        dataStream << catIdx;
}


void Database::readItemFromDatabase(Item &item, QDataStream &dataStream, Version)
{
    dataStream >> item.m_id >> item.m_name >> item.m_itemTypeIndex >> item.m_categoryIndex
            >> item.m_defaultColorIndex >> (qint8 &) item.m_itemTypeId >> item.m_year_from
            >> item.m_lastInventoryUpdate >> item.m_weight >> item.m_year_to;

    quint32 appearsInSize = 0;
    dataStream >> appearsInSize;
    item.m_appears_in.clear();
    item.m_appears_in.reserve(appearsInSize);

    union {
        quint32 ui32;
        Item::AppearsInRecord ai;
    } appearsInUnion;

    while (appearsInSize-- > 0) {
        dataStream >> appearsInUnion.ui32;
        item.m_appears_in.push_back(appearsInUnion.ai);
    }
    item.m_appears_in.shrink_to_fit();

    quint32 consistsOfSize = 0;
    dataStream >> consistsOfSize;
    item.m_consists_of.clear();
    item.m_consists_of.reserve(consistsOfSize);

    union {
        quint64 ui64;
        Item::ConsistsOf co;
    } consistsOfUnion;

    while (consistsOfSize-- > 0) {
        dataStream >> consistsOfUnion.ui64;
        item.m_consists_of.push_back(consistsOfUnion.co);
    }
    item.m_consists_of.shrink_to_fit();

    quint32 knownColorsSize;
    dataStream >> knownColorsSize;
    item.m_knownColorIndexes.clear();
    item.m_knownColorIndexes.reserve(knownColorsSize);

    while (knownColorsSize-- > 0) {
        quint16 colorIndex;
        dataStream >> colorIndex;
        item.m_knownColorIndexes.push_back(colorIndex);
    }
    item.m_knownColorIndexes.shrink_to_fit();

    quint32 additonalCategoriesSize;
    dataStream >> additonalCategoriesSize;
    item.m_additionalCategoryIndexes.clear();
    item.m_additionalCategoryIndexes.reserve(additonalCategoriesSize);

    while (additonalCategoriesSize-- > 0) {
        qint16 catIndex;
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

    union {
        quint32 ui32;
        Item::AppearsInRecord ai;
    } appearsInUnion;

    dataStream << quint32(item.m_appears_in.size());
    for (const auto &ai : item.m_appears_in) {
        appearsInUnion.ai = ai;
        dataStream << appearsInUnion.ui32;
    }

    dataStream << quint32(item.m_consists_of.size());
    union {
        quint64 ui64;
        Item::ConsistsOf co;
    } consistsOfUnion;
    for (const auto &co : item.m_consists_of) {
        consistsOfUnion.co = co;
        dataStream << consistsOfUnion.ui64;
    }

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
    dataStream >> e.m_fromTypeAndId >> e.m_toTypeAndId;
}

void Database::writeItemChangeLogToDatabase(const ItemChangeLogEntry &e, QDataStream &dataStream, Version) const
{
    dataStream << e.m_fromTypeAndId << e.m_toTypeAndId;
}

void Database::readColorChangeLogFromDatabase(ColorChangeLogEntry &e, QDataStream &dataStream, Version)
{
    dataStream >> e.m_fromColorId >> e.m_toColorId;
}

void Database::writeColorChangeLogToDatabase(const ColorChangeLogEntry &e, QDataStream &dataStream, Version) const
{
    dataStream << e.m_fromColorId << e.m_toColorId;
}



} // namespace BrickLink

//#include "moc_database.cpp"
