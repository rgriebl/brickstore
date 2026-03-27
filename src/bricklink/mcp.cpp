// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "mcp.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/itemtype.h"
#include "bricklink/category.h"
#include "bricklink/color.h"
#include "bricklink/relationship.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

using namespace Qt::StringLiterals;

namespace BrickLink {

static constexpr qsizetype MaxResults = 200;


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
            return Result::error(u"Unknown item type: \"%1\""_s.arg(itemTypeArg));
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
            return Result::error(u"Unknown category: \"%1\""_s.arg(categoryArg));
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
            return Result::error(u"Unknown color: \"%1\""_s.arg(colorArg));
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
            return Result::error(u"related_to_item_type is missing or unknown: \"%1\""_s.arg(relatedToItemTypeArg));

        const Item *refItem = bl->item(relatedToType->id(),
                                       relatedToItemIdArg.toLatin1());
        if (!refItem)
            return Result::error(u"related_to_item_id not found: \"%1\""_s.arg(relatedToItemIdArg));

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
                return Result::error(u"Unknown relationship type: \"%1\""_s.arg(relationshipArg));
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

} // namespace BrickLink
