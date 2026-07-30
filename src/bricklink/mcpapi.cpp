// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "mcpapi.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/itemtype.h"
#include "bricklink/category.h"
#include "bricklink/color.h"
#include "bricklink/picture.h"
#include "bricklink/priceguide.h"
#include "bricklink/relationship.h"

#include <QCoro/QCoroSignal>
#include <QCoro/QCoroTask>

#include <QDeadlineTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <array>
#include <chrono>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace BrickLink {

static constexpr qsizetype MaxResults = 200;

// Maximum time an MCP tool waits for a BrickLink network fetch before giving up.
static constexpr auto FetchTimeout = 20s;

// Resolves an item type from its full name or single-letter BrickLink ID.
static const ItemType *resolveItemType(const QString &arg)
{
    const Core *bl = BrickLink::core();
    const ItemType *itemType = nullptr;
    if (arg.size() == 1)
        itemType = bl->itemType(arg.at(0).toUpper().toLatin1());
    if (!itemType) {
        for (const ItemType &it : bl->itemTypes()) {
            if (it.name().compare(arg, Qt::CaseInsensitive) == 0) {
                itemType = &it;
                break;
            }
        }
    }
    return itemType;
}

// Resolves a color by exact name first, then case-insensitive partial match.
static const Color *resolveColorByName(const QString &name)
{
    const Core *bl = BrickLink::core();
    if (const Color *c = bl->colorFromName(name))
        return c;
    for (const Color &c : bl->colors()) {
        if (c.name().contains(name, Qt::CaseInsensitive))
            return &c;
    }
    return nullptr;
}


CatalogQueryMcpTool::CatalogQueryMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString CatalogQueryMcpTool::name() const
{
    return u"catalog_query"_s;
}

QString CatalogQueryMcpTool::description() const
{
    return u"Search the BrickLink item catalog. All filter parameters are optional; "
           u"combine them to narrow down the result list. Returns up to "_s
           % QString::number(MaxResults)
           % u" matching items together with the total match count."_s;
}

QJsonObject CatalogQueryMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"item_id"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Filter by item ID (case-insensitive partial match, e.g. \"3001\")"_s }
            }},
            { u"item_name"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Filter by item name (case-insensitive partial match, e.g. \"Brick 2 x 4\")"_s }
            }},
            { u"item_type"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Filter by item type: use the full name (e.g. \"Part\", \"Set\", \"Minifig\") "
                                    u"or the single-letter BrickLink ID (P, S, M, B, G, I, O, C)"_s }
            }},
            { u"category"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Filter by category name (case-insensitive partial match)"_s }
            }},
            { u"color"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Filter by color name (case-insensitive partial match). "
                                    u"Only items available in the specified color are returned."_s }
            }},
            { u"related_to_item_type"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Item type of the reference item for relationship filtering "
                                    u"(required together with related_to_item_id)"_s }
            }},
            { u"related_to_item_id"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Item ID of the reference item. Returns only items that share "
                                    u"a relationship with this item."_s }
            }},
            { u"relationship"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Relationship type name to filter on (case-insensitive partial match). "
                                    u"Only considered when related_to_item_id is also set."_s }
            }},
            { u"year_min"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"Minimum year of production (inclusive). "
                                    u"Matches items that were still being produced in or after this year."_s }
            }},
            { u"year_max"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"Maximum year of production (inclusive). "
                                    u"Matches items first released in or before this year."_s }
            }}
        }}
    };
}

McpTool::Result CatalogQueryMcpTool::execute(const QJsonObject &arguments)
{
    const Core *bl = BrickLink::core();
    if (!bl || !bl->database()->isValid())
        return Result::error(u"BrickLink database is not loaded"_s);

    // ---- Resolve item-type filter ----------------------------------------

    const QString itemTypeArg = arguments[u"item_type"_s].toString().trimmed();
    const ItemType *filterType = nullptr;
    if (!itemTypeArg.isEmpty()) {
        if (itemTypeArg.size() == 1) {
            filterType = bl->itemType(itemTypeArg.at(0).toLatin1());
        }
        if (!filterType) {
            for (const ItemType &it : bl->itemTypes()) {
                if (it.name().compare(itemTypeArg, Qt::CaseInsensitive) == 0) {
                    filterType = &it;
                    break;
                }
            }
        }
        if (!filterType)
            return Result::error(u"Unknown item type \"%1\": catalog_schema lists all item types"_s.arg(itemTypeArg));
    }

    // ---- Resolve category filter -----------------------------------------

    const QString categoryArg = arguments[u"category"_s].toString().trimmed();
    const Category *filterCategory = nullptr;
    if (!categoryArg.isEmpty()) {
        for (const Category &cat : bl->categories()) {
            if (cat.name().contains(categoryArg, Qt::CaseInsensitive)) {
                filterCategory = &cat;
                break;
            }
        }
        if (!filterCategory)
            return Result::error(u"Unknown category \"%1\": catalog_schema lists all category names"_s.arg(categoryArg));
    }

    // ---- Resolve color filter --------------------------------------------

    const QString colorArg = arguments[u"color"_s].toString().trimmed();
    const Color *filterColor = nullptr;
    if (!colorArg.isEmpty()) {
        filterColor = bl->colorFromName(colorArg);
        if (!filterColor) {
            for (const Color &c : bl->colors()) {
                if (c.name().contains(colorArg, Qt::CaseInsensitive)) {
                    filterColor = &c;
                    break;
                }
            }
        }
        if (!filterColor)
            return Result::error(u"Unknown color \"%1\": catalog_schema lists all color names"_s.arg(colorArg));
    }

    // ---- Parse year range ------------------------------------------------

    const int yearMin = arguments[u"year_min"_s].toInt(0);
    const int yearMax = arguments[u"year_max"_s].toInt(0);

    // ---- Parse item-id / item-name filters -------------------------------

    // Pre-lowercased for case-insensitive QByteArray partial match
    const QByteArray itemIdFilter = arguments[u"item_id"_s].toString().trimmed().toLatin1().toLower();
    const QString itemNameFilter = arguments[u"item_name"_s].toString().trimmed();

    // ---- Resolve relationship filter ------------------------------------

    // relatedItems: the set of items that must be in the result (related to the reference item).
    // Only populated when related_to_item_id is provided.
    QSet<const Item *> relatedItems;
    const QString relatedToItemIdArg   = arguments[u"related_to_item_id"_s].toString().trimmed();
    const QString relatedToItemTypeArg = arguments[u"related_to_item_type"_s].toString().trimmed();
    const QString relationshipArg      = arguments[u"relationship"_s].toString().trimmed();

    if (!relatedToItemIdArg.isEmpty()) {
        // Resolve item type for the reference item
        const ItemType *relatedToType = nullptr;
        if (relatedToItemTypeArg.size() == 1) {
            relatedToType = bl->itemType(relatedToItemTypeArg.at(0).toLatin1());
        }
        if (!relatedToType) {
            for (const ItemType &it : bl->itemTypes()) {
                if (it.name().compare(relatedToItemTypeArg, Qt::CaseInsensitive) == 0) {
                    relatedToType = &it;
                    break;
                }
            }
        }
        if (!relatedToType)
            return Result::error(u"related_to_item_type is missing or unknown (\"%1\"): supply the "
                                 u"item type of related_to_item_id, as listed by catalog_schema"_s
                                 .arg(relatedToItemTypeArg));

        const Item *refItem = bl->item(relatedToType->id(),
                                       relatedToItemIdArg.toLatin1());
        if (!refItem)
            return Result::error(u"related_to_item_id \"%1\" not found: it must be an exact item ID; "
                                 u"use catalog_query to find it"_s.arg(relatedToItemIdArg));

        // Optionally resolve the relationship type
        const Relationship *filterRelationship = nullptr;
        if (!relationshipArg.isEmpty()) {
            for (const Relationship &rel : bl->relationships()) {
                if (rel.name().contains(relationshipArg, Qt::CaseInsensitive)) {
                    filterRelationship = &rel;
                    break;
                }
            }
            if (!filterRelationship)
                return Result::error(u"Unknown relationship type \"%1\": catalog_schema lists all relationship types"_s.arg(relationshipArg));
        }

        // Collect all items that share a qualifying RelationshipMatch with refItem
        for (const RelationshipMatch *match : refItem->relationshipMatches()) {
            if (filterRelationship && match->relationshipId() != filterRelationship->id())
                continue;
            for (const Item *peer : match->items()) {
                if (peer != refItem)
                    relatedItems.insert(peer);
            }
        }
    }

    // ---- Iterate and filter ----------------------------------------------

    QJsonArray results;
    int totalCount = 0;

    for (const Item &item : bl->items()) {
        if (filterType && (item.itemTypeId() != filterType->id()))
            continue;

        if (filterCategory) {
            bool matched = (item.category() == filterCategory);
            if (!matched) {
                for (const Category *cat : item.categories()) {
                    if (cat == filterCategory) {
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched)
                continue;
        }

        if (!itemIdFilter.isEmpty() && !item.id().toLower().contains(itemIdFilter))
            continue;

        if (!itemNameFilter.isEmpty() && !item.name().contains(itemNameFilter, Qt::CaseInsensitive))
            continue;

        if (filterColor && !item.hasKnownColor(filterColor))
            continue;

        const int released = item.yearReleased();
        const int lastProduced = item.yearLastProduced();   // equals yearReleased() when year_to==0
        if (yearMin > 0) {
            if ((released == 0) || (lastProduced < yearMin))
                continue;
        }
        if (yearMax > 0) {
            if ((released == 0) || (released > yearMax))
                continue;
        }

        if (!relatedItems.isEmpty() && !relatedItems.contains(&item))
            continue;

        ++totalCount;

        if (results.size() < MaxResults) {
            QJsonObject obj;
            obj[u"id"_s]   = QString::fromLatin1(item.id());
            obj[u"name"_s] = item.name();
            obj[u"type_id"_s]   = QString(QChar::fromLatin1(item.itemTypeId()));
            obj[u"type_name"_s] = item.itemType() ? item.itemType()->name() : QString();
            obj[u"category"_s]  = item.category() ? item.category()->name() : QString();
            if (released > 0)
                obj[u"year_released"_s] = released;
            if (lastProduced > 0 && lastProduced != released)
                obj[u"year_last_produced"_s] = lastProduced;
            results.append(obj);
        }
    }

    QJsonObject response;
    response[u"total_count"_s]    = totalCount;
    response[u"returned_count"_s] = results.size();
    response[u"items"_s]          = results;

    if (totalCount > MaxResults) {
        response[u"note"_s] = u"Results capped at %1. Refine the filters to see all matches."_s
                              .arg(MaxResults);
    }

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


CatalogSchemaMcpTool::CatalogSchemaMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString CatalogSchemaMcpTool::name() const
{
    return u"catalog_schema"_s;
}

QString CatalogSchemaMcpTool::description() const
{
    return u"Return the valid schema values for BrickLink catalog queries: "
           u"all item types (single-letter ID + full name), all category names, "
           u"all color names, and all relationship type names."_s;
}

QJsonObject CatalogSchemaMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {} }
    };
}

McpTool::Result CatalogSchemaMcpTool::execute(const QJsonObject & /*arguments*/)
{
    const Core *bl = BrickLink::core();
    if (!bl || !bl->database()->isValid())
        return Result::error(u"BrickLink database is not loaded"_s);

    // ---- Item types ------------------------------------------------------

    QJsonArray itemTypes;
    for (const ItemType &it : bl->itemTypes()) {
        itemTypes.append(QJsonObject {
            { u"id"_s,   QString(QChar::fromLatin1(it.id())) },
            { u"name"_s, it.name() }
        });
    }

    // ---- Categories ------------------------------------------------------

    QJsonArray categories;
    for (const Category &cat : bl->categories())
        categories.append(cat.name());

    // ---- Colors ----------------------------------------------------------

    QJsonArray colors;
    for (const Color &c : bl->colors())
        colors.append(c.name());

    // ---- Relationship types ----------------------------------------------

    QJsonArray relationships;
    for (const Relationship &rel : bl->relationships())
        relationships.append(rel.name());

    // ---- Build response --------------------------------------------------

    QJsonObject response;
    response[u"item_types"_s]    = itemTypes;
    response[u"categories"_s]    = categories;
    response[u"colors"_s]        = colors;
    response[u"relationships"_s] = relationships;

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


// Shared input schema for the item+color addressed tools below.
static QJsonObject itemColorSchema(const QString &colorDescription)
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"item_id"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"The item's BrickLink ID (exact match, e.g. \"3001\"): "
                                    u"use catalog_query to find item IDs"_s }
            }},
            { u"item_type"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Item type as full name or single-letter BrickLink ID "
                                    u"(default: Part)"_s }
            }},
            { u"color"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, colorDescription }
            }}
        }},
        { u"required"_s, QJsonArray { u"item_id"_s } }
    };
}

// Resolves the item + (optional) color arguments shared by the price-guide and
// picture tools. On success, sets *item / *color and returns an empty string;
// otherwise returns a ready-to-use error message.
static QString resolveItemAndColor(const QJsonObject &arguments,
                                   const Item **item, const Color **color)
{
    const Core *bl = BrickLink::core();
    if (!bl || !bl->database()->isValid())
        return u"BrickLink database is not loaded"_s;

    const QString itemTypeArg = arguments[u"item_type"_s].toString(u"P"_s).trimmed();
    const ItemType *itemType = resolveItemType(itemTypeArg);
    if (!itemType)
        return u"Unknown item type \"%1\": catalog_schema lists all item types"_s.arg(itemTypeArg);

    const QString itemIdArg = arguments[u"item_id"_s].toString().trimmed();
    *item = bl->item(itemType->id(), itemIdArg.toLatin1());
    if (!*item)
        return u"No item with ID \"%1\" and type %2: use catalog_query to search the catalog"_s
               .arg(itemIdArg, itemType->name());

    *color = nullptr;
    const QString colorArg = arguments[u"color"_s].toString().trimmed();

    if (!itemType->hasColors()) {
        // This item type has no colors (e.g. Set, Minifig): ignore any color arg
        // and use the fixed "(Not Applicable)" color.
        *color = bl->color(0);
    } else if (!colorArg.isEmpty()) {
        *color = resolveColorByName(colorArg);
        if (!*color)
            return u"Unknown color \"%1\": catalog_schema lists all color names"_s.arg(colorArg);
    } else {
        return u"This item type requires a color: supply the \"color\" argument "
               u"(catalog_schema lists all color names)"_s;
    }
    return { };
}


// Builds the JSON price matrix for a valid price guide.
static QJsonObject priceGuideJson(const PriceGuide *pg, const Item *item, const Color *color)
{
    static const std::array priceNames { u"min"_s, u"avg"_s, u"qty_avg"_s, u"max"_s };

    auto conditionBlock = [pg](Time t, Condition c) {
        QJsonObject prices;
        for (int p = 0; p < int(Price::Count); ++p)
            prices[priceNames[p]] = pg->price(t, c, Price(p));
        return QJsonObject {
            { u"total_quantity"_s, pg->quantity(t, c) },
            { u"lots"_s,           pg->lots(t, c) },
            { u"prices"_s,         prices }
        };
    };
    auto timeBlock = [&](Time t) {
        return QJsonObject {
            { u"new"_s,  conditionBlock(t, Condition::New) },
            { u"used"_s, conditionBlock(t, Condition::Used) }
        };
    };

    return QJsonObject {
        { u"item_id"_s,      QString::fromLatin1(item->id()) },
        { u"item_name"_s,    item->name() },
        { u"color"_s,        color ? color->name() : QString() },
        { u"currency"_s,     u"USD"_s },
        { u"last_updated"_s, pg->lastUpdated().toUTC().toString(Qt::ISODate) },
        { u"last_six_months"_s, timeBlock(Time::PastSix) },
        { u"current"_s,         timeBlock(Time::Current) }
    };
}


CatalogPriceGuideMcpTool::CatalogPriceGuideMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString CatalogPriceGuideMcpTool::name() const
{
    return u"catalog_price_guide"_s;
}

QString CatalogPriceGuideMcpTool::description() const
{
    return u"Return the BrickLink price guide for one or more items: sold and "
           u"currently-available quantities and min/avg/qty-weighted-avg/max prices, both "
           u"for the last 6 months and the current listings, for new and used condition. "
           u"Prices are in USD. Pass all the items you need in a single call: uncached "
           u"guides are fetched from BrickLink in one batched request, which is far more "
           u"efficient than calling this tool repeatedly. Each result entry carries either "
           u"the price data or an \"error\" field, so one bad item does not fail the rest."_s;
}

QJsonObject CatalogPriceGuideMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"items"_s, QJsonObject {
                { u"type"_s, u"array"_s },
                { u"description"_s, u"The items to look up the price guide for"_s },
                { u"items"_s, itemColorSchema(u"Color name (case-insensitive), as listed by "
                                              u"catalog_schema. Required for item types that "
                                              u"have colors (e.g. Part), ignored for those "
                                              u"that do not (e.g. Set, Minifig)."_s) }
            }}
        }},
        { u"required"_s, QJsonArray { u"items"_s } }
    };
}

McpTool::Result CatalogPriceGuideMcpTool::execute(const QJsonObject &arguments)
{
    const QJsonArray itemsArg = arguments[u"items"_s].toArray();
    if (itemsArg.isEmpty())
        return Result::error(u"No items supplied"_s);

    auto *cache = BrickLink::core()->priceGuideCache();

    // Per request: either a resolved PriceGuide, or an error string.
    struct Entry {
        PriceGuideRef pg;
        QString error;
    };
    std::vector<Entry> entries;
    entries.reserve(size_t(itemsArg.size()));

    // Phase 1: resolve all items and kick off every needed fetch up front, so the
    // cache can coalesce them into as few batched network requests as possible.
    for (const QJsonValue &value : itemsArg) {
        Entry entry;
        const Item *item = nullptr;
        const Color *color = nullptr;
        if (QString error = resolveItemAndColor(value.toObject(), &item, &color); !error.isEmpty()) {
            entry.error = error;
        } else if (PriceGuideRef pg = cache->priceGuide(item, color, true /*highPriority*/)) {
            if (!pg->isValid() && (pg->updateStatus() != UpdateStatus::UpdateFailed))
                cache->updatePriceGuide(pg, true);
            entry.pg = std::move(pg);
        } else {
            entry.error = u"Could not create a price guide request"_s;
        }
        entries.push_back(std::move(entry));
    }

    // Phase 2: wait until every requested guide is settled (valid or failed), or
    // the overall timeout elapses.
    auto stillPending = [&entries]() {
        for (const Entry &e : entries) {
            if (e.pg && !e.pg->isValid() && (e.pg->updateStatus() != UpdateStatus::UpdateFailed))
                return true;
        }
        return false;
    };
    if (stillPending()) {
        QCoro::waitFor([&]() -> QCoro::Task<> {
            const QDeadlineTimer deadline(FetchTimeout);
            while (stillPending() && !deadline.hasExpired()) {
                const auto remaining = std::chrono::milliseconds(deadline.remainingTime());
                if (!co_await qCoro(cache, &PriceGuideCache::priceGuideUpdated, remaining))
                    break; // timed out
            }
        }());
    }

    // Phase 3: assemble per-item results.
    QJsonArray results;
    for (const Entry &e : entries) {
        if (!e.error.isEmpty()) {
            results.append(QJsonObject { { u"error"_s, e.error } });
        } else if (e.pg->isValid()) {
            results.append(priceGuideJson(e.pg.get(), e.pg->item(), e.pg->color()));
        } else {
            results.append(QJsonObject {
                { u"item_id"_s, QString::fromLatin1(e.pg->item()->id()) },
                { u"error"_s, u"Price guide not available yet: still fetching, try again shortly"_s }
            });
        }
    }

    QJsonObject response { { u"results"_s, results } };
    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


CatalogPictureMcpTool::CatalogPictureMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString CatalogPictureMcpTool::name() const
{
    return u"catalog_picture"_s;
}

QString CatalogPictureMcpTool::description() const
{
    return u"Return the catalog picture of an item in a given color as a PNG image. "
           u"If the image is not cached, it is fetched from BrickLink, which may take "
           u"a few seconds."_s;
}

QJsonObject CatalogPictureMcpTool::inputSchema() const
{
    return itemColorSchema(u"Color name (case-insensitive), as listed by catalog_schema. "
                           u"Required for item types that have colors (e.g. Part), ignored "
                           u"for those that do not (e.g. Set, Minifig)."_s);
}

McpTool::Result CatalogPictureMcpTool::execute(const QJsonObject &arguments)
{
    const Item *item = nullptr;
    const Color *color = nullptr;
    if (QString error = resolveItemAndColor(arguments, &item, &color); !error.isEmpty())
        return Result::error(error);

    PictureRef pic = BrickLink::core()->pictureCache()->picture(item, color, true /*highPriority*/);
    if (!pic)
        return Result::error(u"Could not create a picture request"_s);

    if (!pic->isValid() && (pic->updateStatus() != UpdateStatus::UpdateFailed)) {
        BrickLink::core()->pictureCache()->updatePicture(pic, true);
        if (pic->updateStatus() != UpdateStatus::Ok) {
            QCoro::waitFor([]() -> QCoro::Task<> {
                co_await qCoro(BrickLink::core()->pictureCache(),
                               &PictureCache::pictureUpdated, FetchTimeout);
            }());
        }
    }

    const QImage image = pic->image();
    if (!pic->isValid() || image.isNull()) {
        return Result::error(u"The picture for \"%1\" is not available yet: "
                             u"BrickStore is still fetching it, try again shortly"_s
                             .arg(QString::fromLatin1(item->id())));
    }

    const QString caption = color ? u"%1 (%2), color: %3"_s.arg(item->name(),
                                                                 QString::fromLatin1(item->id()),
                                                                 color->name())
                                  : u"%1 (%2)"_s.arg(item->name(), QString::fromLatin1(item->id()));
    return Result::image(image, caption);
}

} // namespace BrickLink
