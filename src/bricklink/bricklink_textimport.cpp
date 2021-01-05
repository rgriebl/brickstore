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

QString decodeEntities(const QString &src)
{
    QString ret(src);
    QRegExp re("&#([0-9]+);");
    re.setMinimal(true);

    int pos = 0;
    while( (pos = re.indexIn(src, pos)) != -1 )
    {
        ret = ret.replace(re.cap(0), QChar(re.cap(1).toInt(nullptr,10)));
        pos += re.matchedLength();
    }
    return ret;
}

QString decodeEntities(const char *src)
{
    // Regular Expressions would be easier, but too slow here

    QString decoded(src);

    int pos = decoded.indexOf(QLatin1String("&#"));
    if (pos < 0)
        return decoded;

    do {
        int endpos = decoded.indexOf(QLatin1Char(';'), pos + 2);
        if (endpos < 0) {
            pos += 2;
        } else {
            int unicode = decoded.midRef(pos + 2, endpos - pos - 2).toInt();
            if (unicode > 0) {
                decoded.replace(pos, endpos - pos + 1, QChar(unicode));
                pos++;
            } else {
                pos = endpos + 1;
            }
        }
        pos = decoded.indexOf(QLatin1String("&#"), pos);
    } while (pos >= 0);

    return decoded;
}


const BrickLink::Category *BrickLink::TextImport::findCategoryByName(const QStringRef &name) const
{
    if (name.isEmpty())
        return nullptr;
    for (const Category *cat : m_categories) {
        if (cat->name() == name)
            return cat;
    }
    return nullptr;
}

const BrickLink::Item *BrickLink::TextImport::findItem(char tid, const QString &id)
{
    Item key;
    key.m_item_type = m_item_types.value(tid);
    key.m_id = id;

    if (!key.m_item_type || key.m_id.isEmpty())
        return nullptr;

    // Finds the lower bound in at most log(last - first) + 1 comparisons
    auto it = std::lower_bound(m_items.constBegin(), m_items.constEnd(), &key, Item::lessThan);
    if (it != m_items.constEnd() && !Item::lessThan(&key, *it))
        return *it;

    return nullptr;
}

namespace BrickLink {
// explicitly namespaced, since gcc seems to have problems with specializations
// within namespaces:
// template <> BrickLink::Category *BrickLink::TextImport::parse<BrickLink::Category> ( uint count, const char **strs )

template <> Category *TextImport::parse<Category> (uint count, const char **strs)
{
    if (count < 2)
        return nullptr;

    auto *cat = new Category();
    cat->m_id   = uint(strtoul(strs[0], nullptr, 10));
    cat->m_name = decodeEntities(strs[1]);

    return cat;
}

template <> Color *TextImport::parse<Color> (uint count, const char **strs)
{
    if (count < 4)
        return nullptr;

    auto *col = new Color();
    col->m_id       = uint(strtoul(strs[0], nullptr, 10));
    col->m_name     = strs[1];
    col->m_ldraw_id = -1;
    col->m_color    = QColor(QString('#') + strs[2]);
    col->m_type     = Color::Type();

    if (!strcmp(strs[3], "Transparent"))  col->m_type |= Color::Transparent;
    if (!strcmp(strs[3], "Glitter"))  col->m_type |= Color::Glitter;
    if (!strcmp(strs[3], "Speckle"))  col->m_type |= Color::Speckle;
    if (!strcmp(strs[3], "Metallic"))  col->m_type |= Color::Metallic;
    if (!strcmp(strs[3], "Chrome"))  col->m_type |= Color::Chrome;
    if (!strcmp(strs[3], "Pearl"))  col->m_type |= Color::Pearl;
    if (!strcmp(strs[3], "Milky"))  col->m_type |= Color::Milky;
    if (!strcmp(strs[3], "Modulex"))  col->m_type |= Color::Modulex;
    if (!col->m_type)
        col->m_type = Color::Solid;

    if (count >= 8) {
        col->m_popularity = strtol(strs[4], nullptr, 10) // parts
                          + strtol(strs[5], nullptr, 10) // in sets
                          + strtol(strs[6], nullptr, 10) // wanted
                          + strtol(strs[7], nullptr, 10); // for sale
        // needs to be divided by the max. after all colors are parsed!
        // mark it as raw data meanwhile:
        col->m_popularity = -col->m_popularity;
    }
    if (count >= 10) {
        col->m_year_from = quint16(strtoul(strs[8], nullptr, 10));
        col->m_year_to   = quint16(strtoul(strs[9], nullptr, 10));
    }
    return col;
}

template <> ItemType *TextImport::parse<ItemType> (uint count, const char **strs)
{
    if (count < 2)
        return nullptr;

    char c = strs [0][0];
    auto *itt = new ItemType();
    itt->m_id                = c;
    itt->m_picture_id        = (c == 'I') ? 'S' : c;
    itt->m_name              = strs[1];
    itt->m_has_inventories   = false;
    itt->m_has_colors        = (c == 'P' || c == 'G');
    itt->m_has_weight        = (c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I');
    itt->m_has_year          = (c == 'B' || c == 'C' || c == 'G' || c == 'S' || c == 'I');
    itt->m_has_subconditions = (c == 'S');

    return itt;
}

template <> Item *TextImport::parse<Item> (uint count, const char **strs)
{
    if (count < 4)
        return nullptr;

    Item *item = new Item();
    item->m_id        = strs[2];
    item->m_name      = decodeEntities(strs[3]);
    item->m_item_type = m_current_item_type;
    item->m_categories.clear();

    const Category *maincat = m_categories.value(uint(strtoul(strs[0], nullptr, 10)));
    const QString allcats = decodeEntities(strs[1]);
    QVector<QStringRef> auxcats = allcats.splitRef(QLatin1String(" / "));

    int span = 0;
    for (int i = 0; i < auxcats.size(); ++i) {
        const QStringRef &current = auxcats.at(i);
        const Category *cat;
        if (!span) {
            cat = findCategoryByName(current);
        } else {
            int start = auxcats.at(i - span).position();
            int end = current.position() + current.length();
            cat = findCategoryByName(current.string()->midRef(start, end - start));
        }
        if (cat) {
            item->m_categories.append(cat);
            span = 0;
        } else {
            // The field separator ' / ' also appears in category names !!!
            ++span;
        }
    }
    if (span || item->m_categories.isEmpty()) {
        qWarning() << "Not all categories could be resolved for item" << item->m_id
                   << "\n   " << allcats;
    } else if (*item->m_categories.constBegin() != maincat) {
        if (!maincat) {
            qWarning() << "The main category did not resolve for item" << item->m_id
                       << "\n   id=" << strs[0];
        } else {
            qWarning() << "The main category did not match for item" << item->m_id
                       << "\n   id=" << maincat->id() << "name=" << (*item->m_categories.constBegin())->name();
        }
    }

    uint parsedfields = 4;

    if ((parsedfields < count) && (item->m_item_type->hasYearReleased())) {
        uint y = uint(strtoul(strs[parsedfields++], nullptr, 10)) - 1900;
        item->m_year = ((y > 0) && (y < 255)) ? y : 0; // we only have 8 bits for the year
        parsedfields++;
    } else {
        item->m_year = 0;
    }

    if ((parsedfields < count) && (item->m_item_type->hasWeight())) {
        static QLocale c = QLocale::c();
        item->m_weight = c.toFloat(strs[parsedfields++]);
    } else {
        item->m_weight = 0;
    }

    if (parsedfields < count)
        item->m_color = m_colors.value(uint(strtoul(strs[parsedfields++], nullptr, 10)));
    else
        item->m_color = nullptr;

    return item;
}

} // namespace BrickLink

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

BrickLink::TextImport::TextImport()
{
    m_current_item_type = nullptr;
}

BrickLink::TextImport::~TextImport()
{
    for (auto &&items : m_consists_of_hash)
        qDeleteAll(items);
}

bool BrickLink::TextImport::import(const QString &path)
{
    bool ok = true;

    ok = ok && readDB<>(path + "colors.txt",     m_colors, true);
    ok = ok && readLDrawColors(path + "ldconfig.ldr");
    ok = ok && readDB<>(path + "categories.txt", m_categories, true);
    ok = ok && readDB<>(path + "itemtypes.txt",  m_item_types, true);

    calculateColorPopularity();

    // speed up loading (exactly 137522 items on 16.06.2020)
    m_items.reserve(150000);

    for (const ItemType *itt : qAsConst(m_item_types)) {
        m_current_item_type = itt;
        ok = ok && readDB<>(path + "items_" + char(itt->m_id) + ".txt", m_items, true);
    }
    m_current_item_type = nullptr;

    std::sort(m_items.begin(), m_items.end(), Item::lessThan);

    Item **itp = const_cast<Item **>(m_items.constData());

    for (int i = 0; i < m_items.count(); itp++, i++) {
        (*itp)->m_index = uint(i);

        auto *itt = const_cast<ItemType *>((*itp)->m_item_type);

        if (itt) {
            for (const Category *cat : qAsConst((*itp)->m_categories)) {
                if (!itt->m_categories.contains(cat))
                    itt->m_categories.append(cat);
            }
        }
    }

    btinvlist_dummy btinvlist_dummy;
    ok = ok && readDB<>(path + "btinvlist.txt", btinvlist_dummy);
    btchglog_dummy btchglog_dummy;
    ok = ok && readDB<>(path + "btchglog.txt", btchglog_dummy);
    known_colors_dummy known_colors_dummy;
    ok = ok && readDB<>(path + "part_color_codes.txt", known_colors_dummy);

    m_current_item_type = nullptr;

    if (!ok)
        qWarning() << "Error importing databases!";

    return ok;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine(QHash<uint, const T *> &h, uint tokencount, const char **tokens)
{
    T *t = parse<T> (tokencount, tokens);

    if (t)
        h.insert(t->id(), t);
    return t;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine(QHash<char, const T *> &h, uint tokencount, const char **tokens)
{
    T *t = parse<T> (tokencount, tokens);

    if (t)
        h.insert(t->id(), t);
    return t;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine(QVector<const T *> &v, uint tokencount, const char **tokens)
{
    T *t = parse<T> (tokencount, tokens);

    if (t)
        v.append(t);
    return t;
}


bool BrickLink::TextImport::readDB_processLine(btinvlist_dummy & /*dummy*/, uint count, const char **strs)
{
    if (count < 2 || !strs [0][0] || !strs [1][0])
        return false;

    if (const Item *itm = findItem(strs[0][0], decodeEntities(strs[1]))) {
        auto t = time_t(0);   // 1.1.1970 00:00

        if ((count > 2) && strs[2][0]) {
            static QString fmtFull = QStringLiteral("M/d/yyyy h:mm:ss AP");
            static QString fmtShort = QStringLiteral("M/d/yyyy");

            QString dtStr = QString::fromLatin1(strs[2]);

            QDateTime dt = QDateTime::fromString(dtStr, dtStr.length() <= 10 ? fmtShort : fmtFull);
#if !defined(DAN_FIXED_BTINVLIST_TO_USE_UTC)
            static QTimeZone tzEST = QTimeZone("EST5EDT");
            dt.setTimeZone(tzEST);
            dt = dt.toUTC();
#endif
            t = dt.toTime_t();
        }
        const_cast <Item *>(itm)->m_last_inv_update = t;
        const_cast <ItemType *>(itm->m_item_type)->m_has_inventories = true;
    }
    else {
        qWarning() << "WARNING: parsing btinvlist: item" << strs[0][0] << strs[1] << "doesn't exist!";
    }
    return true;
}

bool BrickLink::TextImport::readDB_processLine(btchglog_dummy & /*dummy*/, uint count, const char **strs)
{
    if (count < 7 || !strs[2][0])
        return false;

    ChangeLogEntry::Type t;

    switch (strs[2][0]) {
    case 'I': t = ChangeLogEntry::ItemId; break;
    case 'T': t = ChangeLogEntry::ItemType; break;
    case 'M': t = ChangeLogEntry::ItemMerge; break;
    case 'E': t = ChangeLogEntry::CategoryName; break;
    case 'G': t = ChangeLogEntry::CategoryMerge; break;
    case 'C': t = ChangeLogEntry::ColorName; break;
    case 'A': t = ChangeLogEntry::ColorMerge; break;
    default : qWarning("Parsing btchglog: skipping invalid change code %c", strs[2][0]);
              return true;
    }

    if (t == ChangeLogEntry::CategoryName || t == ChangeLogEntry::CategoryMerge || t == ChangeLogEntry::ColorName)
        return true;

    //QDate::fromString(QLatin1String(strs[1]), QLatin1String("M/d/yyyy"));

    QByteArray ba;
    ba.append(char(t));
    for (int i = 0; i < 4; ++i) {
        ba.append('\t');
        ba.append(strs[3+i]);
    }

    m_changelog.append(ba);
    return true;
}

bool BrickLink::TextImport::readDB_processLine(BrickLink::TextImport::known_colors_dummy &, uint count, const char **strs)
{
    if (count < 3 || !strs[0][0] || !strs[1][0])
        return false;

    if (const Item *itm = findItem('P', decodeEntities(strs[0]))) {
        bool found = false;
        for (const Color *color : qAsConst(m_colors)) {
            if (color->name() == QLatin1String(strs[1])) {
                const_cast<Item *>(itm)->m_known_colors << color->id();
                found = true;
                break;
            }
        }
        if (!found)
            qWarning("Parsing part_color_codes: skipping invalid color %s on item %s", strs[1], strs[0]);
    } else {
        qWarning("Parsing part_color_codes: skipping invalid item %s", strs[0]);
    }
    return true;
}

template <typename C> bool BrickLink::TextImport::readDB(const QString &name, C &container, bool skip_header)
{
    // plain C is way faster than Qt on top of C++
    // and this routine has to be fast to reduce the startup time

    // TODO5: since this is only ever run by a daily cron job on a server, speed is not so
    //        important here as is maintainability

    FILE *f = fopen(name.toLatin1(), "r");
    if (f) {
        char line[1000];
        int lineno = 1;

        if (skip_header) {
            char *gcc_supress_warning = fgets(line, 1000, f);
            Q_UNUSED(gcc_supress_warning)
        }

        while (!feof(f)) {
            if (!fgets(line, 1000, f))
                break;

            lineno++;

            if (!line [0] || line [0] == '#' || line [0] == '\r' || line [0] == '\n')
                continue;

            const char *tokens[20];
            uint tokencount = 0;

            for (const char *pos = line; tokencount < 20;) {
                const char *newpos = pos;

                while (*newpos && (*newpos != '\t') && (*newpos != '\r') && (*newpos != '\n'))
                    newpos++;

                bool stop = (*newpos != '\t');

                tokens[tokencount++] = pos;

                line[newpos - line] = 0;
                pos = newpos + 1;

                if (stop)
                    break;
            }

            if (!readDB_processLine(container, tokencount, tokens)) {
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

bool BrickLink::TextImport::importInventories(QVector<const Item *> &invs)
{
    const Item **itemp = invs.data();
    for (int i = 0; i < invs.count(); i++) {
        const Item *&item = itemp[i];

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
    QString filename = BrickLink::core()->dataPath(item) + "inventory.xml";

    QFileInfo fi(filename);
    if (fi.exists() && (fi.lastModified() < item->inventoryUpdated()))
        return false;

    bool ok = false;

    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        QString emsg;
        int eline = 0, ecol = 0;
        QDomDocument doc;

        if (doc.setContent(&f, &emsg, &eline, &ecol)) {
            QDomElement root = doc.documentElement();

            const Core::ParseItemListXMLResult result = BrickLink::core()->parseItemListXML(doc.documentElement().toElement(), BrickLink::XMLHint_Inventory);

            if (result.items) {
                if (!result.invalidItemCount) {
                    for (const BrickLink::InvItem *ii : *result.items) {
                        if (!ii->item() || !ii->color() || !ii->quantity())
                            continue;

                        BrickLink::AppearsInColor &vec = m_appears_in_hash[ii->item()][ii->color()];
                        vec.append(QPair<int, const BrickLink::Item *>(ii->quantity(), item));
                    }
                    // the hash owns the items now
                    m_consists_of_hash.insert(item, *result.items);
                    ok = true;
                }
                delete result.items;
            }
        }
    }
    return ok;
}

bool BrickLink::TextImport::readLDrawColors(const QString &path)
{
    QFile f(path);
    if (f.open(QFile::ReadOnly)) {
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

                    bool found = false;

                    for (auto it = m_colors.begin(); !found && (it != m_colors.end()); ++it) {
                        Color *color = const_cast<Color *>(it.value());
                        QString blname = color->name()
                                .toLower()
                                .replace(' ', '_')
                                .replace('-', '_')
                                .replace("gray", "grey");
                        if (blname == name) {
                            color->m_ldraw_id = id;
                            matchCount++;
                            found = true;
                        }
                    }
                }
            }
        }
        return true;
    }
    return  false;
}

void BrickLink::TextImport::exportTo(Core *bl)
{
    bl->setDatabase_Basics(m_colors, m_categories, m_item_types, m_items);
    bl->setDatabase_ChangeLog(m_changelog);
}

void BrickLink::TextImport::exportInventoriesTo(Core *bl)
{
    bl->setDatabase_ConsistsOf(m_consists_of_hash);
    bl->setDatabase_AppearsIn(m_appears_in_hash);
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
