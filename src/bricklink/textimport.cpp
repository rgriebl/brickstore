// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QtDebug>
#include <QtCore/QTimeZone>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QStringBuilder>
#include <QtXml/QDomElement>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "utility/exception.h"
#include "bricklink/core.h"
#include "bricklink/dimensions.h"
#include "bricklink/textimport.h"
#include "bricklink/changelogentry.h"


static void xmlParse(const QString &path, const QString &rootName, const QString &elementName,
                     const std::function<void (const QDomElement &)> &callback)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    QDomDocument doc;
    QString emsg;
    int eline = 0, ecolumn = 0;
    if (!doc.setContent(&f, false, &emsg, &eline, &ecolumn))
        throw ParseException(&f, "%1 at line %2, column %3").arg(emsg).arg(eline).arg(ecolumn);

    auto root = doc.documentElement().toElement();
    if (root.nodeName() != rootName)
        throw ParseException(&f, "expected root node %1, but got %2").arg(rootName, root.nodeName());

    try {
        for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
            if (node.nodeName() == elementName)
                callback(node.toElement());
        }
    } catch (const Exception &e) {
        throw ParseException(&f, e.what());
    }
}

static QString xmlTagText(const QDomElement &parent, const char *tagName)
{
    const auto dnl = parent.elementsByTagName(QString::fromLatin1(tagName));
    if (dnl.size() != 1) {
        throw ParseException("Expected a single %1 tag, but found %2")
            .arg(QString::fromLatin1(tagName)).arg(dnl.size());
    }
    QString text = dnl.at(0).toElement().text().trimmed();

    // QXml did not decode entities, so we have to do it here
    qsizetype pos = 0;
    while ((pos = text.indexOf(u"&#", pos)) >= 0) {
        auto endpos = text.indexOf(u';', pos + 2);
        if (endpos < 0) {
            pos += 2;
        } else {
            bool isHex = (text.at(pos + 2) == u'x');
            int unicode = QStringView { text }.mid(pos + 2, endpos - pos - 2)
                              .toInt(nullptr, isHex ? 16 : 10);
            if (unicode > 0) {
                text.replace(pos, endpos - pos + 1, QChar(unicode));
                pos++;
            } else {
                pos = endpos + 1;
            }
        }
    }
    return text;
}


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


BrickLink::TextImport::TextImport()
    : m_db(core()->database())
{ }

BrickLink::TextImport::~TextImport()
{ }

bool BrickLink::TextImport::import(const QString &path)
{
    try {
        // colors
        readColors(path + u"colors.xml");
        readLDrawColors(path + u"ldconfig.ldr", path + u"rebrickable_colors.json");
        calculateColorPopularity();

        // categories
        readCategories(path + u"categories.xml");

        // itemtypes
        readItemTypes(path + u"itemtypes.xml");

        // speed up loading (exactly 137522 items on 16.06.2020)
        m_db->m_items.reserve(200000);

        for (const ItemType &itt : std::as_const(m_db->m_itemTypes)) {
            readItems(path + u"items_" + QChar::fromLatin1(itt.m_id) + u".xml", &itt);
            readAdditionalItemCategories(path + u"items_" + QChar::fromLatin1(itt.m_id) + u".csv", &itt);
        }
        readRelationships(path + u"relationships.html");

        readPartColorCodes(path + u"part_color_codes.xml");
        readInventoryList(path + u"btinvlist.csv");
        readChangeLog(path + u"btchglog.csv");

        return true;
    } catch (const Exception &e) {
        qWarning() << "Error importing database:" << e.what();
        return false;
    }
}


void BrickLink::TextImport::readColors(const QString &path)
{
    xmlParse(path, u"CATALOG"_qs, u"ITEM"_qs, [this](QDomElement e) {
        Color col;
        uint colid = xmlTagText(e, "COLOR").toUInt();

        col.m_id       = colid;
        col.m_name.copyQString(xmlTagText(e, "COLORNAME").simplified(), nullptr);
        col.m_color    = QColor(u'#' + xmlTagText(e, "COLORRGB"));

        col.m_ldraw_id = -1;
        col.m_type     = ColorType();

        auto type = xmlTagText(e, "COLORTYPE");
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

        int partCnt    = xmlTagText(e, "COLORCNTPARTS").toInt();
        int setCnt     = xmlTagText(e, "COLORCNTSETS").toInt();
        int wantedCnt  = xmlTagText(e, "COLORCNTWANTED").toInt();
        int forSaleCnt = xmlTagText(e, "COLORCNTINV").toInt();

        col.m_popularity = float(partCnt + setCnt + wantedCnt + forSaleCnt);

        // needs to be divided by the max. after all colors are parsed!
        // mark it as raw data meanwhile:
        col.m_popularity = -col.m_popularity;

        col.m_year_from = xmlTagText(e, "COLORYEARFROM").toUShort();
        col.m_year_to   = xmlTagText(e, "COLORYEARTO").toUShort();

        m_db->m_colors.push_back(col);
    });

    std::sort(m_db->m_colors.begin(), m_db->m_colors.end());
}

void BrickLink::TextImport::readCategories(const QString &path)
{
    xmlParse(path, u"CATALOG"_qs, u"ITEM"_qs, [this](QDomElement e) {
        Category cat;
        uint catid = xmlTagText(e, "CATEGORY").toUInt();

        cat.m_id   = catid;
        cat.m_name.copyQString(xmlTagText(e, "CATEGORYNAME").simplified(), nullptr);

        m_db->m_categories.push_back(cat);
    });

    std::sort(m_db->m_categories.begin(), m_db->m_categories.end());

}

void BrickLink::TextImport::readAdditionalItemCategories(const QString &path, const BrickLink::ItemType *itt)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    QTextStream ts(&f);
    ts.readLine(); // skip header
    ts.readLine(); // skip empty line
    ts.readLine(); // skip empty line
    ts.readLine(); // skip merge cat
    int lineNumber = 4;

    while (!ts.atEnd()) {
        ++lineNumber;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split(u'\t');

        if (strs.count() < 3)
            throw ParseException(&f, "expected at least 2 fields in line %1").arg(lineNumber);

        const QByteArray itemId = strs.at(2).toLatin1();
        auto item = const_cast<Item *>(core()->item(itt->m_id, itemId));
        if (!item)
            throw ParseException(&f, "expected a valid item-id field in line %1").arg(lineNumber);

        QString catStr = strs.at(1).simplified();
        const Category &mainCat = m_db->m_categories.at(item->m_categoryIndexes[0]);
        if (!catStr.startsWith(mainCat.name()))
            throw ParseException(&f, "additional categories do not start with the main category in line %1").arg(lineNumber);
        if (strs.at(0).toUInt() != mainCat.m_id)
            throw ParseException(&f, "the main category id does not match the XML in line %1").arg(lineNumber);

        catStr = catStr.mid(mainCat.name().length() + 3);

        const QStringList cats = catStr.split(u" / "_qs);
        for (int i = 0; i < cats.count(); ++i) {
            for (quint16 catIndex = 0; catIndex < quint16(m_db->m_categories.size()); ++catIndex) {
                const QString catName = m_db->m_categories.at(catIndex).name();
                bool disambiguate = catName.contains(u" / ");
                quint16 addCatIndex = quint16(-1);

                // The " / " sequence is used to separate the fields, but it also appears in
                // category names like "String Reel / Winch"

                if (disambiguate) {
                    const auto catNameList = catName.split(u" / "_qs);
                    if (catNameList == cats.mid(i, catNameList.size())) {
                        addCatIndex = catIndex;
                        i += (catNameList.size() - 1);
                    }
                } else if (catName == cats.at(i)) {
                    addCatIndex = catIndex;
                }

                if (addCatIndex != quint16(-1))
                    item->m_categoryIndexes.push_back(addCatIndex, nullptr);
            }
        }
    }
}

void BrickLink::TextImport::readItemTypes(const QString &path)
{
    xmlParse(path, u"CATALOG"_qs, u"ITEM"_qs, [this](QDomElement e) {
        ItemType itt;
        char c = ItemType::idFromFirstCharInString(xmlTagText(e, "ITEMTYPE"));

        if (c == 'U')
            return;

        itt.m_id   = c;
        itt.m_name.copyQString(xmlTagText(e, "ITEMTYPENAME").simplified(), nullptr);

        itt.m_has_inventories   = false;
        itt.m_has_colors        = (c == 'P' || c == 'G');
        itt.m_has_weight        = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
        itt.m_has_subconditions = (c == 'S');

        m_db->m_itemTypes.push_back(itt);
    });

    std::sort(m_db->m_itemTypes.begin(), m_db->m_itemTypes.end());
}

void BrickLink::TextImport::readItems(const QString &path, const BrickLink::ItemType *itt)
{
    xmlParse(path, u"CATALOG"_qs, u"ITEM"_qs, [this, itt](QDomElement e) {
        Item item;
        item.m_id.copyQByteArray(xmlTagText(e, "ITEMID").toLatin1(), nullptr);
        const QString itemName = xmlTagText(e, "ITEMNAME").simplified();
        item.m_name.copyQString(itemName, nullptr);
        item.m_itemTypeIndex = (itt - m_db->m_itemTypes.data());

        uint catId = xmlTagText(e, "CATEGORY").toUInt();
        auto cat = core()->category(catId);
        if (!cat)
            throw ParseException("item %1 has no category").arg(QString::fromLatin1(item.id()));
        item.m_categoryIndexes.push_back(quint16(cat->index()), nullptr);

        uint y = 0;
        try {
            y = xmlTagText(e, "ITEMYEAR").toUInt();
        } catch (...) { }
        item.m_year_from = ((y > 1900) && (y < 2155)) ? (y - 1900) : 0; // we only have 8 bits for the year
        item.m_year_to = item.m_year_from;

        if (itt->hasWeight())
            item.m_weight = xmlTagText(e, "ITEMWEIGHT").toFloat();
        else
            item.m_weight = 0;

        try {
            auto color = core()->color(xmlTagText(e, "IMAGECOLOR").toUInt());
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

void BrickLink::TextImport::readPartColorCodes(const QString &path)
{
    QHash<uint, std::vector<Item::PCC>> pccs;

    xmlParse(path, u"CODES"_qs, u"ITEM"_qs, [this, &pccs](QDomElement e) {
        char itemTypeId = ItemType::idFromFirstCharInString(xmlTagText(e, "ITEMTYPE"));
        const QByteArray itemId = xmlTagText(e, "ITEMID").toLatin1();
        QString colorName = xmlTagText(e, "COLOR").simplified();
        bool numeric = false;
        uint code = xmlTagText(e, "CODENAME").toUInt(&numeric, 10);

        if (!numeric) {
            qWarning() << "  > Parsing part_color_codes: pcc" << xmlTagText(e, "CODENAME")
                       << "is not numeric";
        }

        if (auto item = core()->item(itemTypeId, itemId)) {
            bool colorLess = !core()->itemType(itemTypeId)->hasColors();

            // workaround for a few Minifigs having a PCC with a real color
            static const QString notApplicable = core()->color(0)->name();
            if (colorLess && (colorName != notApplicable))
                colorName = notApplicable;

            int itemIndex = item->index();
            bool found = false;
            for (uint colorIndex = 0; colorIndex < m_db->m_colors.size(); ++colorIndex) {
                if (m_db->m_colors[colorIndex].name() == colorName) {
                    addToKnownColors(itemIndex, colorIndex);

                    if (numeric) {
                        Item::PCC pcc;
                        pcc.m_pcc = code;
                        pcc.m_colorIndex = colorIndex;
                        pccs[itemIndex].push_back(pcc);
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
                qWarning() << "  > Parsing part_color_codes: skipping invalid color" << colorName
                           << "on item" << itemTypeId << itemId;
        } else {
            qWarning() << "  > Parsing part_color_codes: skipping invalid item" << itemTypeId << itemId;
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


bool BrickLink::TextImport::importInventories(std::vector<bool> &processedInvs,
                                              ImportInventoriesStep step)
{
    for (uint itemIndex = 0; itemIndex < m_db->m_items.size(); ++itemIndex) {
        if (processedInvs[itemIndex]) // already yanked
            continue;

        bool hasInventory = (m_inventoryLastUpdated.value(itemIndex, -1) >= 0);

        if (!hasInventory || readInventory(&m_db->m_items[itemIndex], step))
            processedInvs[itemIndex] = true;
    }
    return true;
}

bool BrickLink::TextImport::readInventory(const Item *item, ImportInventoriesStep step)
{
    std::unique_ptr<QFile> f(BrickLink::core()->dataReadFile(u"inventory.xml", item));

    if (!f || !f->isOpen())
        return false;

    QDateTime fileTime = f->fileTime(QFileDevice::FileModificationTime);
    QDate fileDate = fileTime.date();
    uint itemIndex = uint(item - items().data());

    if (fileTime.toSecsSinceEpoch() < m_inventoryLastUpdated.value(itemIndex, -1))
        return false;

    QVector<Item::ConsistsOf> inventory;
    QVector<QPair<int, int>> knownColors;

    try {
        f->close();
        xmlParse(f->fileName(), u"INVENTORY"_qs, u"ITEM"_qs,
                 [this, &inventory, &knownColors, fileDate](QDomElement e) {
            char itemTypeId = ItemType::idFromFirstCharInString(xmlTagText(e, "ITEMTYPE"));
            const QByteArray itemId = xmlTagText(e, "ITEMID").toLatin1();
            uint colorId = xmlTagText(e, "COLOR").toUInt();
            int qty = xmlTagText(e, "QTY").toInt();
            bool extra = (xmlTagText(e, "EXTRA") == u"Y");
            bool counterPart = (xmlTagText(e, "COUNTERPART") == u"Y");
            bool alternate = (xmlTagText(e, "ALTERNATE") == u"Y");
            uint matchId = xmlTagText(e, "MATCHID").toUInt();

            auto item = core()->item(itemTypeId, itemId);
            auto color = core()->color(colorId);

            if (!item)
                throw Exception("Unknown item-id %1 %2").arg(itemTypeId).arg(QString::fromLatin1(itemId));
            if (!color)
                throw Exception("Unknown color-id %1").arg(colorId);
            if (!qty)
                throw Exception("Invalid Quantity %1").arg(qty);

            int itemIndex = item->index();
            int colorIndex = color->index();

            Item::ConsistsOf co;
            co.m_quantity = qty;
            co.m_itemIndex = itemIndex;
            co.m_colorIndex = colorIndex;
            co.m_extra = extra;
            co.m_isalt = alternate;
            co.m_altid = matchId;
            co.m_cpart = counterPart;

            // if this itemid was involved in a changelog entry after the last time we downloaded
            // the inventory, we need to reload
            QByteArray itemTypeAndId = itemTypeId + itemId;
            auto [lit, uit] = std::equal_range(m_db->m_itemChangelog.cbegin(), m_db->m_itemChangelog.cend(), itemTypeAndId);
            for (auto it = lit; it != uit; ++it) {
                if (it->date() > fileDate) {
                    throw Exception("Item id %1 changed on %2 (last download: %3)")
                        .arg(QString::fromLatin1(itemTypeAndId))
                        .arg(it->date().toString(u"yyyy/MM/dd"))
                        .arg(fileDate.toString(u"yyyy/MM/dd"));
                }
            }

            inventory.append(co);
            knownColors.append({ itemIndex, colorIndex});

        });

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
                vec.append(qMakePair(co.quantity(), itemIndex));
            }
        }
        // the hash owns the items now
        m_consists_of_hash.insert(itemIndex, inventory);
        return true;

    } catch (const Exception &e) {
        if (step != ImportFromDiskCache)
            qWarning() << "  >" << qPrintable(e.errorString());
        return false;
    }
}

void BrickLink::TextImport::readLDrawColors(const QString &ldconfigPath, const QString &rebrickableColorsPath)
{
    QFile fre(rebrickableColorsPath);
    if (!fre.open(QFile::ReadOnly))
        throw ParseException(&fre, "could not open file");

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(fre.readAll(), &err);
    if (doc.isNull())
        throw ParseException(&fre, "Invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

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

    QFile fld(ldconfigPath);
    if (!fld.open(QFile::ReadOnly))
        throw ParseException(&fld, "could not open file");

    QTextStream in(&fld);
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
                    ; // ignore
                } else if (sl[idx] == u"MATTE_METALLIC") {
                    ; // ignore
                } else if (sl[idx] == u"METAL") {
                    type.setFlag(ColorTypeFlag::Metallic);
                } else if ((sl[idx] == u"MATERIAL") && (idx < lastIdx)) {
                    const auto mat = sl[idx + 1];

                    if (mat == u"GLITTER")
                        type.setFlag(name.startsWith(u"Opal_") ? ColorTypeFlag::Satin : ColorTypeFlag::Glitter);
                    else if (mat == u"SPECKLE")
                        type.setFlag(ColorTypeFlag::Speckle);
                    else
                        qWarning() << "Found unsupported MATERIAL" << mat << "at line" << lineno << "of LDConfig.ldr";
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

                if (!name.startsWith(u"Rubber_"))
                    qWarning() << "  > Could not match LDraw color" << id << name << "to any BrickLink color";
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

            if (!c.isModulex())
                qWarning() << "  > Could not match BrickLink color" << c.id() << c.name() << "to any LDraw color" << c.m_ldraw_color;
        }
    }
}

void BrickLink::TextImport::readInventoryList(const QString &path)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    QTextStream ts(&f);
    int lineNumber = 0;
    while (!ts.atEnd()) {
        ++lineNumber;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split(u'\t');

        if (strs.count() < 2)
            throw ParseException(&f, "expected at least 2 fields in line %1").arg(lineNumber);

        char itemTypeId = ItemType::idFromFirstCharInString(strs.at(0));
        const QByteArray itemId = strs.at(1).toLatin1();

        if (!itemTypeId || itemId.isEmpty())
            throw ParseException(&f, "expected a valid item-type and an item-id field in line %1").arg(lineNumber);

        auto item = core()->item(itemTypeId, itemId);
        if (item) {
            int itemIndex = item->index();
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
            qWarning() << "  > btinvlist: item" << itemTypeId << itemId << "doesn't exist!";
        }
    }
}

void BrickLink::TextImport::readChangeLog(const QString &path)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    m_db->m_latestChangelogId = 0;

    QTextStream ts(&f);
    int lineNumber = 0;
    while (!ts.atEnd()) {
        ++lineNumber;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split(u'\t');

        if (strs.count() < 7)
            throw ParseException(&f, "expected at least 7 fields in line %1").arg(lineNumber);

        uint id = strs.at(0).toUInt();
        QDate date = QDate::fromString(strs.at(1), u"M/d/yyyy"_qs);
        char c = ItemType::idFromFirstCharInString(strs.at(2));

        switch (c) {
        case 'I':   // ItemId
        case 'T':   // ItemType
        case 'M': { // ItemMerge
            const QString &fromType = strs.at(3);
            const QString &fromId = strs.at(4);
            const QString &toType = strs.at(5);
            const QString &toId = strs.at(6);
            if ((fromType.length() == 1) && (toType.length() == 1)
                    && !fromId.isEmpty() && !toId.isEmpty()) {
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
            qWarning("  > Parsing btchglog: skipping invalid change code %x", uint(c));
            break;
        }
    }
    std::sort(m_db->m_colorChangelog.begin(), m_db->m_colorChangelog.end());
    std::sort(m_db->m_itemChangelog.begin(), m_db->m_itemChangelog.end());
}

void BrickLink::TextImport::readRelationships(const QString &path)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    auto data = QString::fromUtf8(f.readAll());

    static const QRegularExpression rxLink(uR"-(<A HREF="catalogRelCat\.asp\?relID=(\d+)">([^<]+)</A>)-"_qs);

    QNetworkAccessManager nam;

    for (const auto &m : rxLink.globalMatch(data)) {
        uint id = m.captured(1).toUInt();
        QString name = m.captured(2);

        Relationship rel;
        rel.m_id = id;
        rel.m_name.copyQString(name, nullptr);

        QHash<uint, std::vector<uint>> matches;   // match-id -> list of item-indexes

        uint page = 1;
        do {
            QString url = u"https://www.bricklink.com/catalogRelList.asp?v=0&relID=%1&pg=%2"_qs.arg(id).arg(page);
            auto reply = nam.get(QNetworkRequest(url));
            while (!reply->isFinished())
                QCoreApplication::processEvents();
            const auto listData = QString::fromUtf8(reply->readAll());
            delete reply;

            static const QRegularExpression rxHeader(uR"(<B>(\d+)</B> Matches found: Page <B>(\d+)</B> of <B>(\d+)</B>)"_qs);
            auto mh = rxHeader.match(listData);

            if (!mh.hasMatch())
                throw Exception("Relationships: couldn't find list header");

            uint count = mh.captured(1).toUInt();
            uint currentPage = mh.captured(2).toUInt();
            uint maxPage = mh.captured(3).toUInt();

            if (page == 1)
                rel.m_count = count;
            else if (rel.m_count != count)
                throw Exception("Relationships: count mismatch between pages: expected %1, but got %2 on page %3").arg(rel.m_count).arg(count).arg(page);
            if (currentPage != page)
                throw Exception("Relationships: wrong page: expected %1, but got %2").arg(page).arg(currentPage);

            auto startPos = mh.capturedEnd();
            auto endPos = listData.indexOf(u"</TABLE>"_qs, startPos);

            static const QRegularExpression rxTR(uR"-(<TR .*?</TR>)-"_qs);
            static const QRegularExpression rxMatch(uR"-(<TR BGCOLOR="#......"><TD COLSPAN="4">.*?<B>Match #(\d+)</B></FONT></TD></TR>)-"_qs);
            static const QRegularExpression rxItem (uR"-(<TR BGCOLOR="#......"><TD ALIGN="CENTER" WIDTH="10%">.*?<A HREF="/v2/catalog/catalogitem\.page\?([A-Z])=([A-Za-z0-9._-]+)">([A-Za-z0-9._-]+)</A>.*</FONT></TD></TR>)-"_qs);

            uint currentMatchId = 0;
            int trCount = 0;

            while (true) {
                auto mt = rxTR.match(listData, startPos);
                if (!mt.hasMatch())
                    break;

                int matchStartPos = mt.capturedStart(0);
                int matchEndPos = mt.capturedEnd(0);
                if (matchStartPos > endPos || matchEndPos > endPos)
                    break;
                startPos = matchEndPos;

                if (++trCount == 1) // skip header
                    continue;

                auto currentRow = mt.capturedView(0);

                auto mm = rxMatch.match(currentRow);
                if (mm.hasMatch()) {
                    uint matchId = mm.captured(1).toUInt();
                    currentMatchId = matchId;
                } else {
                    if (!currentMatchId)
                        throw Exception("Relationships: got an item row without a preceeding match # row");

                    auto mi = rxItem.match(currentRow);
                    if (mi.hasMatch()) {
                        QString ittId = mi.captured(1);
                        QString itemId = mi.captured(2);
                        QString itemId2 = mi.captured(3);
                        if (itemId != itemId2)
                            throw Exception("Relationships: item ids do not match up: %1 vs. %2").arg(itemId).arg(itemId2);
                        if (ittId.size() != 1)
                            throw Exception("Relationships: invalid item-type id: %1").arg(ittId);
                        auto item = core()->item(ittId.at(0).toLatin1(), itemId.toLatin1());
                        if (!item) {
                            qWarning() << "  > Relationships: could not resolve item:"
                                       << ittId << itemId << "for match id" << currentMatchId
                                       << "in" << rel.name();
                        } else {
                            matches[currentMatchId].push_back(uint(item->index()));
                        }
                    } else {
                        qWarning().noquote() << currentRow;
                        throw Exception("Relationships: Found a TR that is neither an item nor a match id");
                    }
                }
            }

            page = (page < maxPage) ? (page + 1) : 0;
        } while (page);

        if (rel.m_count != matches.count()) {
            qWarning() << "  > Relationships:" << rel.name() << "should have" << rel.m_count << "entries, but has" << matches.count();
            rel.m_count = matches.count();
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
    for (const auto &relMatch : m_db->m_relationshipMatches) {
        if (relMatch.m_id > 0xffffu) {
            qWarning() << "  > relationship match id" << relMatch.m_id << "exceeds 16 bits";
            continue;
        }
        for (const auto &itemIndex : relMatch.m_itemIndexes) {
            Item &item = m_db->m_items[itemIndex];
            item.m_relationshipMatchIds.push_back(quint16(relMatch.m_id), nullptr);
        }
    }
}

void BrickLink::TextImport::finalizeDatabase()
{
    for (auto it = m_consists_of_hash.cbegin(); it != m_consists_of_hash.cend(); ++it) {
        Item &item = m_db->m_items[it.key()];
        const auto &coItems = it.value();
        item.m_consists_of.copyContainer(coItems.cbegin(), coItems.cend(), nullptr);
    }

    for (auto it = m_appears_in_hash.cbegin(); it != m_appears_in_hash.cend(); ++it) {
        Item &item = m_db->m_items[it.key()];

        // color-idx -> { vector < qty, item-idx > }
        const QHash<uint, QVector<QPair<int, uint>>> &appearHash = it.value();

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
}

void BrickLink::TextImport::setApiKeys(const QHash<QByteArray, QString> &apiKeys)
{
    m_db->m_apiKeys = apiKeys;
}

void BrickLink::TextImport::calculateColorPopularity()
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

void BrickLink::TextImport::calculateItemTypeCategories()
{
    for (const auto &item : m_db->m_items) {
        // calculate the item-type -> category relation
        ItemType &itemType = m_db->m_itemTypes[item.m_itemTypeIndex];
        auto &catv = itemType.m_categoryIndexes;

        for (quint16 catIndex : item.m_categoryIndexes) {
            if (std::find(catv.cbegin(), catv.cend(), catIndex) == catv.cend())
                catv.push_back(catIndex, nullptr);
        }
    }
}

void BrickLink::TextImport::calculateKnownAssemblyColors()
{
    for (auto it = m_appears_in_hash.begin(); it != m_appears_in_hash.end(); ++it) {
        const Item &item = m_db->m_items[it.key()];
        const ItemType &itemType = m_db->m_itemTypes[item.m_itemTypeIndex];

        if (!it->contains(0) || !itemType.hasColors())
            continue;

        // The type supports colors, but we have entries for color "not available"
        //   -> check assemblies

        // We need to copy here, as we modify (*it) later on and this would invalidate noColor
        QVector<QPair<int, uint>> noColor = it->value(0);

        for (auto ncit = noColor.begin(); ncit != noColor.end(); ) {
            int ncQty = ncit->first;
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

const std::vector<BrickLink::Item> &BrickLink::TextImport::items() const
{
    return m_db->m_items;
}

void BrickLink::TextImport::calculateCategoryRecency()
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

void BrickLink::TextImport::calculatePartsYearUsed()
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

                for (const BrickLink::Item::ConsistsOf &part : itemParts) {
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

void BrickLink::TextImport::addToKnownColors(int itemIndex, int addColorIndex)
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
