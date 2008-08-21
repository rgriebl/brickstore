/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include "utility.h"
#include "bricklink.h"


static char *my_strdup(const char *str)
{
    return str ? strcpy(new char [strlen(str) + 1], str) : 0;
}

static bool my_strncmp(const char *s1, const char *s2, int len)
{
    while (*s1 && (len-- > 0)) {
        if (*s1++ != *s2++)
            return false;
    }
    return (len == 0) && (*s1 == 0);
}

void set_tz(const char *tz)
{
    char pebuf [256] = "TZ=";
    if (tz)
        strcat(pebuf, tz);

#if defined( Q_OS_WIN32 )
    _putenv(pebuf);
    _tzset();
#else
    putenv(pebuf);
    tzset();
#endif
}

time_t toUTC(const QDateTime &dt, const char *settz)
{
    QByteArray oldtz;

    if (settz) {
        oldtz = getenv("TZ");
        set_tz(settz);
    }

    // get a tm structure from the system to get the correct tz_name
    time_t t = time(0);
    struct tm *lt = localtime(&t);

    lt->tm_sec   = dt.time().second();
    lt->tm_min   = dt.time().minute();
    lt->tm_hour  = dt.time().hour();
    lt->tm_mday  = dt.date().day();
    lt->tm_mon   = dt.date().month() - 1;       // 0-11 instead of 1-12
    lt->tm_year  = dt.date().year() - 1900;     // year - 1900
    lt->tm_wday  = -1;
    lt->tm_yday  = -1;
    lt->tm_isdst = -1; // tm_isdst negative ->mktime will find out about DST

    // keep tm_zone and tm_gmtoff
    t = mktime(lt);

    if (settz)
        set_tz(oldtz.data());

    //time_t t2 = dt.toTime_t();

    return t;
}


const BrickLink::Category *BrickLink::TextImport::findCategoryByName(const char *name, int len)
{
    if (!name || !name [0])
        return 0;
    if (len < 0)
        len = strlen(name);

    foreach(const Category *cat, m_categories) {
        if (my_strncmp(cat->name(), name, len))
            return cat;
    }
    return 0;
}

const BrickLink::Item *BrickLink::TextImport::findItem(char tid, const char *id)
{
    Item key;
    key.m_item_type = m_item_types.value(tid);
    key.m_id = const_cast<char *>(id);

    Item *keyp = &key;

    Item **itp = (Item **) bsearch(&keyp, m_items.data(), m_items.count(), sizeof(Item *), (int (*)(const void *, const void *)) Item::compare);

    key.m_id = 0;
    key.m_item_type = 0;

    return itp ? *itp : 0;
}

void BrickLink::TextImport::appendCategoryToItemType(const Category *cat, ItemType *itt)
{
    bool found = false;
    quint32 catcount = 0;
    for (const Category **catp = itt->m_categories; catp && *catp; catp++) {
        if (*catp == cat) {
            found = true;
            break;
        }
        catcount++;
    }

    if (found)
        return;

    const Category **catarr = new const Category * [catcount + 2];
    memcpy(catarr, itt->m_categories, catcount * sizeof(const Category *));
    catarr [catcount] = cat;
    catarr [catcount + 1] = 0;

    delete [] itt->m_categories;
    itt->m_categories = catarr;
}

namespace BrickLink {
// explicitly namespaced, since gcc seems to have problems with specializations
// within namespaces:
// template <> BrickLink::Category *BrickLink::TextImport::parse<BrickLink::Category> ( uint count, const char **strs )

template <> Category *TextImport::parse<Category> (uint count, const char **strs)
{
    if (count < 2)
        return 0;

    Category *cat = new Category();
    cat->m_id   = strtol(strs[0], 0, 10);
    cat->m_name = my_strdup(strs[1]);
    return cat;
}

template <> Color *TextImport::parse<Color> (uint count, const char **strs)
{
    if (count < 2)
        return 0;

    Color *col = new Color();
    col->m_id          = strtol(strs[0], 0, 10);
    col->m_name        = my_strdup(strs[1]);
    col->m_ldraw_id    = -1;
    col->m_color       = QColor(QString('#') + strs[2]);
    col->m_type        = Color::Solid;

    if (!strcmp(strs[3], "Transparent"))  col->m_type |= Color::Transparent;
    if (!strcmp(strs[3], "Glitter"))  col->m_type |= Color::Glitter;
    if (!strcmp(strs[3], "Speckle"))  col->m_type |= Color::Speckle;
    if (!strcmp(strs[3], "Metallic"))  col->m_type |= Color::Metallic;
    if (!strcmp(strs[3], "Chrome"))  col->m_type |= Color::Chrome;
    if (!strcmp(strs[3], "Pearl"))  col->m_type |= Color::Pearl;
    if (!strcmp(strs[3], "Milky"))  col->m_type |= Color::Milky;
    return col;
}

template <> ItemType *TextImport::parse<ItemType> (uint count, const char **strs)
{
    if (count < 2)
        return 0;

    char c = strs [0][0];
    ItemType *itt = new ItemType();
    itt->m_id              = c;
    itt->m_picture_id      = (c == 'I') ? 'S' : c;
    itt->m_name            = my_strdup(strs[1]);
    itt->m_has_inventories = false;
    itt->m_has_colors      = (c == 'P' || c == 'G');
    itt->m_has_weight      = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I');
    itt->m_has_year        = (c == 'B' || c == 'C' || c == 'G' || c == 'S' || c == 'I');

    return itt;
}

template <> Item *TextImport::parse<Item> (uint count, const char **strs)
{
    if (count < 4)
        return 0;

    Item *item = new Item();
    item->m_id        = my_strdup(strs[2]);
    item->m_name      = my_strdup(strs[3]);
    item->m_item_type = m_current_item_type;

    // only allocate one block for name, id and category to speed up things

    const char *pos = strs[1];
    const Category *maincat = m_categories.value(strtol(strs[0], 0, 10));

    if (maincat && pos [0]) {
        pos += strlen(maincat->name());

        if (*pos)
            pos += 3;
    }
    const char *auxcat = pos;

    uint catcount = 1;
    const Category *catlist[256] = { maincat };

    while ((pos = strstr(pos, " / "))) {
        if (auxcat[0]) {
            const Category *cat = findCategoryByName(auxcat, pos - auxcat);
            if (cat)
                catlist[catcount++] = cat;
            else {
                // The field separator ' / ' also appears in category names !!!
                pos += 3;
                continue;
            }
        }
        pos += 3;
        auxcat = pos;
    }
    if (auxcat[0]) {
        const Category *cat = findCategoryByName(auxcat);
        if (cat)
            catlist[catcount++] = cat;
        else
            qWarning() << "Invalid category name:" << auxcat;
    }

    item->m_categories = new const Category * [catcount + 1];
    memcpy(item->m_categories, catlist, catcount * sizeof(const Category *));
    item->m_categories[catcount] = 0;

    uint parsedfields = 4;

    if ((parsedfields < count) && (item->m_item_type->hasYearReleased())) {
        int y = strtol(strs[parsedfields], 0, 10) - 1900;
        item->m_year = ((y > 0) && (y < 255)) ? y : 0; // we only have 8 bits for the year
        parsedfields++;
    }
    else
        item->m_year = 0;

    if ((parsedfields < count) && (item->m_item_type->hasWeight())) {
        item->m_weight = QLocale::c().toFloat(strs[parsedfields]);
        parsedfields++;
    }
    else
        item->m_weight = 0;

    if (parsedfields < count)
        item->m_color = m_colors.value(strtol(strs[parsedfields], 0, 10));
    else
        item->m_color = 0;

    return item;
}

}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

BrickLink::TextImport::TextImport()
{
    m_current_item_type = 0;
}

bool BrickLink::TextImport::import(const QString &path)
{
    bool ok = true;

    ok &= readDB<>(path + "colors.txt",     m_colors);
    ok &= readDB<>(path + "categories.txt", m_categories);
    ok &= readDB<>(path + "itemtypes.txt",  m_item_types);

    ok &= readPeeronColors(path + "peeron_colors.html");

    // speed up loading (exactly 47976 items on 05.12.2007)
    m_items.reserve(50000);

    foreach(const ItemType *itt, m_item_types) {
        m_current_item_type = itt;
        ok &= readDB<>(path + "items_" + char(itt->m_id) + ".txt", m_items);
    }
    m_current_item_type = 0;

    qsort(m_items.data(), m_items.count(), sizeof(Item *), (int (*)(const void *, const void *)) Item::compare);

    Item **itp = const_cast<Item **>(m_items.data());

    for (int i = 0; i < m_items.count(); itp++, i++) {
        (*itp)->m_index = i;

        ItemType *itt = const_cast<ItemType *>((*itp)->m_item_type);

        if (itt) {
            for (const Category **catp = (*itp)->m_categories; *catp; catp++)
                appendCategoryToItemType(*catp, itt);
        }
    }

    btinvlist_dummy btinvlist_dummy;
    ok &= readDB <> (path + "btinvlist.txt", btinvlist_dummy);

    btpriceguide_dummy btpriceguide_dummy;
    foreach(const ItemType *itt, m_item_types) {
        m_current_item_type = itt;
        ok &= readDB<>(path + "alltimepg_" + char(itt->m_id) + ".txt", btpriceguide_dummy);
    }
    m_current_item_type = 0;

    if (!ok)
        qWarning() << "Error importing databases!";

    return ok;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine(QHash<int, const T *> &h, uint tokencount, const char **tokens)
{
    T *t = parse<T> (tokencount, (const char **) tokens);

    if (t) {
        h.insert(t->id(), t);
        return true;
    }
    else
        return false;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine(QVector<const T *> &v, uint tokencount, const char **tokens)
{
    T *t = parse<T> (tokencount, (const char **) tokens);

    if (t) {
        v.append(t);
        return true;
    }
    else
        return false;
}


bool BrickLink::TextImport::readDB_processLine(btinvlist_dummy & /*dummy*/, uint count, const char **strs)
{
    if (count < 2 || !strs [0][0] || !strs [1][0])
        return false;

    const Item *itm = findItem(strs [0][0], strs [1]);

    if (itm) {
        time_t t = time_t (0);   // 1.1.1970 00:00

        if (strs [2][0]) {
#if DAN_FIXED_BTINVLIST_TO_USE_UTC
            t = QDateTime::fromString(QLatin1String(strs[2][0]), QLatin1String("%mm/%dd/%yyyy %hh:%mm:%ss %ap")).toTime_t();
#else         
            char ampm;
            int d, m, y, hh, mm, ss;

            if (sscanf(strs [2], "%d/%d/%d %d:%d:%d %cM", &m, &d, &y, &hh, &mm, &ss, &ampm) == 7) {
                QDateTime dt;
                dt.setDate(QDate(y, m, d));
                if (ampm == 'P')
                    hh += (hh == 12) ? -12 : 12;
                dt.setTime(QTime(hh, mm, ss));

                // !!! These dates are in EST (-0500), not UTC !!!
                t = toUTC(dt, "EST5EDT");
            }
        }
#endif
        const_cast <Item *>(itm)->m_last_inv_update = t;
        const_cast <ItemType *>(itm->m_item_type)->m_has_inventories = true;
    }
    else
        qWarning() << "WARNING: parsing btinvlist: item " << strs [1] << " [" << strs [0][0] << "] doesn't exist!";

    return true;
}


bool BrickLink::TextImport::readDB_processLine(btpriceguide_dummy & /*dummy*/, uint count, const char **strs)
{
    if (count < 10 || !strs[0][0] || !strs[1][0])
        return false;

    const Item *itm = findItem(m_current_item_type->id(), strs[0]);
    const Color *col = m_colors.value(strtol(strs[1], 0, 10));

    if (itm && col) {
        AllTimePriceGuide pg;
        pg.item = itm;
        pg.color = col;
        pg.condition[New].minPrice = money_t::fromCString(strs[2]);
        pg.condition[New].avgPrice = money_t::fromCString(strs[3]);
        pg.condition[New].maxPrice = money_t::fromCString(strs[4]);
        pg.condition[New].quantity = strtol(strs[5], 0, 10);
        pg.condition[Used].minPrice = money_t::fromCString(strs[6]);
        pg.condition[Used].avgPrice = money_t::fromCString(strs[7]);
        pg.condition[Used].maxPrice = money_t::fromCString(strs[8]);
        pg.condition[Used].quantity = strtol(strs[9], 0, 10);

        if (pg.condition[New].quantity || pg.condition[Used].quantity)
            m_alltime_pg_list << pg;
    }
    else
        qWarning() << "WARNING: parsing btpriceguide: item " << strs[0] << " [" << m_current_item_type->id() << "] in color " << strs[1] << " doesn't exist!";

    return true;
}


template <typename C> bool BrickLink::TextImport::readDB(const QString &name, C &container)
{
    // plain C is way faster than Qt on top of C++
    // and this routine has to be fast to reduce the startup time

    FILE *f = fopen(name.toLatin1(), "r");
    if (f) {
        char line [1000];
        int lineno = 1;

        (void) fgets(line, 1000, f);    // skip header

        while (!feof(f)) {
            if (!fgets(line, 1000, f))
                break;

            lineno++;

            if (!line [0] || line [0] == '#' || line [0] == '\r' || line [0] == '\n')
                continue;

            char *tokens [20];
            uint tokencount = 0;

            for (char *pos = line; tokencount < 20;) {
                char *newpos = pos;

                while (*newpos && (*newpos != '\t') && (*newpos != '\r') && (*newpos != '\n'))
                    newpos++;

                bool stop = (*newpos != '\t');

                tokens [tokencount++] = pos;

                *newpos = 0;
                pos = newpos + 1;

                if (stop)
                    break;
            }

            if (!readDB_processLine(container, tokencount, (const char **) tokens)) {
                qWarning().nospace() << "ERROR: parse error in file \"" << name << "\", line " << lineno << ": \"" << line << "\"";
                return false;
            }
        }

        fclose(f);
        return true;
    }
    qWarning().nospace() <<  "ERROR: could not open file \"" << name << "\"";
    return false;
}


bool BrickLink::TextImport::readPeeronColors(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream in(&f);

        QString line;
        int count = 0;

        QRegExp namepattern("<a href=[^>]+>(.+)</a>");

        while (!(line = in.readLine()).isNull()) {
            if (line.startsWith("<td>") && line.endsWith("</td>")) {
                QString tmp = line.mid(4, line.length() - 9);
                QStringList sl = tmp.split("</td><td>", QString::KeepEmptyParts);

                bool line_ok = false;

                if (sl.count() >= 5) {
                    int ldraw_id = -1, id = -1;
                    QString peeron_name;

                    if (!sl [3].isEmpty())
                        id = sl [3].toInt();
                    if (!sl [4].isEmpty())
                        ldraw_id = sl [4].toInt();

                    if (namepattern.exactMatch(sl [0]))
                        peeron_name = namepattern.cap(1);

                    if (id != -1) {
                        Color *colp = const_cast<Color *>(m_colors.value(id));
                        if (colp) {
                            if (!peeron_name.isEmpty())
                                colp->m_peeron_name = my_strdup(peeron_name.toLatin1());
                            colp->m_ldraw_id = ldraw_id;

                            count++;
                        }
                    }
                    line_ok = true;
                }

                if (!line_ok)
                    qWarning() << "Failed to parse item line: " << line;
            }
        }

        return (count > 0);
    }
    return false;
}

bool BrickLink::TextImport::importInventories(const QString &path, QVector<const Item *> &invs)
{
    const Item **itemp = invs.data();
    for (int i = 0; i < invs.count(); i++) {
        const Item *&item = itemp[i];

        if (!item) // already yanked
            continue;

        if (!item->hasInventory()) {
            item = 0;  // no inv at all ->yank it
            continue;
        }

        if (readInventory(path, item)) {
            item = 0;
        }
    }
    return true;
}

bool BrickLink::TextImport::readInventory(const QString &path, const Item *item)
{
    QString filename = QString("%1/%2/%3/inventory.xml").arg(path).arg(QChar(item->itemType()->id())).arg(item->id());

    QFileInfo fi(filename);
    if (fi.exists() && (fi.lastModified() < item->inventoryUpdated()))
        return false;

    bool ok = false;

    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        uint invalid_items = 0;
        InvItemList *items = 0;

        QString emsg;
        int eline = 0, ecol = 0;
        QDomDocument doc;

        if (doc.setContent(&f, &emsg, &eline, &ecol)) {
            QDomElement root = doc.documentElement();

            items = BrickLink::core()->parseItemListXML(doc.documentElement().toElement(), BrickLink::XMLHint_Inventory , &invalid_items);

            if (items) {
                if (!invalid_items) {
                    foreach(const BrickLink::InvItem *ii, *items) {
                        if (!ii->item() || !ii->color() || !ii->quantity())
                            continue;

                        BrickLink::AppearsInColor &vec = m_appears_in_hash[ii->item()][ii->color()];
                        vec.append(QPair<int, const BrickLink::Item *>(ii->quantity(), item));
                    }
                    m_consists_of_hash.insert(item, *items);
                    ok = true;
                }
                delete items;
            }
        }
    }
    return ok;
}

void BrickLink::TextImport::exportTo(Core *bl)
{
    bl->setDatabase_Basics(m_colors, m_categories, m_item_types, m_items);
    bl->setDatabase_AllTimePG(m_alltime_pg_list);
}

void BrickLink::TextImport::exportInventoriesTo(Core *bl)
{
    bl->setDatabase_ConsistsOf(m_consists_of_hash);
    bl->setDatabase_AppearsIn(m_appears_in_hash);
}
