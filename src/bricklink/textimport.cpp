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
#include <cstdlib>
#include <ctime>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QtDebug>
#include <QtCore/QTimeZone>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include "utility/exception.h"
#include "utility/xmlhelpers.h"
#include "bricklink/core.h"
#include "bricklink/textimport.h"
#include "bricklink/partcolorcode.h"
#include "bricklink/changelogentry.h"


int BrickLink::TextImport::findItemIndex(char tid, const QByteArray &id) const
{
    auto needle = std::make_pair(tid, id);
    auto it = std::lower_bound(m_items.cbegin(), m_items.cend(), needle, Item::lessThan);
    if ((it != m_items.cend()) && (it->itemTypeId() == tid) && (it->id() == id))
        return int(std::distance(m_items.cbegin(), it));
    return -1;
}

int BrickLink::TextImport::findColorIndex(uint id) const
{
    auto it = std::lower_bound(m_colors.cbegin(), m_colors.cend(), id, Color::lessThan);
    if ((it != m_colors.cend()) && (it->id() == id))
        return int(std::distance(m_colors.cbegin(), it));
    return -1;
}

int BrickLink::TextImport::findCategoryIndex(uint id) const
{
    auto it = std::lower_bound(m_categories.cbegin(), m_categories.cend(), id, Category::lessThan);
    if ((it != m_categories.cend()) && (it->id() == id))
        return int(std::distance(m_categories.cbegin(), it));
    return -1;
}


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


BrickLink::TextImport::TextImport()
{ }

BrickLink::TextImport::~TextImport()
{ }

bool BrickLink::TextImport::import(const QString &path)
{
    try {
        // colors
        readColors(path % u"colors.xml");
        readLDrawColors(path % u"ldconfig.ldr", path % u"rebrickable_colors.json");
        calculateColorPopularity();

        // categories
        readCategories(path % u"categories.xml");

        // itemtypes
        readItemTypes(path % u"itemtypes.xml");

        // speed up loading (exactly 137522 items on 16.06.2020)
        m_items.reserve(200000);

        for (ItemType &itt : m_item_types) {
            readItems(path % u"items_" % QLatin1Char(itt.m_id) % u".xml", &itt);
            readAdditionalItemCategories(path % u"items_" % QLatin1Char(itt.m_id) % u".csv", &itt);
        }

        readPartColorCodes(path % u"part_color_codes.xml");
        readInventoryList(path % u"btinvlist.csv");
        readChangeLog(path % u"btchglog.csv");

        return true;
    } catch (const Exception &e) {
        qWarning() << "Error importing database:" << e.what();
        return false;
    }
}


void BrickLink::TextImport::readColors(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p](QDomElement e) {
        Color col;
        uint colid = p.elementText(e, "COLOR").toUInt();

        col.m_id       = colid;
        col.m_name     = p.elementText(e, "COLORNAME").simplified();
        col.m_color    = QColor(u'#' % p.elementText(e, "COLORRGB"));

        col.m_ldraw_id = -1;
        col.m_type     = Color::Type();

        auto type = p.elementText(e, "COLORTYPE");
        if (type.contains("Transparent"_l1)) col.m_type |= Color::Transparent;
        if (type.contains("Glitter"_l1))     col.m_type |= Color::Glitter;
        if (type.contains("Speckle"_l1))     col.m_type |= Color::Speckle;
        if (type.contains("Metallic"_l1))    col.m_type |= Color::Metallic;
        if (type.contains("Chrome"_l1))      col.m_type |= Color::Chrome;
        if (type.contains("Pearl"_l1))       col.m_type |= Color::Pearl;
        if (type.contains("Milky"_l1))       col.m_type |= Color::Milky;
        if (type.contains("Modulex"_l1))     col.m_type |= Color::Modulex;
        if (type.contains("Satin"_l1))       col.m_type |= Color::Satin;
        if (!col.m_type)
            col.m_type = Color::Solid;

        int partCnt    = p.elementText(e, "COLORCNTPARTS").toInt();
        int setCnt     = p.elementText(e, "COLORCNTSETS").toInt();
        int wantedCnt  = p.elementText(e, "COLORCNTWANTED").toInt();
        int forSaleCnt = p.elementText(e, "COLORCNTINV").toInt();

        col.m_popularity = float(partCnt + setCnt + wantedCnt + forSaleCnt);

        // needs to be divided by the max. after all colors are parsed!
        // mark it as raw data meanwhile:
        col.m_popularity = -col.m_popularity;

        col.m_year_from = p.elementText(e, "COLORYEARFROM").toUShort();
        col.m_year_to   = p.elementText(e, "COLORYEARTO").toUShort();

        m_colors.push_back(col);
    });

    std::sort(m_colors.begin(), m_colors.end(), [](const auto &col1, const auto &col2) {
        return Color::lessThan(col1, col2.id());
    });
}

void BrickLink::TextImport::readCategories(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p](QDomElement e) {
        Category cat;
        uint catid = p.elementText(e, "CATEGORY").toUInt();

        cat.m_id   = catid;
        cat.m_name = p.elementText(e, "CATEGORYNAME").simplified();

        m_categories.push_back(cat);
    });

    std::sort(m_categories.begin(), m_categories.end(), [](const auto &cat1, const auto &cat2) {
        return Category::lessThan(cat1, cat2.id());
    });

}

void BrickLink::TextImport::readAdditionalItemCategories(const QString &path, BrickLink::ItemType *itt)
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
        QStringList strs = line.split('\t'_l1);

        if (strs.count() < 3)
            throw ParseException(&f, "expected at least 2 fields in line %1").arg(lineNumber);

        const QByteArray itemId = strs.at(2).toLatin1();
        int itemIndex = findItemIndex(itt->m_id, itemId);

        if (itemIndex == -1)
            throw ParseException(&f, "expected a valid item-id field in line %1").arg(lineNumber);
        if (m_items.at(itemIndex).m_categoryIndex == -1)
            throw ParseException(&f, "item has no main category in line %1").arg(lineNumber);

        QString catStr = strs.at(1).simplified();
        const Category &mainCat = m_categories.at(m_items.at(itemIndex).m_categoryIndex);
        if (!catStr.startsWith(mainCat.name()))
            throw ParseException(&f, "additional categories do not start with the main category in line %1").arg(lineNumber);
        if (strs.at(0).toUInt() != mainCat.m_id)
            throw ParseException(&f, "the main category id does not match the XML in line %1").arg(lineNumber);

        catStr = catStr.mid(mainCat.name().length() + 3);

        const QStringList cats = catStr.split(" / "_l1);
        for (int i = 0; i < cats.count(); ++i) {
            for (qint16 catIndex = 0; catIndex < qint16(m_categories.size()); ++catIndex) {
                const QString catName = m_categories.at(catIndex).name();
                bool disambiguate = catName.contains(" / "_l1);
                qint16 addCatIndex = -1;

                // The " / " sequence is used to separate the fields, but it also appears in
                // category names like "String Reel / Winch"

                if (disambiguate) {
                    const auto catNameList = catName.split(" / "_l1);
                    if (catNameList == cats.mid(i, catNameList.size())) {
                        addCatIndex = catIndex;
                        i += (catNameList.size() - 1);
                    }
                } else if (catName == cats.at(i)) {
                    addCatIndex = catIndex;
                }

                if (addCatIndex != -1)
                    m_items[itemIndex].m_additionalCategoryIndexes.push_back(addCatIndex);
            }
        }
    }
}

void BrickLink::TextImport::readItemTypes(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p](QDomElement e) {
        ItemType itt;
        char c = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));

        if (c == 'U')
            return;

        itt.m_id   = c;
        itt.m_name = p.elementText(e, "ITEMTYPENAME").simplified();

        itt.m_has_inventories   = false;
        itt.m_has_colors        = (c == 'P' || c == 'G');
        itt.m_has_weight        = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
        itt.m_has_subconditions = (c == 'S');

        m_item_types.push_back(itt);
    });

    std::sort(m_item_types.begin(), m_item_types.end(), [](const auto &itt1, const auto &itt2) {
        return ItemType::lessThan(itt1, itt2.id());
    });
}

void BrickLink::TextImport::readItems(const QString &path, BrickLink::ItemType *itt)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p, itt](QDomElement e) {
        Item item;
        item.m_id = p.elementText(e, "ITEMID").toLatin1();
        item.m_name = p.elementText(e, "ITEMNAME").simplified();
        item.m_itemTypeIndex = (itt - m_item_types.data());
        item.m_itemTypeId = itt->id();

        uint catId = p.elementText(e, "CATEGORY").toUInt();
        item.m_categoryIndex = findCategoryIndex(catId);
        if (item.m_categoryIndex == -1)
            throw ParseException("item %1 has no category").arg(QLatin1String(item.m_id));

        uint y = p.elementText(e, "ITEMYEAR", "0").toUInt();
        item.m_year_from = ((y > 1900) && (y < 2155)) ? (y - 1900) : 0; // we only have 8 bits for the year
        item.m_year_to = item.m_year_from;

        if (itt->hasWeight())
            item.m_weight = p.elementText(e, "ITEMWEIGHT").toFloat();
        else
            item.m_weight = 0;

        try {
            item.m_defaultColorIndex = findColorIndex(p.elementText(e, "IMAGECOLOR").toUInt());
        } catch (...) {
            item.m_defaultColorIndex = -1;
        }

        m_items.push_back(item);
    });

    std::sort(m_items.begin(), m_items.end(), [](const auto &item1, const auto &item2) {
        return Item::lessThan(item1, std::make_pair(item2.itemTypeId(), item2.id()));
    });
}

void BrickLink::TextImport::readPartColorCodes(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CODES", "ITEM");
    p.parse([this, &p](QDomElement e) {
        char itemTypeId = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));
        const QByteArray itemId = p.elementText(e, "ITEMID").toLatin1();
        const QString colorName = p.elementText(e, "COLOR").simplified();
        bool numeric = false;
        uint code = p.elementText(e, "CODENAME").toUInt(&numeric, 10);

        if (!numeric) {
            qWarning() << "Parsing part_color_codes: pcc" << p.elementText(e, "CODENAME") << "is not numeric";
        }

        int itemIndex = findItemIndex(itemTypeId, itemId);
        if (itemIndex != -1) {
            bool found = false;
            for (uint colorIndex = 0; colorIndex < m_colors.size(); ++colorIndex) {
                if (m_colors[colorIndex].name() == colorName) {
                    addToKnownColors(itemIndex, colorIndex);

                    if (numeric) {
                        PartColorCode pcc;
                        pcc.m_id = code;
                        pcc.m_itemIndex = itemIndex;
                        pcc.m_colorIndex = colorIndex;
                        m_pccs.push_back(pcc);
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
                qWarning() << "Parsing part_color_codes: skipping invalid color" << colorName
                           << "on item" << itemTypeId << itemId;
        } else {
            qWarning() << "Parsing part_color_codes: skipping invalid item" << itemTypeId << itemId;
        }
    });

    std::sort(m_pccs.begin(), m_pccs.end(), [](const auto &item1, const auto &item2) {
        return PartColorCode::lessThan(item1, item2.id());
    });
}


bool BrickLink::TextImport::importInventories(std::vector<bool> &processedInvs)
{
    for (uint i = 0; i < m_items.size(); ++i) {
        if (processedInvs[i]) // already yanked
            continue;

        if (!m_items[i].hasInventory() || readInventory(&m_items[i]))
            processedInvs[i] = true;
    }
    return true;
}

bool BrickLink::TextImport::readInventory(const Item *item)
{
    std::unique_ptr<QFile> f(BrickLink::core()->dataReadFile(u"inventory.xml", item));

    if (!f || !f->isOpen() || (f->fileTime(QFileDevice::FileModificationTime) < item->inventoryUpdated()))
        return false;

    QVector<Item::ConsistsOf> inventory;

    try {
        XmlHelpers::ParseXML p(f.release(), "INVENTORY", "ITEM");
        p.parse([this, &p, &inventory](QDomElement e) {
            char itemTypeId = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));
            const QByteArray itemId = p.elementText(e, "ITEMID").toLatin1();
            uint colorId = p.elementText(e, "COLOR").toUInt();
            int qty = p.elementText(e, "QTY").toInt();
            bool extra = (p.elementText(e, "EXTRA") == "Y"_l1);
            bool counterPart = (p.elementText(e, "COUNTERPART") == "Y"_l1);
            bool alternate = (p.elementText(e, "ALTERNATE") == "Y"_l1);
            uint matchId = p.elementText(e, "MATCHID").toUInt();

            int itemIndex = findItemIndex(itemTypeId, itemId);
            int colorIndex = findColorIndex(colorId);

            if ((itemIndex == -1) || (colorIndex == -1) || !qty)
                throw Exception("Unknown item-id or color-id or 0 qty");

            Item::ConsistsOf co;
            co.m_qty = qty;
            co.m_itemIndex = itemIndex;
            co.m_colorIndex = colorIndex;
            co.m_extra = extra;
            co.m_isalt = alternate;
            co.m_altid = matchId;
            co.m_cpart = counterPart;

            inventory.append(co);

            addToKnownColors(itemIndex, colorIndex);
        });

        uint itemIndex = uint(item - items().data());


        // BL bug: if an extra item is part of an alternative match set, then none of the
        //         alternatives have the 'extra' flag set.
        for (Item::ConsistsOf &co : inventory) {
            if (co.m_isalt && !co.m_extra) {
                for (const Item::ConsistsOf &mainCo : qAsConst(inventory)) {
                    if ((mainCo.m_altid == co.m_altid) && !mainCo.m_isalt) {
                        co.m_extra = mainCo.m_extra;
                        break;
                    }
                }
            }
        }
        // pre-sort to have a nice sorting order, even in unsorted views
        std::sort(inventory.begin(), inventory.end(), [](const auto &co1, const auto &co2) {
            if (co1.m_extra != co2.m_extra)
                return co1.m_extra < co2.m_extra;
            else if (co1.m_cpart != co2.m_cpart)
                return co1.m_cpart < co2.m_cpart;
            else if (co1.m_altid != co2.m_altid)
                return co1.m_altid < co2.m_altid;
            else if (co1.m_isalt != co2.m_isalt)
                return co1.m_isalt < co2.m_isalt;
            else
                return co1.m_itemIndex < co2.m_itemIndex;
        });

        for (const Item::ConsistsOf &co : qAsConst(inventory)) {
            if (!co.m_extra) {
                auto &vec = m_appears_in_hash[co.m_itemIndex][co.m_colorIndex];
                vec.append(qMakePair(co.quantity(), itemIndex));
            }
        }
        // the hash owns the items now
        m_consists_of_hash.insert(itemIndex, inventory);
        return true;

    } catch (const Exception &) {
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
    const QJsonArray results = doc["results"_l1].toArray();
    for (const auto &&result : results) {
        const auto blIds = result["external_ids"_l1]["BrickLink"_l1]["ext_ids"_l1].toArray();
        const auto ldIds = result["external_ids"_l1]["LDraw"_l1]["ext_ids"_l1].toArray();

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

        QStringList sl = line.simplified().split(' '_l1);

        if (sl.count() >= 9 &&
                sl[0].toInt() == 0 &&
                sl[1] == "!COLOUR"_l1 &&
                sl[3] == "CODE"_l1 &&
                sl[5] == "VALUE"_l1 &&
                sl[7] == "EDGE"_l1) {
            // 0 !COLOUR name CODE x VALUE v EDGE e [ALPHA a] [LUMINANCE l] [ CHROME | PEARLESCENT | RUBBER | MATTE_METALLIC | METAL | MATERIAL <params> ]

            QString name = sl[2];
            int id = sl[4].toInt();
            QColor color = sl[6];
            QColor edgeColor = sl[8];
            float luminance = 0;
            Color::Type type = Color::Solid;
            float particleMinSize = 0;
            float particleMaxSize = 0;
            float particleFraction = 0;
            float particleVFraction = 0;
            QColor particleColor;

            if (id == 16 || id == 24)
                continue;

            bool isRubber = name.startsWith("Rubber_"_l1);

            int lastIdx = int(sl.count()) - 1;

            for (int idx = 9; idx < sl.count(); ++idx) {
                if (sl[idx] == "ALPHA"_l1 && (idx < lastIdx)) {
                    int alpha = sl[++idx].toInt();
                    color.setAlpha(alpha);
                    type.setFlag(Color::Solid, false);
                    type.setFlag(Color::Transparent);
                } else if (sl[idx] == "LUMINANCE"_l1 && (idx < lastIdx)) {
                    luminance = float(sl[++idx].toInt()) / 255;
                } else if (sl[idx] == "CHROME"_l1) {
                    type.setFlag(Color::Chrome);
                } else if (sl[idx] == "PEARLESCENT"_l1) {
                    type.setFlag(Color::Pearl);
                } else if (sl[idx] == "RUBBER"_l1) {
                    ; // ignore
                } else if (sl[idx] == "MATTE_METALLIC"_l1) {
                    ; // ignore
                } else if (sl[idx] == "METAL"_l1) {
                    type.setFlag(Color::Metallic);
                } else if (sl[idx] == "MATERIAL"_l1 && (idx < lastIdx)) {
                    const auto mat = sl[idx + 1];

                    if (mat == "GLITTER"_l1)
                        type.setFlag(name.startsWith("Opal_"_l1) ? Color::Satin : Color::Glitter);
                    else if (mat == "SPECKLE"_l1)
                        type.setFlag(Color::Speckle);
                    else
                        qWarning() << "Found unsupported MATERIAL" << mat << "at line" << lineno << "of LDConfig.ldr";
                } else if (sl[idx] == "SIZE"_l1 && (idx < lastIdx)) {
                    particleMinSize = particleMaxSize = sl[++idx].toFloat();
                } else if (sl[idx] == "MINSIZE"_l1 && (idx < lastIdx)) {
                    particleMinSize = sl[++idx].toFloat();
                } else if (sl[idx] == "MAXSIZE"_l1 && (idx < lastIdx)) {
                    particleMaxSize = sl[++idx].toFloat();
                } else if (sl[idx] == "VALUE"_l1 && (idx < lastIdx)) {
                    particleColor = sl[++idx];
                } else if (sl[idx] == "FRACTION"_l1 && (idx < lastIdx)) {
                    particleFraction = sl[++idx].toFloat();
                } else if (sl[idx] == "VFRACTION"_l1 && (idx < lastIdx)) {
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
            for (auto &c : m_colors) {
                QString ldName = name.toLower();
                QString blName = c.name()
                        .toLower()
                        .replace(' '_l1, '_'_l1)
                        .replace('-'_l1, '_'_l1)
                        .replace("gray_"_l1, "grey_"_l1)
                        .replace("satin_"_l1, "opal_"_l1)
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
                    { "Trans_Bright_Light_Green"_l1, "Trans-Light Bright Green"_l1 },
                };

                QString blName = manualLDrawToBrickLink.value(name);
                if (!blName.isEmpty()) {
                    for (auto &c : m_colors) {
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
                    int colorIndex = findColorIndex(blColorId);
                    if (colorIndex >= 0) {
                        updateColor(m_colors[colorIndex]);
                        found = true;
                    }
                }
            }

            // We can't map the color to a BrickLink one, but at least we keep a record of it to
            // be able to render composite parts with fixed colors (e.g. electric motors)
            if (!found) {
                Color c;
                c.m_name = "LDraw: "_l1 % name;
                c.m_type = type;
                c.m_color = color;
                updateColor(c);

                m_ldrawExtraColors.push_back(c);

                if (!name.startsWith("Rubber_"_l1))
                    qWarning() << "Could not match LDraw color" << id << name << "to any BrickLink color";
            }
        }
    }

    for (auto &c : m_colors) {
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
                qWarning() << "Could not match BrickLink color" << c.m_id << c.m_name << "to any LDraw color" << c.m_ldraw_color;
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
        QStringList strs = line.split('\t'_l1);

        if (strs.count() < 2)
            throw ParseException(&f, "expected at least 2 fields in line %1").arg(lineNumber);

        char itemTypeId = XmlHelpers::firstCharInString(strs.at(0));
        const QByteArray itemId = strs.at(1).toLatin1();

        if (!itemTypeId || itemId.isEmpty())
            throw ParseException(&f, "expected a valid item-type and an item-id field in line %1").arg(lineNumber);

        int itemIndex = findItemIndex(itemTypeId, itemId);
        if (itemIndex != -1) {
            qint64 t(0);   // 1.1.1970 00:00

            if (strs.count() > 2) {
                QString dtStr = strs.at(2);
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
            Item &item = m_items[itemIndex];
            item.m_lastInventoryUpdate = t;

            m_item_types[item.m_itemTypeIndex].m_has_inventories = true;

            auto catIndexes = item.m_additionalCategoryIndexes;
            catIndexes.push_back(item.m_categoryIndex);
            Q_ASSERT(item.m_itemTypeIndex < 8);
            for (const auto &catIndex : catIndexes)
                m_categories[catIndex].m_has_inventories |= (quint8(1) << item.m_itemTypeIndex);
        } else {
            qWarning() << "WARNING: parsing btinvlist: item" << itemTypeId << itemId << "doesn't exist!";
        }
    }
}

void BrickLink::TextImport::readChangeLog(const QString &path)
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
        QStringList strs = line.split('\t'_l1);

        if (strs.count() < 7)
            throw ParseException(&f, "expected at least 7 fields in line %1").arg(lineNumber);

        char c = XmlHelpers::firstCharInString(strs.at(2));

        switch (c) {
        case 'I':   // ItemId
        case 'T':   // ItemType
        case 'M': { // ItemMerge
            QString fromType = strs.at(3);
            QString fromId = strs.at(4);
            QString toType = strs.at(5);
            QString toId = strs.at(6);
            if ((fromType.length() == 1) && (toType.length() == 1)
                    && !fromId.isEmpty() && !toId.isEmpty()) {
                m_itemChangelog.emplace_back((fromType % fromId).toLatin1(),
                                             (toType % toId).toLatin1());
            }
            break;
        }
        case 'A': { // ColorMerge
            bool fromOk = false, toOk = false;
            uint fromId = strs.at(3).toUInt(&fromOk);
            uint toId = strs.at(5).toUInt(&toOk);
            if (fromOk && toOk)
                m_colorChangelog.emplace_back(fromId, toId);
            break;
        }
        case 'E':   // CategoryName
        case 'G':   // CategoryMerge
        case 'C':   // ColorName
            break;
        default:
            qWarning("Parsing btchglog: skipping invalid change code %x", uint(c));
            break;
        }
    }
    std::sort(m_colorChangelog.begin(), m_colorChangelog.end());
    std::sort(m_itemChangelog.begin(), m_itemChangelog.end());
}

void BrickLink::TextImport::exportTo(Database *db)
{
    std::swap(db->m_colors, m_colors);
    std::swap(db->m_ldrawExtraColors, m_ldrawExtraColors);
    std::swap(db->m_itemTypes, m_item_types);
    std::swap(db->m_categories, m_categories);
    std::swap(db->m_items, m_items);
    std::swap(db->m_pccs, m_pccs);
    std::swap(db->m_itemChangelog, m_itemChangelog);
    std::swap(db->m_colorChangelog, m_colorChangelog);

    for (auto it = m_consists_of_hash.cbegin(); it != m_consists_of_hash.cend(); ++it) {
        Item &item = db->m_items[it.key()];
        item.setConsistsOf(it.value());
    }

    for (auto it = m_appears_in_hash.cbegin(); it != m_appears_in_hash.cend(); ++it) {
        Item &item = db->m_items[it.key()];
        item.setAppearsIn(it.value());
    }
}

void BrickLink::TextImport::calculateColorPopularity()
{
    float maxpop = 0;
    for (const auto &col : m_colors)
        maxpop = qMin(maxpop, col.m_popularity);
    for (auto &col : m_colors) {
        float &pop = col.m_popularity;

        if (!qFuzzyIsNull(maxpop))
            pop /= maxpop;
        else
            pop = 0;
    }
}

void BrickLink::TextImport::calculateItemTypeCategories()
{
    for (const auto &item : m_items) {
        // calculate the item-type -> category relation
        auto catIndexes = item.m_additionalCategoryIndexes;
        catIndexes.push_back(item.m_categoryIndex);

        ItemType &itemType = m_item_types[item.m_itemTypeIndex];
        auto &catv = itemType.m_categoryIndexes;

        for (const auto &catIndex : catIndexes) {
            if (std::find(catv.cbegin(), catv.cend(), catIndex) == catv.cend())
                catv.push_back(catIndex);
        }
    }
}

void BrickLink::TextImport::calculateCategoryRecency()
{
    QHash<uint, std::pair<quint64, quint32>> catCounter;

    for (const auto &item : m_items) {
        if (item.m_year_from && item.m_year_to) {
            auto catIndexes = item.m_additionalCategoryIndexes;
            if (item.m_categoryIndex != -1)
                catIndexes.push_back(item.m_categoryIndex);

            for (qint16 catIndex : catIndexes) {
                auto &cc = catCounter[catIndex];
                cc.first += (item.m_year_from + item.m_year_to);
                cc.second += 2;

                auto &cat = m_categories[catIndex];
                cat.m_year_from = cat.m_year_from ? std::min(cat.m_year_from, item.m_year_from)
                                                  : item.m_year_from;
                cat.m_year_to = std::max(cat.m_year_to, item.m_year_to);
            }
        }
    }
    for (uint catIndex = 0; catIndex < m_categories.size(); ++catIndex) {
        auto cc = catCounter.value(catIndex);
        if (cc.first && cc.second) {
            auto y = quint8(qBound(0ULL, cc.first / cc.second, 255ULL));
            m_categories[catIndex].m_year_recency = y;
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
        for (uint itemIndex = 0; itemIndex < m_items.size(); ++itemIndex) {
            Item &item = m_items[itemIndex];

            bool isPart = (item.itemTypeId() == 'P');

            if ((pass == 1 ? !isPart : isPart) && item.hasInventory() && item.yearReleased()) {
                const auto itemParts = m_consists_of_hash.value(itemIndex);

                for (const BrickLink::Item::ConsistsOf &part : itemParts) {
                    Item &partItem = m_items[part.m_itemIndex];
                    if (partItem.m_itemTypeId == 'P') {
                        partItem.m_year_from = partItem.m_year_from ? std::min(partItem.m_year_from, item.m_year_from)
                                                                    : item.m_year_from;
                        partItem.m_year_to   = std::max(partItem.m_year_to, item.m_year_to);
                    }
                }
            }
        }
    }
}

void BrickLink::TextImport::addToKnownColors(int itemIndex, int colorIndex)
{
    Item &item = m_items[itemIndex];
    auto it = std::lower_bound(item.m_knownColorIndexes.begin(), item.m_knownColorIndexes.end(),
                               colorIndex);
    if ((it == item.m_knownColorIndexes.end()) || (*it != colorIndex))
        item.m_knownColorIndexes.insert(it, colorIndex);
}

