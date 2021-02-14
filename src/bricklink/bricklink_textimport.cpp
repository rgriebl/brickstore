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
#include <cstdlib>
#include <ctime>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtDebug>
#include <QTimeZone>

#include "bricklink.h"
#include "exception.h"
#include "xmlhelpers.h"


const BrickLink::Item *BrickLink::TextImport::findItem(char tid, const QString &id) const
{
    auto needle = std::make_pair(tid, id);
    auto it = std::lower_bound(m_items.cbegin(), m_items.cend(), needle, Item::lowerBound);
    if ((it != m_items.cend()) && ((*it)->itemType()->id() == tid) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const BrickLink::Color *BrickLink::TextImport::findColor(uint id) const
{
    auto it = std::lower_bound(m_colors.cbegin(), m_colors.cend(), id, Color::lowerBound);
    if ((it != m_colors.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const BrickLink::Category *BrickLink::TextImport::findCategory(uint id) const
{
    auto it = std::lower_bound(m_categories.cbegin(), m_categories.cend(), id, Category::lowerBound);
    if ((it != m_categories.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


BrickLink::TextImport::TextImport()
{ }

BrickLink::TextImport::~TextImport()
{
    for (auto &&items : m_consists_of_hash)
        qDeleteAll(items);
}

bool BrickLink::TextImport::import(const QString &path)
{
    try {
        // colors
        readColors(path + "colors.xml");
        readLDrawColors(path + "ldconfig.ldr");
        calculateColorPopularity();

        // categories
        readCategories(path + "categories.xml");

        // itemtypes
        readItemTypes(path + "itemtypes.xml");

        // speed up loading (exactly 137522 items on 16.06.2020)
        m_items.reserve(200000);

        for (const ItemType *itt : qAsConst(m_item_types))
            readItems(path + "items_" + char(itt->m_id) + ".xml", itt);


        // we need to set index for faster reverse lookups
        auto itp = const_cast<Item **>(m_items.data());

        for (uint i = 0; i < m_items.size(); itp++, i++) {
            auto item = *itp;
            item->m_index = i;

            if (auto *itt = const_cast<ItemType *>(item->m_item_type)) {
                const Category *cat = item->m_category;
                if (!itt->m_categories.contains(cat))
                    itt->m_categories.append(cat);
            }
        }

        readPartColorCodes(path + "part_color_codes.xml");
        readInventoryList(path + "btinvlist.csv");
        readChangeLog(path + "btchglog.csv");

        return true;
    } catch (const ParseException &e) {
        qWarning() << "Error importing database:" << e.what();
        return false;
    }
}


void BrickLink::TextImport::readColors(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p](QDomElement e) {
        QScopedPointer<Color> col(new Color);
        uint colid = p.elementText(e, "COLOR").toUInt();

        col->m_id       = colid;
        col->m_name     = p.elementText(e, "COLORNAME");
        col->m_color    = QColor(QString('#') + p.elementText(e, "COLORRGB"));

        col->m_ldraw_id = -1;
        col->m_type     = Color::Type();

        auto type = p.elementText(e, "COLORTYPE");
        if (type.contains("Transparent")) col->m_type |= Color::Transparent;
        if (type.contains("Glitter"))     col->m_type |= Color::Glitter;
        if (type.contains("Speckle"))     col->m_type |= Color::Speckle;
        if (type.contains("Metallic"))    col->m_type |= Color::Metallic;
        if (type.contains("Chrome"))      col->m_type |= Color::Chrome;
        if (type.contains("Pearl"))       col->m_type |= Color::Pearl;
        if (type.contains("Milky"))       col->m_type |= Color::Milky;
        if (type.contains("Modulex"))     col->m_type |= Color::Modulex;
        if (type.contains("Satin"))       col->m_type |= Color::Satin;
        if (!col->m_type)
            col->m_type = Color::Solid;

        int partCnt    = p.elementText(e, "COLORCNTPARTS").toInt();
        int setCnt     = p.elementText(e, "COLORCNTSETS").toInt();
        int wantedCnt  = p.elementText(e, "COLORCNTWANTED").toInt();
        int forSaleCnt = p.elementText(e, "COLORCNTINV").toInt();

        col->m_popularity = partCnt + setCnt + wantedCnt + forSaleCnt;

        // needs to be divided by the max. after all colors are parsed!
        // mark it as raw data meanwhile:
        col->m_popularity = -col->m_popularity;

        col->m_year_from = p.elementText(e, "COLORYEARFROM").toUShort();
        col->m_year_to   = p.elementText(e, "COLORYEARTO").toUShort();

        m_colors.emplace_back(col.take());
    });

    std::sort(m_colors.begin(), m_colors.end(), [](const auto &col1, const auto &col2) {
        return Color::lowerBound(col1, col2->id());
    });
}

void BrickLink::TextImport::readCategories(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p](QDomElement e) {
        QScopedPointer<Category> cat(new Category);
        uint catid = p.elementText(e, "CATEGORY").toUInt();

        cat->m_id   = catid;
        cat->m_name = p.elementText(e, "CATEGORYNAME");

        m_categories.emplace_back(cat.take());
    });

    std::sort(m_categories.begin(), m_categories.end(), [](const auto &cat1, const auto &cat2) {
        return Category::lowerBound(cat1, cat2->id());
    });

}

void BrickLink::TextImport::readItemTypes(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p](QDomElement e) {
        QScopedPointer<ItemType> itt(new ItemType);
        char c = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));

        if (c == 'U')
            return;

        itt->m_id   = c;
        itt->m_name = p.elementText(e, "ITEMTYPENAME");

        itt->m_picture_id        = (c == 'I') ? 'S' : c;
        itt->m_has_inventories   = false;
        itt->m_has_colors        = (c == 'P' || c == 'G');
        itt->m_has_weight        = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
        itt->m_has_year          = (c == 'B' || c == 'C' || c == 'G' || c == 'S' || c == 'I' || c == 'M');
        itt->m_has_subconditions = (c == 'S');

        m_item_types.emplace_back(itt.take());
    });

    std::sort(m_item_types.begin(), m_item_types.end(), [](const auto &itt1, const auto &itt2) {
        return ItemType::lowerBound(itt1, itt2->id());
    });
}

void BrickLink::TextImport::readItems(const QString &path, const BrickLink::ItemType *itt)
{
    XmlHelpers::ParseXML p(path, "CATALOG", "ITEM");
    p.parse([this, &p, itt](QDomElement e) {
        QScopedPointer<Item> item(new Item);
        item->m_id        = p.elementText(e, "ITEMID");
        item->m_name      = p.elementText(e, "ITEMNAME");
        item->m_item_type = itt;

        uint catId = p.elementText(e, "CATEGORY").toUInt();
        item->m_category = findCategory(catId);
        if (!item->m_category)
            throw ParseException("item %1 has no category").arg(item->m_id);

        if (itt->hasYearReleased()) {
            uint y = p.elementText(e, "ITEMYEAR").toUInt() - 1900;
            item->m_year = ((y > 0) && (y < 255)) ? y : 0; // we only have 8 bits for the year
        } else {
            item->m_year = 0;
        }

        if (itt->hasWeight()) {
            static QLocale c = QLocale::c();
            item->m_weight = c.toFloat(p.elementText(e, "ITEMWEIGHT"));
        } else {
            item->m_weight = 0;
        }

        try {
            item->m_color = findColor(p.elementText(e, "IMAGECOLOR").toUInt());
        } catch (...) {
            item->m_color = nullptr;
        }

        m_items.emplace_back(item.take());
    });

    std::sort(m_items.begin(), m_items.end(), [](const auto &item1, const auto &item2) {
        return Item::lowerBound(item1, std::make_pair(item2->itemType()->id(), item2->id()));
    });
}

void BrickLink::TextImport::readPartColorCodes(const QString &path)
{
    XmlHelpers::ParseXML p(path, "CODES", "ITEM");
    p.parse([this, &p](QDomElement e) {
        char itemTypeId = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));
        const QString itemId = p.elementText(e, "ITEMID");
        const QString colorName = p.elementText(e, "COLOR");
        uint code = p.elementText(e, "CODENAME").toUInt();

        if (const Item *item = findItem(itemTypeId, itemId)) {
            bool found = false;
            for (const Color *color : m_colors) {
                if (color->name() == colorName) {
                    if (!item->m_known_colors.contains(color->id()))
                        const_cast<Item *>(item)->m_known_colors << color->id();
                    if (code) {
                        PartColorCode *pcc = new PartColorCode;
                        pcc->m_id = code;
                        pcc->m_item = item;
                        pcc->m_color = color;
                        m_pccs.emplace_back(pcc);
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
        return PartColorCode::lowerBound(item1, item2->id());
    });
}


bool BrickLink::TextImport::importInventories(std::vector<const Item *> &invs)
{
    for (auto &&item : invs) {
        if (!item) // already yanked
            continue;

        if (!item->hasInventory()) {
            item = nullptr;  // no inv at all ->yank it
            continue;
        }

        if (readInventory(item)) {
            item = nullptr;
        }
    }
    return true;
}

bool BrickLink::TextImport::readInventory(const Item *item)
{
    QScopedPointer<QFile> f(BrickLink::core()->dataFile(u"inventory.xml", QIODevice::ReadOnly, item));

    if (!f || !f->isOpen() || (f->fileTime(QFileDevice::FileModificationTime) < item->inventoryUpdated()))
        return false;

    InvItemList invItems;

    try {
        XmlHelpers::ParseXML p(f.take(), "INVENTORY", "ITEM");
        p.parse([this, &p, &invItems](QDomElement e) {
            char itemTypeId = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));
            const QString itemId = p.elementText(e, "ITEMID");
            uint colorId = p.elementText(e, "COLOR").toUInt();
            int qty = p.elementText(e, "QTY").toInt();
            bool extra = (p.elementText(e, "EXTRA") == QLatin1String("Y"));
            bool counterPart = (p.elementText(e, "COUNTERPART") == QLatin1String("Y"));
            bool alternate = (p.elementText(e, "ALTERNATE") == QLatin1String("Y"));
            uint matchId = p.elementText(e, "MATCHID").toUInt();

            const Item *item = findItem(itemTypeId, itemId);
            const Color *color = findColor(colorId);

            if (!item || !color || !qty)
                throw Exception("Unknown item- or color-id or 0 qty");

            auto *ii = new InvItem(color, item);
            ii->setQuantity(qty);
            ii->setStatus(extra ? Status::Extra : Status::Include);
            ii->setCounterPart(counterPart);
            ii->setAlternate(alternate);
            ii->setAlternateId(matchId);

            invItems << ii;
        });

        for (const InvItem *ii : qAsConst(invItems)) {
            AppearsInColor &vec = m_appears_in_hash[ii->item()][ii->color()];
            vec.append(qMakePair(ii->quantity(), item));
        }
        // the hash owns the items now
        m_consists_of_hash.insert(item, invItems);
        return true;

    } catch (const Exception &) {
        qDeleteAll(invItems);
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
        if (!line.isEmpty() && line.at(0) == '0') {
            const QStringList sl = line.simplified().split(' ');

            if ((sl.count() >= 9)
                    && (sl[1] == "!COLOUR")
                    && (sl[3] == "CODE")
                    && (sl[5] == "VALUE")
                    && (sl[7] == "EDGE")) {
                QString name = sl[2];
                int id = sl[4].toInt();

                name = name.toLower();
                if (name.startsWith("rubber"))
                    continue;

                for (auto &&color : m_colors) {
                    QString blname = color->name()
                            .toLower()
                            .replace(' ', '_')
                            .replace('-', '_')
                            .replace("gray", "grey");
                    if (blname == name) {
                        const_cast<Color *>(color)->m_ldraw_id = id;
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
        QStringList strs = line.split('\t');

        if (strs.count() < 2)
            throw ParseException(&f, "expected at least 2 fields in line %1").arg(line);

        char itemTypeId = XmlHelpers::firstCharInString(strs.at(0));
        const QString itemId = strs.at(1);

        if (!itemTypeId || itemId.isEmpty())
            throw ParseException(&f, "expected a valid item-type and an item-id field in line %1").arg(line);

        if (const Item *itm = findItem(itemTypeId, itemId)) {
            auto t = time_t(0);   // 1.1.1970 00:00

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
            const_cast <Item *>(itm)->m_last_inv_update = t;
            const_cast <ItemType *>(itm->m_item_type)->m_has_inventories = true;
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
        QStringList strs = line.split('\t');

        if (strs.count() < 7)
            throw ParseException(&f, "expected at least 7 fields in line %1").arg(line);

        ChangeLogEntry::Type t;
        char c = XmlHelpers::firstCharInString(strs.at(2));

        switch (c) {
        case 'I': t = ChangeLogEntry::ItemId; break;
        case 'T': t = ChangeLogEntry::ItemType; break;
        case 'M': t = ChangeLogEntry::ItemMerge; break;
        case 'E': t = ChangeLogEntry::CategoryName; break;
        case 'G': t = ChangeLogEntry::CategoryMerge; break;
        case 'C': t = ChangeLogEntry::ColorName; break;
        case 'A': t = ChangeLogEntry::ColorMerge; break;
        default:
            qWarning("Parsing btchglog: skipping invalid change code %x", uint(c));
            continue;
        }

        if (t == ChangeLogEntry::CategoryName || t == ChangeLogEntry::CategoryMerge || t == ChangeLogEntry::ColorName)
            continue;

        QByteArray ba;
        ba.append(char(t));
        for (int i = 0; i < 4; ++i) {
            ba.append('\t');
            ba.append(strs.at(3+i).toLatin1());
        }

        m_changelog.emplace_back(ba);
    }
}

void BrickLink::TextImport::exportTo(Core *bl)
{
    /// QMutexLocker lock(&m_corelock);

    bl->m_colors     = m_colors;
    bl->m_item_types = m_item_types;
    bl->m_categories = m_categories;
    bl->m_items      = m_items;
    bl->m_pccs       = m_pccs;

    bl->m_changelog = m_changelog;
}

void BrickLink::TextImport::exportInventoriesTo(Core * /*bl*/)
{
    //QMutexLocker lock(&m_corelock);

    for (auto it = m_consists_of_hash.cbegin(); it != m_consists_of_hash.cend(); ++it)
        it.key()->setConsistsOf(it.value());

    for (auto it = m_appears_in_hash.cbegin(); it != m_appears_in_hash.cend(); ++it)
        it.key()->setAppearsIn(it.value());
}


void BrickLink::TextImport::calculateColorPopularity()
{
    qreal maxpop = 0;
    for (const auto &col : qAsConst(m_colors))
        maxpop = qMin(maxpop, col->m_popularity);
    for (const auto &col : qAsConst(m_colors)) {
        qreal &pop = const_cast<Color *>(col)->m_popularity;

        if (!qFuzzyIsNull(maxpop))
            pop /= maxpop;
        else
            pop = 0;
    }
}

