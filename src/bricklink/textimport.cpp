// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QBitArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QStringBuilder>
#include <QtCore/QTextStream>
#include <QtCore/QTimeZone>
#include <QtCore/QtDebug>
#include <QtCore/QDirIterator>
#include <QtCore/QUrlQuery>
#include <QtCore/QXmlStreamReader>

#include <QCoro/QCoroSignal>
#include <QCoro/QCoroTimer>

#include "bricklink/changelogentry.h"
#include "bricklink/core.h"
#include "bricklink/dimensions.h"
#include "bricklink/textimport.h"
#include "bricklink/textimport_p.h"
#include "minizip/minizip.h"
#include "utility/exception.h"
#include "utility/transfer.h"


AwaitableTransferJob::AwaitableTransferJob(TransferJob *job)
    : m_job(job)
{
    connect(BrickLink::core(), &BrickLink::Core::authenticatedTransferFinished,
            this, [this](TransferJob *finishedJob) {
        if (m_job == finishedJob) {
            m_job->setAutoDelete(false);
            emit finished();
        }
    });
}

AwaitableTransferJob::~AwaitableTransferJob()
{
    if (!m_job->autoDelete())
        delete m_job;
}

AwaitableTransferJob::operator TransferJob *() const
{
    return m_job;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


static QUrl ldrawUrl(const char *page)
{
    return QUrl(u"https://library.ldraw.org/"_qs + QString::fromLatin1(page));
};

static QUrl brickLinkUrl(const char *page)
{
    return QUrl(u"https://www.bricklink.com/"_qs + QString::fromLatin1(page));
};

static QUrl catalogQuery(int which)
{
    QUrl url(brickLinkUrl("catalogDownload.asp"));
    url.setQuery({
        { u"a"_qs,            u"a"_qs },
        { u"viewType"_qs,     QString::number(which) },
        { u"downloadType"_qs, u"X"_qs },
    });
    return url;
};

static QUrl catalogItemQuery(char itemType, bool asXml)
{
    QUrl url(brickLinkUrl("catalogDownload.asp"));
    QUrlQuery query({
        { u"a"_qs,            u"a"_qs },
        { u"viewType"_qs,     u"0"_qs },
        { u"itemType"_qs,     QString(QChar::fromLatin1(itemType)) },
        { u"downloadType"_qs, asXml ? u"X"_qs : u"T"_qs },
    });
    if (asXml) {
        query.addQueryItem(u"selItemColor"_qs, u"Y"_qs);  // special BrickStore flag to get default color - thanks Dan
        query.addQueryItem(u"selWeight"_qs,    u"Y"_qs);
        query.addQueryItem(u"selYear"_qs,      u"Y"_qs);
    }
    url.setQuery(query);
    return url;
};

static QUrl itemInventoryQuery(const BrickLink::Item *item)
{
    QUrl url(brickLinkUrl("catalogDownload.asp"));
    url.setQuery({
        { u"a"_qs,            u"a"_qs },
        { u"viewType"_qs,     u"4"_qs },
        { u"itemTypeInv"_qs,  QString(QChar::fromLatin1(item->itemTypeId())) },
        { u"itemNo"_qs,       QString::fromLatin1(item->id()) },
        { u"downloadType"_qs, u"X"_qs },
    });
    return url;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


namespace BrickLink {

TextImport::TextImport()
    : m_db(core()->database())
{
    m_archiveName = core()->dataPath() + u"downloads.zip";
}

TextImport::~TextImport()
{ }

void TextImport::initialize(bool skipDownload)
{
    m_skipDownload = skipDownload;

    nextStep(u"Initializing"_qs);

    if (skipDownload) {
        m_lastRunArchive = std::make_unique<MiniZip>(m_archiveName);
        if (!m_lastRunArchive->open()) {
            throw Exception("Last-run archive %1 is not available and skip-downloads was requested")
                .arg(m_archiveName);
        }
        message(u"Skipping downloads. Using last-run archive %1"_qs.arg(m_archiveName));
    } else {
        QString downloadArchiveName = m_archiveName + u".new";
        m_downloadArchive = std::make_unique<MiniZip>(downloadArchiveName);

        QFile::remove(downloadArchiveName);
        if (!m_downloadArchive->create())
            throw Exception("failed to create inventory cache zip %1").arg(downloadArchiveName);

        m_lastRunArchive = std::make_unique<MiniZip>(m_archiveName);
        if (!m_lastRunArchive->open()) {
            message(u"Last-run archive %1 is not available"_qs.arg(m_archiveName));
            m_lastRunArchive.reset();
        }
    }
}

QCoro::Task<> TextImport::login(const QString &username, const QString &password,
                                const QString &rebrickableApiKey)
{
    m_rebrickableApiKey = rebrickableApiKey;

    if (m_skipDownload)
        co_return;

    nextStep(u"Logging into BrickLink"_qs);

    core()->setCredentials({ username, password });
    if (!core()->isAuthenticated()) {
        core()->authenticate();

        auto [usernameAuth, error] = co_await qCoro(core(), &Core::authenticationFinished);
        Q_ASSERT(usernameAuth == username);
        if (!error.isEmpty())
            throw Exception("Failed to log into BrickLink:\n%1").arg(error);
    }
}

QCoro::Task<> TextImport::importCatalog()
{
    auto rebrickableApiQuery = [this](const char *page) -> QUrl {
        QUrl url(u"https://rebrickable.com/api/v3/"_qs + QString::fromLatin1(page));
        url.setQuery({
            { u"page_size"_qs,    u"1000"_qs },
            { u"key"_qs,          m_rebrickableApiKey },
        });
        return url;
    };

    nextStep(u"Importing catalog"_qs);

    co_await download(catalogQuery(3), u"colors.xml"_qs).then(
        [this](auto data) { readColors(data); });

    auto rebrickableData = co_await download(rebrickableApiQuery("lego/colors/"), u"rebrickable_colors.json"_qs);
    auto ldrawData = co_await download(ldrawUrl("library/official/LDConfig.ldr"), u"ldconfig.ldr"_qs);
    readLDrawColors(ldrawData, rebrickableData);

    co_await download(catalogQuery(2), u"categories.xml"_qs).then(
        [this](QByteArray data) { readCategories(data); });

    co_await download(catalogQuery(1), u"itemtypes.xml"_qs).then(
        [this](QByteArray data) { readItemTypes(data); });

    m_db->m_items.reserve(200'000);

    for (const ItemType &itt : std::as_const(m_db->m_itemTypes)) {
        co_await download(catalogItemQuery(itt.id(), true), u"items/%1.xml"_qs.arg(itt.id())).then(
            [this, &itt](QByteArray data) { readItems(data, &itt); });
        co_await download(catalogItemQuery(itt.id(), false), u"items/%1.csv"_qs.arg(itt.id())).then(
            [this, &itt](QByteArray data) { readAdditionalItemCategories(data, &itt); });
    }

    co_await download(brickLinkUrl("catalogRel.asp"), u"relationships.html"_qs).then(
        [this](QByteArray data) -> QCoro::Task<> { co_await readRelationships(data); });

    co_await download(catalogQuery(5), u"part_color_codes.xml"_qs).then(
        [this](QByteArray data) { readPartColorCodes(data); });

    co_await download(brickLinkUrl("btinvlist.asp"), u"btinvlist.csv"_qs).then(
        [this](QByteArray data) { readInventoryList(data); });

    co_await download(brickLinkUrl("btchglog.asp"), u"btchglog.csv"_qs).then(
        [this](QByteArray data) { readChangeLog(data); });
}

QCoro::Task<> TextImport::importInventories()
{
    nextStep(u"Importing inventories"_qs);

    QBitArray done(qsizetype(m_db->m_items.size()));
    for (uint itemIndex = 0; itemIndex < done.size(); ++itemIndex) {
        // mark everything without an inventory as done
        if (m_inventoryLastUpdated.value(itemIndex, -1) < 0)
            done.setBit(qsizetype(itemIndex), true);
    }

    uint loaded = loadLastRunInventories([&, this](char itemTypeId, const QByteArray &itemId,
                                                  const QDateTime &lastModified, const QByteArray &xml) {
        if (auto item = readInventory(itemTypeId, itemId, lastModified, xml, true)) {
            if (m_downloadArchive && m_downloadArchive->isOpen()) {
                const QString filePath = u"%1/%2.xml"_qs.arg(item->itemTypeId()).arg(QLatin1StringView(item->id()));
                try {
                    m_downloadArchive->writeFile(filePath, xml, lastModified);
                } catch (const Exception &e) {
                    message(2, u"Failed to write inventory for %1/%2 to ZIP: %3"_qs
                                   .arg(item->itemTypeId()).arg(QLatin1StringView(item->id()))
                                   .arg(e.errorString()));
                }
            }
            done.setBit(item->index(), true);
        }
    });

    message(u"Loaded %1 inventories, %2 are missing or outdated"_qs.arg(loaded).arg(done.count(false)));

    int downloadsInProgress = 0;
    int downloadsFailed = 0;
    for (uint i = 0; i < m_db->m_items.size(); ++i) {
        const BrickLink::Item *item = &m_db->m_items[i];
        if (!done[i]) {
            const QString filePath = u"%1/%2.xml"_qs.arg(item->itemTypeId()).arg(QLatin1StringView(item->id()));
            ++downloadsInProgress;
            download(itemInventoryQuery(item), filePath).then(
                [&, item](QByteArray data) {
                    if (!readInventory(item->itemTypeId(), item->id(), QDateTime::currentDateTime(), data)) {
                        message(2, u"Failed to import downloaded inventory for %1 %2"_qs
                                       .arg(item->itemTypeId()).arg(QLatin1StringView(item->id())));
                        ++downloadsFailed;
                    }
                    --downloadsInProgress;
                },
                [&](const std::exception &e) {
                    message(2, QString::fromLatin1(e.what()));
                    --downloadsInProgress;
                    ++downloadsFailed;
                });
        }
    }

    while (downloadsInProgress)
        co_await QCoro::sleepFor(std::chrono::milliseconds(100));

    if (downloadsFailed)
        throw Exception("Failed to download %1 inventories").arg(downloadsFailed);
}

void TextImport::finalize()
{
    if (!m_skipDownload) {
        Q_ASSERT(m_downloadArchive && m_downloadArchive->isOpen());
        m_downloadArchive->close();

        nextStep(u"Writing download cache"_qs);

        // close and rename both archives
        if (m_lastRunArchive && m_lastRunArchive->isOpen()) {
            m_lastRunArchive->close();
            QFile::remove(m_archiveName + u".old");
            QFile::rename(m_archiveName, m_archiveName + u".old");
        }
        QFile::remove(m_archiveName);
        QFile::rename(m_archiveName + u".new", m_archiveName);
    }

    nextStep(u"Optimizing data structures"_qs);

    calculateColorPopularity();
    calculateKnownAssemblyColors();
    calculateItemTypeCategories();
    calculatePartsYearUsed();
    calculateCategoryRecency();

    // unroll the consists-of and appears-in hashes into the actual items
    for (auto it = m_consists_of_hash.cbegin(); it != m_consists_of_hash.cend(); ++it) {
        Item &item = m_db->m_items[it.key()];
        const auto &coItems = it.value();
        item.m_consists_of.copyContainer(coItems.cbegin(), coItems.cend(), nullptr);
    }
    for (auto it = m_appears_in_hash.cbegin(); it != m_appears_in_hash.cend(); ++it) {
        Item &item = m_db->m_items[it.key()];

        // color-idx -> { vector < qty, item-idx > }
        const QHash<uint, QVector<QPair<uint, uint>>> &appearHash = it.value();

        // we are compacting a "hash of a vector of pairs" down to a list of 32bit integers
        QVector<Item::AppearsInRecord> tmp;

        for (auto ait = appearHash.cbegin(); ait != appearHash.cend(); ++ait) {
            const auto &colorVector = ait.value();

            Item::AppearsInRecord cair;
            cair.m_colorBits.m_colorIndex = ait.key();
            cair.m_colorBits.m_colorSize = quint32(colorVector.size());
            tmp.push_back(cair);

            for (const auto &[qty, itemIndex] : colorVector) {
                Item::AppearsInRecord iair;
                iair.m_itemBits.m_quantity = qty;
                iair.m_itemBits.m_itemIndex = itemIndex;
                tmp.push_back(iair);
            }
        }
        item.m_appears_in.copyContainer(tmp.cbegin(), tmp.cend(), nullptr);
    }

    m_db->m_lastUpdated = QDateTime::currentDateTime();
    message(0, m_db->dumpDatabaseInformation({ }, true, true));
}

void TextImport::exportDatabase()
{
    nextStep(u"Writing the database to disk"_qs);

    auto dbVersionLowest = Database::Version::OldestStillSupported;
    auto dbVersionHighest = Database::Version::Latest;

    Q_ASSERT(dbVersionHighest >= dbVersionLowest);

    for (auto v = dbVersionHighest; v >= dbVersionLowest; v = Database::Version((int(v) - 1))) {
        message(u"Version v%1"_qs.arg(int(v)));
        core()->database()->write(core()->dataPath() + Database::defaultDatabaseName(v), v);
    }
}

void TextImport::setApiKeys(const QHash<QByteArray, QString> &apiKeys)
{
    m_db->m_apiKeys = apiKeys;
}

void TextImport::setApiQuirks(const QSet<ApiQuirk> &apiQuirks)
{
    m_db->m_apiQuirks = apiQuirks;
}

QCoro::Task<QByteArray> TextImport::download(const QUrl &url, const QString &fileName)
{
    if (m_skipDownload) {
        if (!m_lastRunArchive || !m_lastRunArchive->isOpen())
            throw Exception("No last-run archive to load %1 from").arg(fileName);
        if (!m_lastRunArchive->contains(fileName))
            throw Exception("Last-run archive does not contain %1").arg(fileName);
        message(fileName);
        co_return m_lastRunArchive->readFile(fileName);
    }

    TransferJob *job = TransferJob::get(url.toString(QUrl::RemoveQuery), QUrlQuery(url));
    core()->retrieveAuthenticated(job);
    AwaitableTransferJob atj(job);

    QString fileNameCopy = fileName;

    co_await qCoro(&atj, &AwaitableTransferJob::finished);

    if (!job->isCompleted()) {
        throw Exception("Download failed for %1: %2")
            .arg(job->effectiveUrl().toString())
            .arg(job->errorString());
    } else if (job->data().isEmpty()) {
        throw Exception("Download has no data for %1").arg(job->effectiveUrl().toString());
    } else {
        if (!fileNameCopy.isEmpty() && m_downloadArchive && m_downloadArchive->isOpen())
            m_downloadArchive->writeFile(fileNameCopy, job->data());
        message(fileNameCopy);
        co_return job->data();
    }
}

void TextImport::readColors(const QByteArray &xml)
{
    xmlParse(xml, u"CATALOG", u"ITEM", [this](const auto &e) {
        Color col;
        uint colid = xmlTagText(e, u"COLOR").toUInt();

        col.m_id       = colid;
        col.m_name.copyQString(xmlTagText(e, u"COLORNAME").simplified(), nullptr);
        col.m_color    = QColor(u'#' + xmlTagText(e, u"COLORRGB"));

        col.m_ldraw_id = -1;
        col.m_type     = ColorType();

        auto type = xmlTagText(e, u"COLORTYPE");
        if (type.contains(u"Transparent")) col.m_type |= ColorTypeFlag::Transparent;
        if (type.contains(u"Glitter"))     col.m_type |= ColorTypeFlag::Glitter;
        if (type.contains(u"Speckle"))     col.m_type |= ColorTypeFlag::Speckle;
        if (type.contains(u"Metallic"))    col.m_type |= ColorTypeFlag::Metallic;
        if (type.contains(u"Chrome"))      col.m_type |= ColorTypeFlag::Chrome;
        if (type.contains(u"Pearl"))       col.m_type |= ColorTypeFlag::Pearl;
        if (type.contains(u"Milky"))       col.m_type |= ColorTypeFlag::Milky;
        if (type.contains(u"Modulex"))     col.m_type |= ColorTypeFlag::Modulex;
        if (type.contains(u"Satin"))       col.m_type |= ColorTypeFlag::Satin;
        if (!col.m_type)
            col.m_type = ColorTypeFlag::Solid;

        int partCnt    = xmlTagText(e, u"COLORCNTPARTS").toInt();
        int setCnt     = xmlTagText(e, u"COLORCNTSETS").toInt();
        int wantedCnt  = xmlTagText(e, u"COLORCNTWANTED").toInt();
        int forSaleCnt = xmlTagText(e, u"COLORCNTINV").toInt();

        col.m_popularity = float(partCnt + setCnt + wantedCnt + forSaleCnt);

        // needs to be divided by the max. after all colors are parsed!
        // mark it as raw data meanwhile:
        col.m_popularity = -col.m_popularity;

        col.m_year_from = xmlTagText(e, u"COLORYEARFROM").toUShort();
        col.m_year_to   = xmlTagText(e, u"COLORYEARTO").toUShort();

        m_db->m_colors.push_back(col);
    });

    std::sort(m_db->m_colors.begin(), m_db->m_colors.end());
}

void TextImport::readCategories(const QByteArray &xml)
{
    xmlParse(xml, u"CATALOG", u"ITEM", [this](const auto &e) {
        Category cat;
        uint catid = xmlTagText(e, u"CATEGORY").toUInt();

        cat.m_id   = catid;
        cat.m_name.copyQString(xmlTagText(e, u"CATEGORYNAME").simplified(), nullptr);

        m_db->m_categories.push_back(cat);
    });

    std::sort(m_db->m_categories.begin(), m_db->m_categories.end());
}

void TextImport::readItemTypes(const QByteArray &xml)
{
    xmlParse(xml, u"CATALOG", u"ITEM", [this](const auto &e) {
        ItemType itt;
        char c = ItemType::idFromFirstCharInString(xmlTagText(e, u"ITEMTYPE"));

        if (c == 'U')
            return;

        itt.m_id   = c;
        itt.m_name.copyQString(xmlTagText(e, u"ITEMTYPENAME").simplified(), nullptr);

        itt.m_has_inventories   = false;
        itt.m_has_colors        = (c == 'P' || c == 'G');
        itt.m_has_weight        = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
        itt.m_has_subconditions = (c == 'S');

        m_db->m_itemTypes.push_back(itt);
    });

    std::sort(m_db->m_itemTypes.begin(), m_db->m_itemTypes.end());
}

void TextImport::readItems(const QByteArray &xml, const ItemType *itt)
{
    xmlParse(xml, u"CATALOG", u"ITEM", [this, itt](const auto &e) {
        Item item;
        item.m_id.copyQByteArray(xmlTagText(e, u"ITEMID").toLatin1(), nullptr);
        const QString itemName = xmlTagText(e, u"ITEMNAME").simplified();
        item.m_name.copyQString(itemName, nullptr);
        item.m_itemTypeIndex = quint16(itt - m_db->m_itemTypes.data());

        uint catId = xmlTagText(e, u"CATEGORY").toUInt();
        auto cat = core()->category(catId);
        if (!cat)
            throw ParseException("item %1 has no category").arg(QString::fromLatin1(item.id()));
        item.m_categoryIndexes.push_back(quint16(cat->index()), nullptr);

        QByteArray altIds = xmlTagText(e, u"ALTITEMIDS", true).toLatin1().replace(',', ' ').simplified();
        if (!altIds.isEmpty())
            item.m_alternateIds.copyQByteArray(altIds, nullptr);

        uint y = xmlTagText(e, u"ITEMYEAR", true).toUInt();
        item.m_year_from = quint8(((y > 1900) && (y < 2155)) ? (y - 1900) : 0); // we only have 8 bits for the year
        item.m_year_to = item.m_year_from;

        if (itt->hasWeight())
            item.m_weight = xmlTagText(e, u"ITEMWEIGHT").toFloat();
        else
            item.m_weight = 0;

        try {
            auto color = core()->color(xmlTagText(e, u"IMAGECOLOR").toUInt());
            item.m_defaultColorIndex = !color ? quint16(0xfff) : quint16(color->index());
        } catch (...) {
            item.m_defaultColorIndex = quint16(0xfff);
        }

        // extract dimensions
        if (itemName.contains(u" x ")) {
            std::vector<Dimensions> dims;
            qsizetype offset = 0;
            while (true) {
                const auto dim = Dimensions::parseString(itemName, offset, Dimensions::Strict);
                if (!dim.isValid())
                    break;
                dims.push_back(dim);
                offset = dim.offset() + dim.length();
            }
            if (!dims.empty())
                item.m_dimensions.copyContainer(dims.cbegin(), dims.cend(), nullptr);
        }

        m_db->m_items.push_back(item);
    });

    std::sort(m_db->m_items.begin(), m_db->m_items.end());
}

void TextImport::readAdditionalItemCategories(const QByteArray &csv, const ItemType *itt)
{
    // static is not ideal, but we want to re-use this cache for all item types
    static QHash<QString, uint> catHash;
    if (catHash.isEmpty()) {
        for (const auto &cat : std::as_const(core()->categories()))
            catHash.insert(cat.name(), cat.index());
    }

    const QString fname = u"items/%1.csv"_qs.arg(QChar::fromLatin1(itt->id()));
    QTextStream ts(csv);
    ts.readLine(); // skip header
    int lineNumber = 1;
    QByteArray lastItemId;

    while (!ts.atEnd()) {
        ++lineNumber;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        if (line.startsWith(u'\t')) {
            message(2, u"Item name for %1 most likely ends with a new-line character: %2 in line %3"_qs
                           .arg(QLatin1StringView { lastItemId }).arg(fname).arg(lineNumber -1));
            continue;
        }
        QStringList strs = line.split(u'\t');

        if (strs.count() < 3)
            throw Exception("expected at least 3 fields in %1, line %2").arg(fname).arg(lineNumber);

        const QByteArray itemId = strs.at(2).toLatin1();
        auto item = const_cast<Item *>(core()->item(itt->m_id, itemId));
        if (!item)
            throw Exception("expected a valid item-id field in %1, line %2").arg(fname).arg(lineNumber);

        QString catStr = strs.at(1).simplified();
        const Category &mainCat = m_db->m_categories.at(item->m_categoryIndexes[0]);
        if (!catStr.startsWith(mainCat.name())) {
            throw Exception( "additional categories do not start with the main category in %1, line %2")
                .arg(fname).arg(lineNumber);
        }
        if (strs.at(0).toUInt() != mainCat.m_id) {
            throw Exception("the main category id does not match the XML in %1, line %2")
                .arg(fname).arg(lineNumber);
        }

        catStr = catStr.mid(mainCat.name().length() + 3);
        QVector<quint16> addCatIndexes(1, item->m_categoryIndexes[0]);

        if (!catStr.isEmpty()) {
            const auto cats = QStringView { catStr }.split(u" / ");

            for (int i = 0; i < cats.count(); ++i) {
                QStringView catName = cats.at(i);
                uint catIndex = catHash.value(catName.toString(), Category::InvalidId);

                if (catIndex == Category::InvalidId)
                    catIndex = catHash.value(catName + u" / " + cats.value(++i), Category::InvalidId);

                if (catIndex == Category::InvalidId) {
                    throw Exception("unknown category %1 for item %2 %3").arg(catName)
                        .arg(item->itemTypeId()).arg(QLatin1StringView { item->id() });
                }

                addCatIndexes.append(quint16(catIndex));
            }
        }
        item->m_categoryIndexes.copyContainer(addCatIndexes.cbegin(), addCatIndexes.cend(), nullptr);
        lastItemId = itemId;
    }
}

void TextImport::readPartColorCodes(const QByteArray &xml)
{
    QHash<uint, QVector<Item::PCC>> pccs;

    xmlParse(xml, u"CODES", u"ITEM", [this, &pccs](const auto &e) {
        char itemTypeId = ItemType::idFromFirstCharInString(xmlTagText(e, u"ITEMTYPE"));
        const QByteArray itemId = xmlTagText(e, u"ITEMID").toLatin1();
        QString colorName = xmlTagText(e, u"COLOR").simplified();
        bool numeric = false;
        uint code = xmlTagText(e, u"CODENAME").toUInt(&numeric, 10);

        if (auto item = core()->item(itemTypeId, itemId)) {
            bool noColor = !core()->itemType(itemTypeId)->hasColors();

            // workaround for a few Minifigs having a PCC with a real color
            static const QString notApplicable = core()->color(0)->name();
            if (noColor && (colorName != notApplicable))
                colorName = notApplicable;

            uint itemIndex = item->index();

            if (auto color = core()->colorFromName(colorName)) {
                uint colorIndex = color->index();
                addToKnownColors(itemIndex, colorIndex);

                if (numeric) {
                    Item::PCC pcc;
                    pcc.m_pcc = code;
                    pcc.m_colorIndex = colorIndex;
                    pccs[itemIndex].push_back(pcc);
                } else {
                    message(2, u"Parsing part_color_codes: pcc %1 is not numeric"_qs
                                   .arg(xmlTagText(e, u"CODENAME")));
                }
            } else {
                message(2, u"Parsing part_color_codes: skipping invalid color %1 on item %2 %3"_qs
                               .arg(colorName).arg(itemTypeId).arg(QLatin1StringView(itemId)));
            }
        } else {
            message(2, u"Parsing part_color_codes: skipping invalid item %1 %2"_qs
                           .arg(itemTypeId).arg(QLatin1StringView(itemId)));
        }
    });

    for (auto it = pccs.cbegin(); it != pccs.cend(); ++it) {
        auto itemPccs = it.value();
        std::sort(itemPccs.begin(), itemPccs.end(), [](const auto &pcc1, const auto &pcc2) {
            return pcc1.pcc() < pcc2.pcc();
        });

        m_db->m_items[it.key()].m_pccs.copyContainer(itemPccs.cbegin(), itemPccs.cend(), nullptr);
    }
}

const Item *TextImport::readInventory(char itemTypeId, const QByteArray &itemId,
                                      const QDateTime &lastModified, const QByteArray &xml,
                                      bool failSilently)
{
    const auto *invItem = core()->item(itemTypeId, itemId);
    if (!invItem)
        return nullptr; // no item found
    qint64 lastUpdated =  m_inventoryLastUpdated.value(invItem->index(), -1);
    if (lastUpdated < 0)
        return nullptr; // item found, but does not have an inventory
    if (lastModified.toSecsSinceEpoch() < lastUpdated)
        return nullptr; // item found, has an inventory, but is outdated

    QDate lastModifiedDate = lastModified.date();

    QVector<Item::ConsistsOf> inventory;
    QVector<QPair<uint, uint>> knownColors;

    try {
        xmlParse(xml, u"INVENTORY", u"ITEM",
                 [this, &inventory, &knownColors, lastModifiedDate](const auto &e) {
            char itemTypeId = ItemType::idFromFirstCharInString(xmlTagText(e, u"ITEMTYPE"));
            const QByteArray itemId = xmlTagText(e, u"ITEMID").toLatin1();
            uint colorId = xmlTagText(e, u"COLOR").toUInt();
            uint qty = xmlTagText(e, u"QTY").toUInt();
            bool extra = (xmlTagText(e, u"EXTRA") == u"Y");
            bool counterPart = (xmlTagText(e, u"COUNTERPART") == u"Y");
            bool alternate = (xmlTagText(e, u"ALTERNATE") == u"Y");
            uint matchId = xmlTagText(e, u"MATCHID").toUInt();

            auto item = core()->item(itemTypeId, itemId);
            auto color = core()->color(colorId);

            if (!item)
                throw Exception("Unknown item-id %1 %2").arg(itemTypeId).arg(QString::fromLatin1(itemId));
            if (!color)
                throw Exception("Unknown color-id %1").arg(colorId);
            if (!qty)
                throw Exception("Invalid Quantity %1").arg(qty);

            uint itemIndex = item->index();
            uint colorIndex = color->index();

            Item::ConsistsOf co;
            co.m_quantity = qty;
            co.m_itemIndex = itemIndex;
            co.m_colorIndex = colorIndex;
            co.m_extra = extra;
            co.m_isalt = alternate;
            co.m_altid = matchId;
            co.m_cpart = counterPart;

            // if this itemid or color was involved in a changelog entry after the last time we
            // downloaded the inventory, we need to reload
            QByteArray itemTypeAndId = itemTypeId + itemId;
            const auto irange = std::equal_range(m_db->m_itemChangelog.cbegin(), m_db->m_itemChangelog.cend(), itemTypeAndId);
            for (auto it = irange.first; it != irange.second; ++it) {
                if (it->date() > lastModifiedDate) {
                    throw Exception("Item id %1 changed on %2 (last download: %3)")
                        .arg(QString::fromLatin1(itemTypeAndId))
                        .arg(it->date().toString(u"yyyy/MM/dd"))
                        .arg(lastModifiedDate.toString(u"yyyy/MM/dd"));
                }
            }
            const auto crange = std::equal_range(m_db->m_colorChangelog.cbegin(), m_db->m_colorChangelog.cend(), colorId);
            for (auto it = crange.first; it != crange.second; ++it) {
                if (it->date() > lastModifiedDate) {
                    throw Exception("Color id %1 changed on %2 (last download: %3)")
                        .arg(colorId)
                        .arg(it->date().toString(u"yyyy/MM/dd"))
                        .arg(lastModifiedDate.toString(u"yyyy/MM/dd"));
                }
            }

            inventory.append(co);
            knownColors.append({ itemIndex, colorIndex});

        });

        // no more throwing beyond this point, as we cannot undo changes to global data

        for (const auto &kc : std::as_const(knownColors))
            addToKnownColors(kc.first, kc.second);

        // BL bug: if an extra item is part of an alternative match set, then none of the
        //         alternatives have the 'extra' flag set.
        for (Item::ConsistsOf &co : inventory) {
            if (co.isAlternate() && !co.isExtra()) {
                for (const Item::ConsistsOf &mainCo : std::as_const(inventory)) {
                    if ((mainCo.alternateId() == co.alternateId()) && !mainCo.isAlternate()) {
                        co.m_extra = mainCo.m_extra;
                        break;
                    }
                }
            }
        }
        // pre-sort to have a nice sorting order, even in unsorted views
        std::sort(inventory.begin(), inventory.end(), [](const auto &co1, const auto &co2) {
            if (co1.isExtra() != co2.isExtra())
                return co1.isExtra() < co2.isExtra();
            else if (co1.isCounterPart() != co2.isCounterPart())
                return co1.isCounterPart() < co2.isCounterPart();
            else if (co1.alternateId() != co2.alternateId())
                return co1.alternateId() < co2.alternateId();
            else if (co1.isAlternate() != co2.isAlternate())
                return co1.isAlternate() < co2.isAlternate();
            else
                return co1.itemIndex() < co2.itemIndex();
        });

        for (const Item::ConsistsOf &co : std::as_const(inventory)) {
            if (!co.isExtra()) {
                auto &vec = m_appears_in_hash[co.itemIndex()][co.colorIndex()];
                vec.append(qMakePair(co.quantity(), invItem->index()));
            }
        }
        // the hash owns the items now
        m_consists_of_hash.insert(invItem->index(), inventory);
        return invItem;

    } catch (const Exception &e) {
        if (!failSilently)
            message(2, e.errorString());
        return nullptr;
    }
}

void TextImport::readLDrawColors(const QByteArray &ldconfig, const QByteArray &rebrickableColors)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(rebrickableColors, &err);
    if (doc.isNull())
        throw Exception("Rebrickable colors: invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

    QHash<int, uint> ldrawToBrickLinkId;
    const QJsonArray results = doc[u"results"].toArray();
    for (const auto &&result : results) {
        const auto blIds = result[u"external_ids"][u"BrickLink"][u"ext_ids"].toArray();
        const auto ldIds = result[u"external_ids"][u"LDraw"][u"ext_ids"].toArray();

        if (!blIds.isEmpty() && !ldIds.isEmpty()) {
            for (const auto &&ldId : ldIds)
                ldrawToBrickLinkId.insert(ldId.toInt(), uint(blIds.first().toInt()));
        }
    }

    QTextStream in(ldconfig);
    QString line;
    int lineno = 0;

    while (!in.atEnd()) {
        line = in.readLine();
        lineno++;

        QStringList sl = line.simplified().split(u' ');

        if (sl.count() >= 9 &&
                sl[0].toInt() == 0 &&
                sl[1] == u"!COLOUR" &&
                sl[3] == u"CODE" &&
                sl[5] == u"VALUE" &&
                sl[7] == u"EDGE") {
            // 0 !COLOUR name CODE x VALUE v EDGE e [ALPHA a] [LUMINANCE l] [ CHROME | PEARLESCENT | RUBBER | MATTE_METALLIC | METAL | MATERIAL <params> ]

            QString name = sl[2];
            int id = sl[4].toInt();
            QColor color = sl[6];
            QColor edgeColor = sl[8];
            float luminance = 0;
            ColorType type = ColorTypeFlag::Solid;
            float particleMinSize = 0;
            float particleMaxSize = 0;
            float particleFraction = 0;
            float particleVFraction = 0;
            QColor particleColor;

            if (id == 16 || id == 24)
                continue;

            bool isRubber = name.startsWith(u"Rubber_");

            int lastIdx = int(sl.count()) - 1;

            for (int idx = 9; idx < sl.count(); ++idx) {
                if ((sl[idx] == u"ALPHA") && (idx < lastIdx)) {
                    int alpha = sl[++idx].toInt();
                    color.setAlpha(alpha);
                    type.setFlag(ColorTypeFlag::Solid, false);
                    type.setFlag(ColorTypeFlag::Transparent);
                } else if ((sl[idx] == u"LUMINANCE") && (idx < lastIdx)) {
                    luminance = float(sl[++idx].toInt()) / 255;
                } else if (sl[idx] == u"CHROME") {
                    type.setFlag(ColorTypeFlag::Chrome);
                } else if (sl[idx] == u"PEARLESCENT") {
                    type.setFlag(ColorTypeFlag::Pearl);
                } else if (sl[idx] == u"RUBBER") {
                    qt_noop(); // ignore
                } else if (sl[idx] == u"MATTE_METALLIC") {
                    qt_noop(); // ignore
                } else if (sl[idx] == u"METAL") {
                    type.setFlag(ColorTypeFlag::Metallic);
                } else if ((sl[idx] == u"MATERIAL") && (idx < lastIdx)) {
                    const auto mat = sl[idx + 1];

                    if (mat == u"GLITTER") {
                        type.setFlag(name.startsWith(u"Opal_") ? ColorTypeFlag::Satin : ColorTypeFlag::Glitter);
                    } else if (mat == u"SPECKLE") {
                        type.setFlag(ColorTypeFlag::Speckle);
                    } else {
                        message(2, u"Found unsupported MATERIAL %1 at line %2 of LDConfig.ldr"_qs
                                       .arg(mat).arg(lineno));
                    }
                } else if ((sl[idx] == u"SIZE") && (idx < lastIdx)) {
                    particleMinSize = particleMaxSize = sl[++idx].toFloat();
                } else if ((sl[idx] == u"MINSIZE") && (idx < lastIdx)) {
                    particleMinSize = sl[++idx].toFloat();
                } else if ((sl[idx] == u"MAXSIZE") && (idx < lastIdx)) {
                    particleMaxSize = sl[++idx].toFloat();
                } else if ((sl[idx] == u"VALUE") && (idx < lastIdx)) {
                    particleColor = sl[++idx];
                } else if ((sl[idx] == u"FRACTION") && (idx < lastIdx)) {
                    particleFraction = sl[++idx].toFloat();
                } else if ((sl[idx] == u"VFRACTION") && (idx < lastIdx)) {
                    particleVFraction = sl[++idx].toFloat();
                }
            }

            auto updateColor = [&](Color &c) {
                if (c.m_ldraw_id != -1)
                    return;

                c.m_luminance = luminance;
                c.m_ldraw_id = id;
                c.m_ldraw_color = color;
                c.m_ldraw_edge_color = edgeColor;
                c.m_particleMinSize = particleMinSize;
                c.m_particleMaxSize = particleMaxSize;
                c.m_particleFraction = particleFraction;
                c.m_particleVFraction = particleVFraction;
                c.m_particleColor = particleColor;
            };

            bool found = false;

            // Rebrickable's m:n mappings can be weird - try to first fuzzy-match on names
            // 1. all lower-case
            // 2. all spaces and dashes are underscores
            // 3. LDraw's "grey_" is "gray_" on BrickLink
            // 3. LDraw's "opal_" is "satin_" on BrickLink
            for (auto &c : m_db->m_colors) {
                QString ldName = name.toLower();
                QString blName = c.name()
                        .toLower()
                        .replace(u" "_qs, u"_"_qs)
                        .replace(u"-"_qs, u"_"_qs)
                        .replace(u"gray_"_qs, u"grey_"_qs)
                        .replace(u"satin_"_qs, u"opal_"_qs)
                        .simplified();
                if (blName == ldName) {
                    updateColor(c);
                    found = true;
                    break;
                }
            }

            // Some mapping are missing or are ambigious via Rebrickable - hardcode these
            if (!found) {
                static const QMap<QString, QString> manualLDrawToBrickLink = {
                    { u"Trans_Bright_Light_Green"_qs, u"Trans-Light Bright Green"_qs },
                };

                QString blName = manualLDrawToBrickLink.value(name);
                if (!blName.isEmpty()) {
                    for (auto &c : m_db->m_colors) {
                        if (blName == c.name()) {
                            updateColor(c);
                            found = true;
                            break;
                        }
                    }
                }
            }

            // Try the Rebrickable color table (but not for rubbery colors)
            if (!found && !isRubber) {
                uint blColorId = ldrawToBrickLinkId.value(id);
                if (blColorId) {
                    auto blColor = const_cast<Color *>(core()->color(blColorId));
                    if (blColor) {
                        updateColor(*blColor);
                        found = true;
                    }
                }
            }

            // We can't map the color to a BrickLink one, but at least we keep a record of it to
            // be able to render composite parts with fixed colors (e.g. electric motors)
            if (!found) {
                Color c;
                c.m_name.copyQString(u"LDraw: " + name, nullptr);
                c.m_type = type;
                c.m_color = color;
                updateColor(c);

                m_db->m_ldrawExtraColors.push_back(c);

                if (!name.startsWith(u"Rubber_")) {
                    message(2, u"Could not match LDraw color %1 %2 to any BrickLink color"_qs
                                   .arg(id).arg(name));
                }
            }
        }
    }

    for (auto &c : m_db->m_colors) {
        if (c.m_ldraw_id < 0 && c.id()) {
            // We need at least some estimated values for the 3D renderer
            c.m_ldraw_color = c.m_color;
            c.m_ldraw_edge_color = (c.m_color.lightness() < 128) ? c.m_color.lighter() : c.m_color.darker();

            int alpha = 255;

            if (c.isTransparent())
                alpha = 128;
            else if (c.isMilky())
                alpha = 240;

            if (c.isSpeckle()) {
                c.m_particleMinSize = 1;
                c.m_particleMaxSize = 3;
                c.m_particleFraction = 0.4f;
                c.m_particleVFraction = 0;
                c.m_particleColor = "pink";
            } else if (c.isGlitter()) {
                alpha = 128;
                c.m_particleMinSize = 1;
                c.m_particleMaxSize = 1;
                c.m_particleFraction = 0.17f;
                c.m_particleVFraction = 0.2f;
                c.m_particleColor = "gray";
            } else if (c.isSatin()) {
                alpha = 200;
                c.m_luminance = 5.f / 255;
                c.m_particleMinSize = 0.02f;
                c.m_particleMaxSize = 0.1f;
                c.m_particleFraction = 0.8f;
                c.m_particleVFraction = 0.6f;
                c.m_particleColor = "white";
            }
            c.m_ldraw_color.setAlpha(alpha);

            if (!c.isModulex()) {
                message(2, u"Could not match BrickLink color %1 %2 to any LDraw color (%3)"_qs
                               .arg(c.id()).arg(c.name()).arg(c.m_ldraw_color.name()));
            }
        }
    }
}

void TextImport::readInventoryList(const QByteArray &csv)
{
    QTextStream ts(csv);
    int lineNumber = 0;
    while (!ts.atEnd()) {
        ++lineNumber;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split(u'\t');

        if (strs.count() < 2)
            throw Exception("expected at least 2 fields in btinvlist, line %1").arg(lineNumber);

        char itemTypeId = ItemType::idFromFirstCharInString(strs.at(0));
        const QByteArray itemId = strs.at(1).toLatin1();

        if (!itemTypeId || itemId.isEmpty()) {
            throw Exception("expected a valid item-type and an item-id field in btinvlist, line %1")
                .arg(lineNumber);
        }

        auto item = core()->item(itemTypeId, itemId);
        if (item) {
            uint itemIndex = item->index();
            qint64 t(0);   // 1.1.1970 00:00

            if (strs.count() > 2) {
                const QString &dtStr = strs.at(2);
                if (!dtStr.isEmpty()) {
                    static QString fmtFull = QStringLiteral("M/d/yyyy h:mm:ss AP");
                    static QString fmtShort = QStringLiteral("M/d/yyyy");


                    QDateTime dt = QDateTime::fromString(dtStr, dtStr.length() <= 10 ? fmtShort : fmtFull);
#if !defined(DAN_FIXED_BTINVLIST_TO_USE_UTC)
                    static QTimeZone tzEST = QTimeZone("EST5EDT");
                    dt.setTimeZone(tzEST);
                    dt = dt.toUTC();
#endif
                    t = dt.toSecsSinceEpoch();
                }
            }
            Q_ASSERT(item->m_itemTypeIndex < 8);
            m_db->m_itemTypes[item->m_itemTypeIndex].m_has_inventories = true;

            m_inventoryLastUpdated[itemIndex] = t;

            for (quint16 catIndex : item->m_categoryIndexes)
                m_db->m_categories[catIndex].m_has_inventories |= (quint8(1) << item->m_itemTypeIndex);
        } else {
            message(2, u"item %1 %2 doesn't exist"_qs.arg(itemTypeId).arg(QLatin1StringView { itemId }));
        }
    }
}

void TextImport::readChangeLog(const QByteArray &csv)
{
    m_db->m_latestChangelogId = 0;

    QTextStream ts(csv);
    int lineNumber = 0;
    while (!ts.atEnd()) {
        ++lineNumber;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split(u'\t');

        if (strs.count() < 7)
            throw Exception("expected at least 7 fields in btchglog, line %1").arg(lineNumber);

        uint id = strs.at(0).toUInt();
        QDate date = QDate::fromString(strs.at(1), u"M/d/yyyy"_qs);
        char c = ItemType::idFromFirstCharInString(strs.at(2));

        switch (c) {
        case 'I':   // ItemId
        case 'T':   // ItemType
        case 'M': { // ItemMerge
            const QString &fromType = strs.at(3);
            QString fromId = strs.at(4);
            const QString &toType = strs.at(5);
            const QString &toId = strs.at(6);
            if ((fromType.length() == 1) && (toType.length() == 1)
                    && !fromId.isEmpty() && !toId.isEmpty()) {
                // BL bug: if something changes into a Set, the fromId has a '-1' appended that
                //         shouldn't be there. The other way around, the '-1' is missing.
                if ((toType.at(0) == u'S') && (fromType.at(0) != u'S') && fromId.endsWith(u"-1"_qs))
                    fromId.chop(2);
                if ((toType.at(0) != u'S') && (fromType.at(0) == u'S') && !fromId.endsWith(u"-1"_qs))
                    fromId.append(u"-1"_qs);

                ItemChangeLogEntry icl;
                icl.m_id = id;
                icl.m_julianDay = uint(date.toJulianDay());
                icl.m_fromTypeAndId.copyQByteArray((fromType + fromId).toLatin1(), nullptr);
                icl.m_toTypeAndId.copyQByteArray((toType + toId).toLatin1(), nullptr);
                m_db->m_itemChangelog.push_back(icl);
                m_db->m_latestChangelogId = std::max(m_db->m_latestChangelogId, id);
            }
            break;
        }
        case 'A': { // ColorMerge
            bool fromOk = false, toOk = false;
            uint fromId = strs.at(3).toUInt(&fromOk);
            uint toId = strs.at(5).toUInt(&toOk);
            if (fromOk && toOk) {
                ColorChangeLogEntry ccl;
                ccl.m_id = id;
                ccl.m_julianDay = uint(date.toJulianDay());
                ccl.m_fromColorId = fromId;
                ccl.m_toColorId = toId;
                m_db->m_colorChangelog.push_back(ccl);
                m_db->m_latestChangelogId = std::max(m_db->m_latestChangelogId, id);
            }
            break;
        }
        case 'E':   // CategoryName
        case 'G':   // CategoryMerge
        case 'C':   // ColorName
            break;
        default:
            message(2, u"skipping invalid change code %1"_qs.arg(c));
            break;
        }
    }
    std::sort(m_db->m_colorChangelog.begin(), m_db->m_colorChangelog.end());
    std::sort(m_db->m_itemChangelog.begin(), m_db->m_itemChangelog.end());
}

QCoro::Task<> TextImport::readRelationships(const QByteArray &html)
{
    auto data = QString::fromUtf8(html);

    static const QRegularExpression rxLink(uR"-(<A HREF="catalogRelCat\.asp\?relID=(\d+)">([^<]+)</A>)-"_qs);

    for (const auto &m : rxLink.globalMatch(data)) {
        uint id = m.capturedView(1).toUInt();
        QString name = m.captured(2);

        Relationship rel;
        rel.m_id = id;
        rel.m_name.copyQString(name, nullptr);

        QHash<uint, QVector<uint>> matches;   // match-id -> list of item-indexes

        static const QRegularExpression rxNotWord(uR"(\W)"_qs);
        QString cleanName = name;
        cleanName.replace(rxNotWord, u" "_qs);
        cleanName = cleanName.simplified().toLower();
        cleanName.replace(u' ', u'_');

        uint page = 1;
        do {
            QUrl url = brickLinkUrl("catalogRelList.asp");
            url.setQuery({
                { u"v"_qs, u"0"_qs },
                { u"relID"_qs, QString::number(id) },
                { u"pg"_qs, QString::number(page) },
            });

            QByteArray listHtml = co_await download(url, u"relationships/%1_%2.html"_qs.arg(cleanName).arg(page));
            QString listData = QString::fromUtf8(listHtml);

            static const QRegularExpression rxHeader(uR"(<B>(\d+)</B> Matches found: Page <B>(\d+)</B> of <B>(\d+)</B>)"_qs);
            auto mh = rxHeader.match(listData);

            if (!mh.hasMatch())
                throw Exception("Relationships: couldn't find list header");

            uint count = mh.capturedView(1).toUInt();
            uint currentPage = mh.capturedView(2).toUInt();
            uint maxPage = mh.capturedView(3).toUInt();

            if (page == 1)
                rel.m_count = count;
            else if (rel.m_count != count)
                throw Exception("Relationships: count mismatch between pages: expected %1, but got %2 on page %3").arg(rel.m_count).arg(count).arg(page);
            if (currentPage != page)
                throw Exception("Relationships: wrong page: expected %1, but got %2").arg(page).arg(currentPage);

            qsizetype startPos = mh.capturedEnd();
            qsizetype endPos = listData.indexOf(u"</TABLE>"_qs, startPos);

            static const QRegularExpression rxMatch(uR"-(<TR BGCOLOR="#......"><TD COLSPAN="4">.*?<B>Match #(\d+)</B></FONT></TD></TR>)-"_qs);
            static const QRegularExpression rxItem (uR"-(<TR BGCOLOR="#......"><TD ALIGN="CENTER" WIDTH="10%">.*?<A HREF="/v2/catalog/catalogitem\.page\?([A-Z])=([A-Za-z0-9._-]+)">([A-Za-z0-9._-]+)</A>.*</FONT></TD></TR>)-"_qs);

            uint currentMatchId = 0;
            int trCount = 0;

            while (true) {
                qsizetype matchStartPos = listData.indexOf(u"<TR "_qs, startPos);
                if (matchStartPos < 0)
                    break;
                qsizetype matchEndPos = listData.indexOf(u"</TR>"_qs, matchStartPos);
                if (matchEndPos < 0)
                    break;
                matchEndPos += 5;
                if (matchEndPos > endPos)
                    break;
                startPos = matchEndPos;

                if (++trCount == 1) // skip header
                    continue;

                auto currentRow = QStringView { listData }.sliced(matchStartPos, matchEndPos - matchStartPos);

                auto mm = rxMatch.match(currentRow);
                if (mm.hasMatch()) {
                    uint matchId = mm.capturedView(1).toUInt();
                    currentMatchId = matchId;
                } else {
                    if (!currentMatchId)
                        throw Exception("Relationships: got an item row without a preceeding match # row");

                    auto mi = rxItem.match(currentRow);
                    if (mi.hasMatch()) {
                        QStringView ittId = mi.capturedView(1);
                        QStringView itemId = mi.capturedView(2);
                        QStringView itemId2 = mi.capturedView(3);
                        if (itemId != itemId2)
                            throw Exception("Relationships: item ids do not match up: %1 vs. %2").arg(itemId).arg(itemId2);
                        if (ittId.size() != 1)
                            throw Exception("Relationships: invalid item-type id: %1").arg(ittId);
                        auto item = core()->item(ittId.at(0).toLatin1(), itemId.toLatin1());
                        if (!item) {
                            message(2, u"could not resolve item: %1 %2 for match id %3 in %4"_qs
                                           .arg(ittId).arg(itemId).arg(currentMatchId).arg(rel.name()));
                        } else {
                            matches[currentMatchId].push_back(item->index());
                        }
                    } else {
                        message(2, currentRow.toString());
                        throw Exception("Relationships: Found a TR that is neither an item nor a match id");
                    }
                }
            }

            page = (page < maxPage) ? (page + 1) : 0;
        } while (page);

        if (rel.m_count != matches.count()) {
            message(2, u"%1 should have %2 entries, but has %3 instead"_qs
                           .arg(rel.name()).arg(rel.m_count).arg(matches.count()));
            rel.m_count = uint(matches.count());
        }
        m_db->m_relationships.push_back(rel);

        for (auto it = matches.cbegin(); it != matches.cend(); ++it) {
            const auto &itemIndexes = it.value();

            RelationshipMatch relm;
            relm.m_id = it.key();
            relm.m_relationshipId = rel.m_id;
            relm.m_itemIndexes.copyContainer(itemIndexes.cbegin(), itemIndexes.cend(), nullptr);
            m_db->m_relationshipMatches.push_back(relm);
        }
    }
    std::sort(m_db->m_relationships.begin(), m_db->m_relationships.end());
    std::sort(m_db->m_relationshipMatches.begin(), m_db->m_relationshipMatches.end());

    // store the matches in each affected item as well for faster lookup
    QHash<uint, QVector<quint16>> itemRelMatchIds;

    for (const auto &relMatch : m_db->m_relationshipMatches) {
        if (relMatch.m_id > 0xffffu) {
            message(2, u"relationship match id %1 exceeds 16 bits"_qs.arg(relMatch.m_id));
            continue;
        }
        for (const auto &itemIndex : relMatch.m_itemIndexes)
            itemRelMatchIds[itemIndex].push_back(quint16(relMatch.m_id));
    }
    for (const auto [itemIndex, relMatchIds] : std::as_const(itemRelMatchIds).asKeyValueRange()) {
        m_db->m_items[itemIndex].m_relationshipMatchIds
            .copyContainer(relMatchIds.cbegin(), relMatchIds.cend(), nullptr);
    }
}

void TextImport::calculateColorPopularity()
{
    float maxpop = 0;
    for (const auto &col : m_db->m_colors)
        maxpop = qMin(maxpop, col.m_popularity);
    for (auto &col : m_db->m_colors) {
        float &pop = col.m_popularity;

        if (!qFuzzyIsNull(maxpop))
            pop /= maxpop;
        else
            pop = 0;
    }
}

void TextImport::calculateItemTypeCategories()
{
    QHash<uint, QSet<uint>> ittCats;

    // calculate the item-type -> category relation
    for (const auto &item : m_db->m_items) {
        for (quint16 catIndex : item.m_categoryIndexes)
            ittCats[item.m_itemTypeIndex].insert(catIndex);
    }
    for (const auto [ittIndex, catIndexSet] : std::as_const(ittCats).asKeyValueRange()) {
        m_db->m_itemTypes[ittIndex].m_categoryIndexes
            .copyContainer(catIndexSet.cbegin(), catIndexSet.cend(), nullptr);
    }
}

void TextImport::calculateKnownAssemblyColors()
{
    for (auto it = m_appears_in_hash.begin(); it != m_appears_in_hash.end(); ++it) {
        const Item &item = m_db->m_items[it.key()];
        const ItemType &itemType = m_db->m_itemTypes[item.m_itemTypeIndex];

        if (!it->contains(0) || !itemType.hasColors())
            continue;

        // The type supports colors, but we have entries for color "not available"
        //   -> check assemblies

        // We need to copy here, as we modify (*it) later on and this would invalidate noColor
        QVector<QPair<uint, uint>> noColor = it->value(0);

        for (auto ncit = noColor.begin(); ncit != noColor.end(); ) {
            uint ncQty = ncit->first;
            uint ncItemIndex = ncit->second;
            const Item &ncItem = m_db->m_items[ncItemIndex];

            // "aiItem" contains "item" with color == 0: now find all possible colors
            // for "aiItem" and copy those to "item's" appearHash.
            // Also update the known colors.
            bool foundColor = false;
            for (auto colorIndex : ncItem.m_knownColorIndexes) {
                if (colorIndex) {
                    addToKnownColors(item.index(), colorIndex);
                    (*it)[colorIndex].append({ ncQty, ncItemIndex });
                    foundColor = true;
                }
            }
            if (foundColor)
                ncit = noColor.erase(ncit);
            else
                ++ncit;
        }
        if (!noColor.isEmpty())
            (*it)[0] = noColor;
        else
            it->remove(0);
    }
}

void TextImport::calculateCategoryRecency()
{
    QHash<uint, std::pair<quint64, quint32>> catCounter;

    for (const auto &item : std::as_const(m_db->m_items)) {
        if (item.m_year_from && item.m_year_to) {
            for (quint16 catIndex : item.m_categoryIndexes) {
                auto &cc = catCounter[catIndex];
                cc.first += (item.m_year_from + item.m_year_to);
                cc.second += 2;

                auto &cat = m_db->m_categories[catIndex];
                cat.m_year_from = cat.m_year_from ? std::min(cat.m_year_from, item.m_year_from)
                                                  : item.m_year_from;
                cat.m_year_to = std::max(cat.m_year_to, item.m_year_to);
            }
        }
    }
    for (uint catIndex = 0; catIndex < m_db->m_categories.size(); ++catIndex) {
        auto cc = catCounter.value(catIndex);
        if (cc.first && cc.second) {
            auto y = quint8(qBound(0ULL, cc.first / cc.second, 255ULL));
            m_db->m_categories[catIndex].m_year_recency = y;
        }
    }
}

void TextImport::calculatePartsYearUsed()
{
    // Parts have no "yearReleased" in the downloaded XMLs. We can however calculate a
    // "last recently used" value, which is much more useful for parts anyway.

    // we make 2 passes:
    //   #1 for parts in non-parts (these all have a year-released) and
    //   #2 for parts in parts (which by then should hopefully all have a year-released

    for (int pass = 1; pass <= 2; ++pass) {
        for (uint itemIndex = 0; itemIndex < m_db->m_items.size(); ++itemIndex) {
            Item &item = m_db->m_items[itemIndex];

            bool isPart = (item.itemTypeId() == 'P');

            if ((pass == 1 ? !isPart : isPart) && item.yearReleased()) {
                const auto itemParts = m_consists_of_hash.value(itemIndex);

                for (const Item::ConsistsOf &part : itemParts) {
                    Item &partItem = m_db->m_items[part.itemIndex()];
                    if (partItem.itemTypeId() == 'P') {
                        partItem.m_year_from = partItem.m_year_from ? std::min(partItem.m_year_from, item.m_year_from)
                                                                    : item.m_year_from;
                        partItem.m_year_to   = std::max(partItem.m_year_to, item.m_year_to);
                    }
                }
            }
        }
    }
}

void TextImport::addToKnownColors(uint itemIndex, uint addColorIndex)
{
    if (addColorIndex <= 0)
        return;

    // colorless items don't need the pointless "(Not Applicable)" entry
    Item &item = m_db->m_items[itemIndex];
    if (!item.itemType()->hasColors())
        return;

    for (quint16 colIndex : item.m_knownColorIndexes) {
        if (colIndex == quint16(addColorIndex))
            return;
    }
    item.m_knownColorIndexes.push_back(quint16(addColorIndex), nullptr);
}

uint TextImport::loadLastRunInventories(const std::function<void(char, const QByteArray &, const QDateTime &, const QByteArray &)> &callback)
{
    uint counter = 0;
    auto updateCounter = [&]() {
        if (++counter % 500 == 0) {
            printf("%c", (counter % 5000) ? '.' : 'o');
            fflush(stdout);
        }
    };

    if (m_lastRunArchive && m_lastRunArchive->isOpen()) {
        message(u"Loading inventories from ZIP..."_qs);

        const auto fileList = m_lastRunArchive->fileList();
        for (const QString &filePath : fileList) {
            if (!filePath.endsWith(u".xml") || (filePath.at(1) != u'/'))
                continue;

            const QString itemId = filePath.section(u'/', -1, -1).chopped(4);
            const QString itemTypeId = filePath.section(u'/', -2, -2);

            if ((itemTypeId.size() != 1) || !u"BCGIMOPS"_qs.contains(itemTypeId.at(0)))
                throw Exception("Invalid entry in inventories cache: %1").arg(filePath);

            updateCounter();

            auto [data, lastModified] = m_lastRunArchive->readFileAndLastModified(filePath);
            callback(itemTypeId.at(0).toLatin1(), itemId.toLatin1(), lastModified, data);
        }
    } else {
        message(u"Loading inventories from filesystem..."_qs);

        QDirIterator dit(core()->dataPath(), { u"inventory.xml"_qs }, QDir::Files, QDirIterator::Subdirectories);
        while (dit.hasNext()) {
            const QFileInfo fi = dit.nextFileInfo();
            const QString itemId = fi.filePath().section(u'/', -2, -2);
            const QString itemTypeId = fi.filePath().section(u'/', -4, -4);

            if ((itemTypeId.size() != 1) || !u"BCGIMOPS"_qs.contains(itemTypeId.at(0)))
                continue;

            updateCounter();

            QFile f(fi.filePath());
            if (!f.open(QIODevice::ReadOnly))
                throw Exception(&f, "Failed to open for reading");

            const QByteArray data = f.readAll();
            if (data.isEmpty())
                throw Exception("Failed to read XML from %1").arg(fi.filePath());

            callback(itemTypeId.at(0).toLatin1(), itemId.toLatin1(), fi.lastModified(), data);
        }
    }

    printf("\n");
    return counter;
}

void TextImport::nextStep(const QString &text)
{
    printf("\nSTEP %d: %s\n", ++m_currentStep, qPrintable(text));
}

void TextImport::message(const QString &text)
{
    message(1, text);
}

void TextImport::message(int level, const QString &text)
{
    if (level <= 0)
        printf("%s\n", qPrintable(text));
    else
        printf("%s%c %s\n", QByteArray(level * 2, ' ').constData(), (level <= 1) ? '*' : '>', qPrintable(text));
}

void TextImport::xmlParse(const QByteArray &xml, QStringView rootName, QStringView elementName,
                          const std::function<void (const QHash<QString, QString> &)> &callback)
{
    // Bricklink double encodes entities, so instead of "&#40;", we get "&amp;#40;"
    const QByteArray fixedXml = core()->isApiQuirkActive(ApiQuirk::CatalogDownloadEntitiesAreDoubleEscaped)
                                    ? QByteArray(xml).replace("&amp;#", "&#")
                                    : xml;

    QXmlStreamReader xsr(fixedXml);
    if (xsr.readNextStartElement()) {
        if (xsr.name() != rootName)
            throw Exception("expected XML root node %1, but got %2").arg(rootName, xsr.name());
        while (xsr.readNextStartElement()) {
            if (xsr.name() != elementName)
                throw Exception("expected XML element node %1, but got %2").arg(elementName, xsr.name());

            QHash<QString, QString> hash;
            while (xsr.readNextStartElement())
                hash.insert(xsr.name().toString(), xsr.readElementText().trimmed());
            callback(hash);
        }
    }

    if (xsr.hasError())
        throw Exception("Error parsing XML: %1").arg(xsr.errorString());
}

QString TextImport::xmlTagText(const QHash<QString, QString> &element, QStringView tagName,
                               bool optional)
{
    auto it = element.constFind(tagName.toString());
    if (it != element.constEnd())
        return it.value();
    else if (optional)
        return { };
    else
        throw Exception("Expected a <%1> tag, but couldn't find one").arg(tagName);
}

} // namespace BrickLink

#include "moc_textimport_p.cpp"
