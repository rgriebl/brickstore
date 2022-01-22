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
        return std::distance(m_items.cbegin(), it);
    return -1;
}

int BrickLink::TextImport::findColorIndex(uint id) const
{
    auto it = std::lower_bound(m_colors.cbegin(), m_colors.cend(), id, Color::lessThan);
    if ((it != m_colors.cend()) && (it->id() == id))
        return std::distance(m_colors.cbegin(), it);
    return -1;
}

int BrickLink::TextImport::findCategoryIndex(uint id) const
{
    auto it = std::lower_bound(m_categories.cbegin(), m_categories.cend(), id, Category::lessThan);
    if ((it != m_categories.cend()) && (it->id() == id))
        return std::distance(m_categories.cbegin(), it);
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
        readLDrawColors(path % u"ldconfig.ldr");
        calculateColorPopularity();

        // categories
        readCategories(path % u"categories.xml");

        // itemtypes
        readItemTypes(path % u"itemtypes.xml");

        // speed up loading (exactly 137522 items on 16.06.2020)
        m_items.reserve(200000);

        for (ItemType &itt : m_item_types)
            readItems(path % u"items_" % QLatin1Char(itt.m_id) % u".xml", &itt);

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
        col.m_name     = p.elementText(e, "COLORNAME");
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

        col.m_popularity = partCnt + setCnt + wantedCnt + forSaleCnt;

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
        cat.m_name = p.elementText(e, "CATEGORYNAME");

        m_categories.push_back(cat);
    });

    std::sort(m_categories.begin(), m_categories.end(), [](const auto &cat1, const auto &cat2) {
        return Category::lessThan(cat1, cat2.id());
    });

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
        itt.m_name = p.elementText(e, "ITEMTYPENAME");

        itt.m_picture_id        = (c == 'I') ? 'S' : c;
        itt.m_has_inventories   = false;
        itt.m_has_colors        = (c == 'P' || c == 'G');
        itt.m_has_weight        = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
        itt.m_has_year          = (c == 'B' || c == 'C' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
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
        item.m_name = p.elementText(e, "ITEMNAME");
        item.m_itemTypeIndex = (itt - m_item_types.data());
        item.m_itemTypeId = itt->id();

        uint catId = p.elementText(e, "CATEGORY").toUInt();
        item.m_categoryIndex = findCategoryIndex(catId);
        if (item.m_categoryIndex == -1)
            throw ParseException("item %1 has no category").arg(QLatin1String(item.m_id));

        // calculate the item-type -> category relation
        auto &catv = itt->m_categoryIndexes;
        if (std::find(catv.cbegin(), catv.cend(), item.m_categoryIndex) == catv.cend())
            catv.emplace_back(item.m_categoryIndex);

        if (itt->hasYearReleased()) {
            uint y = p.elementText(e, "ITEMYEAR").toUInt() - 1900;
            item.m_year = ((y > 0) && (y < 255)) ? y : 0; // we only have 8 bits for the year
        } else {
            item.m_year = 0;
        }

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
        const QString colorName = p.elementText(e, "COLOR");
        uint code = p.elementText(e, "CODENAME").toUInt();

        int itemIndex = findItemIndex(itemTypeId, itemId);
        if (itemIndex != -1) {
            bool found = false;
            for (uint colorIndex = 0; colorIndex < m_colors.size(); ++colorIndex) {
                if (m_colors[colorIndex].name() == colorName) {
                    addToKnownColors(itemIndex, colorIndex);

                    if (code) {
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
        if (!code) {
            qWarning() << "Parsing part_color_codes: pcc" << p.elementText(e, "CODENAME") << "is not numeric";
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

        uint itemIndex = item - items().data();;

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

void BrickLink::TextImport::readLDrawColors(const QString &path)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    QTextStream in(&f);
    QString line;
    int matchCount = 0;

    while (!(line = in.readLine()).isNull()) {
        if (!line.isEmpty() && line.at(0) == '0'_l1) {
            const QStringList sl = line.simplified().split(' '_l1);

            if ((sl.count() >= 9)
                    && (sl[1] == "!COLOUR"_l1)
                    && (sl[3] == "CODE"_l1)
                    && (sl[5] == "VALUE"_l1)
                    && (sl[7] == "EDGE"_l1)) {
                QString name = sl[2];
                int id = sl[4].toInt();

                name = name.toLower();
                if (name.startsWith("rubber"_l1))
                    continue;

                for (auto &color : m_colors) {
                    QString blname = color.name()
                            .toLower()
                            .replace(' '_l1, '_'_l1)
                            .replace('-'_l1, '_'_l1)
                            .replace("gray"_l1, "grey"_l1);
                    if (blname == name) {
                        color.m_ldraw_id = id;
                        matchCount++;
                        break;
                    }
                }
            }
        }
    }
}

void BrickLink::TextImport::readInventoryList(const QString &path)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
        throw ParseException(&f, "could not open file");

    QTextStream ts(&f);
    int line = 0;
    while (!ts.atEnd()) {
        ++line;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split('\t'_l1);

        if (strs.count() < 2)
            throw ParseException(&f, "expected at least 2 fields in line %1").arg(line);

        char itemTypeId = XmlHelpers::firstCharInString(strs.at(0));
        const QByteArray itemId = strs.at(1).toLatin1();

        if (!itemTypeId || itemId.isEmpty())
            throw ParseException(&f, "expected a valid item-type and an item-id field in line %1").arg(line);

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
            ItemType &itemType = m_item_types[item.m_itemTypeIndex];
            itemType.m_has_inventories = true;
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
    int line = 0;
    while (!ts.atEnd()) {
        ++line;
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        QStringList strs = line.split('\t'_l1);

        if (strs.count() < 7)
            throw ParseException(&f, "expected at least 7 fields in line %1").arg(line);

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

void BrickLink::TextImport::exportTo(Core *bl)
{
    std::swap(bl->m_colors, m_colors);
    std::swap(bl->m_item_types, m_item_types);
    std::swap(bl->m_categories, m_categories);
    std::swap(bl->m_items, m_items);
    std::swap(bl->m_pccs, m_pccs);
    std::swap(bl->m_itemChangelog, m_itemChangelog);
    std::swap(bl->m_colorChangelog, m_colorChangelog);

    for (auto it = m_consists_of_hash.cbegin(); it != m_consists_of_hash.cend(); ++it) {
        Item &item = bl->m_items[it.key()];
        item.setConsistsOf(it.value());
    }

    for (auto it = m_appears_in_hash.cbegin(); it != m_appears_in_hash.cend(); ++it) {
        Item &item = bl->m_items[it.key()];
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

void BrickLink::TextImport::addToKnownColors(int itemIndex, int colorIndex)
{
    Item &item = m_items[itemIndex];
    auto it = std::lower_bound(item.m_knownColorIndexes.begin(), item.m_knownColorIndexes.end(),
                               colorIndex);
    if ((it == item.m_knownColorIndexes.end()) || (*it != colorIndex))
        item.m_knownColorIndexes.insert(it, colorIndex);
}

