// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "mcpapi.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/document.h"
#include "common/documentio.h"
#include "common/documentlist.h"
#include "common/documentmodel.h"
#include "bricklink/core.h"
#include "bricklink/color.h"
#include "bricklink/io.h"
#include "bricklink/item.h"
#include "bricklink/itemtype.h"
#include "bricklink/lot.h"
#include "utility/exception.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

using namespace Qt::StringLiterals;

static constexpr int MaxLotsPerRead = 500;


static QString statusToString(BrickLink::Status status)
{
    switch (status) {
    case BrickLink::Status::Include: return u"include"_s;
    case BrickLink::Status::Exclude: return u"exclude"_s;
    case BrickLink::Status::Extra  : return u"extra"_s;
    default                        : return { };
    }
}

static QString conditionToString(BrickLink::Condition condition)
{
    return (condition == BrickLink::Condition::New) ? u"new"_s : u"used"_s;
}

static QString subConditionToString(BrickLink::SubCondition subCondition)
{
    switch (subCondition) {
    case BrickLink::SubCondition::Complete  : return u"complete"_s;
    case BrickLink::SubCondition::Incomplete: return u"incomplete"_s;
    case BrickLink::SubCondition::Sealed    : return u"sealed"_s;
    default                                 : return { };
    }
}

static QString stockroomToString(BrickLink::Stockroom stockroom)
{
    switch (stockroom) {
    case BrickLink::Stockroom::A: return u"A"_s;
    case BrickLink::Stockroom::B: return u"B"_s;
    case BrickLink::Stockroom::C: return u"C"_s;
    default                     : return { };
    }
}

static std::optional<BrickLink::Status> statusFromString(const QString &s)
{
    if (s.compare(u"include", Qt::CaseInsensitive) == 0)
        return BrickLink::Status::Include;
    if (s.compare(u"exclude", Qt::CaseInsensitive) == 0)
        return BrickLink::Status::Exclude;
    if (s.compare(u"extra", Qt::CaseInsensitive) == 0)
        return BrickLink::Status::Extra;
    return { };
}

static std::optional<BrickLink::Condition> conditionFromString(const QString &s)
{
    if (s.compare(u"new", Qt::CaseInsensitive) == 0)
        return BrickLink::Condition::New;
    if (s.compare(u"used", Qt::CaseInsensitive) == 0)
        return BrickLink::Condition::Used;
    return { };
}

static std::optional<BrickLink::SubCondition> subConditionFromString(const QString &s)
{
    if (s.compare(u"none", Qt::CaseInsensitive) == 0)
        return BrickLink::SubCondition::None;
    if (s.compare(u"complete", Qt::CaseInsensitive) == 0)
        return BrickLink::SubCondition::Complete;
    if (s.compare(u"incomplete", Qt::CaseInsensitive) == 0)
        return BrickLink::SubCondition::Incomplete;
    if (s.compare(u"sealed", Qt::CaseInsensitive) == 0)
        return BrickLink::SubCondition::Sealed;
    return { };
}

static std::optional<BrickLink::Stockroom> stockroomFromString(const QString &s)
{
    if (s.compare(u"none", Qt::CaseInsensitive) == 0)
        return BrickLink::Stockroom::None;
    if (s.compare(u"a", Qt::CaseInsensitive) == 0)
        return BrickLink::Stockroom::A;
    if (s.compare(u"b", Qt::CaseInsensitive) == 0)
        return BrickLink::Stockroom::B;
    if (s.compare(u"c", Qt::CaseInsensitive) == 0)
        return BrickLink::Stockroom::C;
    return { };
}

static Document *resolveDocument(const QJsonObject &arguments, QString *error)
{
    const auto &documents = DocumentList::inst()->documents();
    const int index = arguments[u"index"_s].toInt(-1);
    if ((index < 0) || (index >= documents.size())) {
        *error = u"Invalid document index %1: use document_list to enumerate the open documents"_s
                 .arg(index);
        return nullptr;
    }
    return documents.at(index);
}

// Mutating tools must not touch state that a suspended UI coroutine still relies on.
static QString mutationBlocked(const Document *doc = nullptr)
{
    if (Application::inst()->isWaitingForUserInput())
        return u"BrickStore is waiting for user input in a modal dialog: retry once the user has closed it"_s;
    if (doc && doc->isBlockingOperationActive())
        return u"The document is busy with a blocking operation: retry later"_s;
    return { };
}

static QJsonObject filePathSchema(const QString &description)
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"file_path"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, description }
            }}
        }},
        { u"required"_s, QJsonArray { u"file_path"_s } }
    };
}

// Exception messages from the Document/DocumentIO layer are meant for message
// boxes and may contain HTML markup, which is just noise in an MCP result.
static QString plainText(QString s)
{
    static const QRegularExpression tagRe(u"<[^>]*>"_s);
    return s.replace(tagRe, u" "_s).simplified();
}

static QString validItemTypes()
{
    QStringList names;
    for (const BrickLink::ItemType &it : BrickLink::core()->itemTypes())
        names << it.name();
    return names.join(u", "_s);
}

static const BrickLink::Color *resolveColor(const QString &name)
{
    const auto *bl = BrickLink::core();
    if (const auto *color = bl->colorFromName(name))
        return color;
    for (const BrickLink::Color &c : bl->colors()) {
        if (c.name().contains(name, Qt::CaseInsensitive))
            return &c;
    }
    return nullptr;
}

// Applies the writable lot fields present in obj to lot. Returns an error string,
// or an empty string if all supplied fields were valid.
static QString applyLotFields(BrickLink::Lot *lot, const QJsonObject &obj,
                              const DocumentModel *model)
{
    if (obj.contains(u"color"_s)) {
        const QString name = obj[u"color"_s].toString();
        const auto *color = resolveColor(name);
        if (!color)
            return u"Unknown color \"%1\": catalog_schema lists all valid color names"_s.arg(name);
        lot->setColor(color);
    }
    if (obj.contains(u"status"_s)) {
        const auto status = statusFromString(obj[u"status"_s].toString());
        if (!status)
            return u"Invalid status: use include, exclude or extra"_s;
        lot->setStatus(*status);
    }
    if (obj.contains(u"condition"_s)) {
        const auto condition = conditionFromString(obj[u"condition"_s].toString());
        if (!condition)
            return u"Invalid condition: use new or used"_s;
        lot->setCondition(*condition);
    }
    if (obj.contains(u"sub_condition"_s)) {
        const auto subCondition = subConditionFromString(obj[u"sub_condition"_s].toString());
        if (!subCondition)
            return u"Invalid sub_condition: use none, complete, incomplete or sealed"_s;
        lot->setSubCondition(*subCondition);
    }
    if (obj.contains(u"stockroom"_s)) {
        const auto stockroom = stockroomFromString(obj[u"stockroom"_s].toString());
        if (!stockroom)
            return u"Invalid stockroom: use none, A, B or C"_s;
        lot->setStockroom(*stockroom);
    }
    if (obj.contains(u"quantity"_s)) {
        const int quantity = obj[u"quantity"_s].toInt();
        if (std::abs(quantity) > DocumentModel::maxQuantity)
            return u"Invalid quantity: %1"_s.arg(quantity);
        lot->setQuantity(quantity);
    }
    if (obj.contains(u"price"_s)) {
        const double price = obj[u"price"_s].toDouble(-1);
        if ((price < 0) || (price > DocumentModel::maxLocalPrice(model->currencyCode())))
            return u"Invalid price: %1"_s.arg(obj[u"price"_s].toDouble());
        lot->setPrice(price);
    }
    if (obj.contains(u"cost"_s)) {
        const double cost = obj[u"cost"_s].toDouble(-1);
        if ((cost < 0) || (cost > DocumentModel::maxLocalPrice(model->currencyCode())))
            return u"Invalid cost: %1"_s.arg(obj[u"cost"_s].toDouble());
        lot->setCost(cost);
    }
    if (obj.contains(u"bulk_quantity"_s))
        lot->setBulkQuantity(obj[u"bulk_quantity"_s].toInt(1));
    if (obj.contains(u"sale"_s))
        lot->setSale(obj[u"sale"_s].toInt());
    if (obj.contains(u"comments"_s))
        lot->setComments(obj[u"comments"_s].toString());
    if (obj.contains(u"remarks"_s))
        lot->setRemarks(obj[u"remarks"_s].toString());
    if (obj.contains(u"reserved"_s))
        lot->setReserved(obj[u"reserved"_s].toString());
    if (obj.contains(u"retain"_s))
        lot->setRetain(obj[u"retain"_s].toBool());
    if (obj.contains(u"marker_text"_s))
        lot->setMarkerText(obj[u"marker_text"_s].toString());
    return { };
}

// JSON schema for the fields accepted by applyLotFields.
static QJsonObject lotFieldProperties()
{
    return QJsonObject {
        { u"color"_s, QJsonObject {
            { u"type"_s, u"string"_s },
            { u"description"_s, u"Color name (case-insensitive), as listed by catalog_schema"_s }
        }},
        { u"status"_s, QJsonObject {
            { u"type"_s, u"string"_s },
            { u"enum"_s, QJsonArray { u"include"_s, u"exclude"_s, u"extra"_s } }
        }},
        { u"condition"_s, QJsonObject {
            { u"type"_s, u"string"_s },
            { u"enum"_s, QJsonArray { u"new"_s, u"used"_s } }
        }},
        { u"sub_condition"_s, QJsonObject {
            { u"type"_s, u"string"_s },
            { u"enum"_s, QJsonArray { u"none"_s, u"complete"_s, u"incomplete"_s, u"sealed"_s } }
        }},
        { u"quantity"_s, QJsonObject {
            { u"type"_s, u"integer"_s }
        }},
        { u"price"_s, QJsonObject {
            { u"type"_s, u"number"_s },
            { u"description"_s, u"Unit price in the document's currency"_s }
        }},
        { u"cost"_s, QJsonObject {
            { u"type"_s, u"number"_s },
            { u"description"_s, u"Unit cost in the document's currency"_s }
        }},
        { u"bulk_quantity"_s, QJsonObject {
            { u"type"_s, u"integer"_s }
        }},
        { u"sale"_s, QJsonObject {
            { u"type"_s, u"integer"_s },
            { u"description"_s, u"Sale in percent (-99 to 100)"_s }
        }},
        { u"comments"_s, QJsonObject {
            { u"type"_s, u"string"_s }
        }},
        { u"remarks"_s, QJsonObject {
            { u"type"_s, u"string"_s }
        }},
        { u"reserved"_s, QJsonObject {
            { u"type"_s, u"string"_s },
            { u"description"_s, u"BrickLink username this lot is reserved for"_s }
        }},
        { u"retain"_s, QJsonObject {
            { u"type"_s, u"boolean"_s }
        }},
        { u"stockroom"_s, QJsonObject {
            { u"type"_s, u"string"_s },
            { u"enum"_s, QJsonArray { u"none"_s, u"A"_s, u"B"_s, u"C"_s } }
        }},
        { u"marker_text"_s, QJsonObject {
            { u"type"_s, u"string"_s }
        }}
    };
}

static QJsonObject documentToJson(const Document *doc, int index)
{
    const DocumentModel *model = doc->model();

    QJsonObject obj;
    obj[u"index"_s]         = index;
    obj[u"title"_s]         = doc->title().isEmpty() ? doc->fileNameOrTitle() : doc->title();
    if (!doc->filePath().isEmpty())
        obj[u"file_path"_s] = doc->filePath();
    obj[u"lot_count"_s]     = model->lotCount();
    obj[u"currency_code"_s] = model->currencyCode();
    obj[u"modified"_s]      = model->isModified();
    if (ActionManager::inst()->activeDocument() == doc)
        obj[u"active"_s]    = true;
    return obj;
}

// Sparse on purpose: default/empty values are omitted to keep the responses small.
static QJsonObject lotToJson(const BrickLink::Lot *lot, int row)
{
    QJsonObject obj;
    obj[u"row"_s]       = row;
    obj[u"item_id"_s]   = QString::fromLatin1(lot->itemId());
    obj[u"item_name"_s] = lot->itemName();
    obj[u"item_type"_s] = lot->itemTypeName();
    obj[u"color"_s]     = lot->colorName();
    obj[u"category"_s]  = lot->categoryName();
    obj[u"status"_s]    = statusToString(lot->status());
    obj[u"condition"_s] = conditionToString(lot->condition());
    if (lot->subCondition() != BrickLink::SubCondition::None)
        obj[u"sub_condition"_s] = subConditionToString(lot->subCondition());
    obj[u"quantity"_s]  = lot->quantity();
    obj[u"price"_s]     = lot->price();
    if (!qFuzzyIsNull(lot->cost()))
        obj[u"cost"_s] = lot->cost();
    if (lot->bulkQuantity() > 1)
        obj[u"bulk_quantity"_s] = lot->bulkQuantity();
    if (lot->sale())
        obj[u"sale"_s] = lot->sale();
    if (lot->tierQuantity(0)) {
        QJsonArray tiers;
        for (int i = 0; i < 3; ++i) {
            if (lot->tierQuantity(i)) {
                tiers.append(QJsonObject {
                    { u"quantity"_s, lot->tierQuantity(i) },
                    { u"price"_s,    lot->tierPrice(i)    }
                });
            }
        }
        obj[u"tier_prices"_s] = tiers;
    }
    if (!lot->comments().isEmpty())
        obj[u"comments"_s] = lot->comments();
    if (!lot->remarks().isEmpty())
        obj[u"remarks"_s] = lot->remarks();
    if (!lot->reserved().isEmpty())
        obj[u"reserved"_s] = lot->reserved();
    if (lot->lotId())
        obj[u"lot_id"_s] = int(lot->lotId());
    if (lot->retain())
        obj[u"retain"_s] = true;
    if (lot->stockroom() != BrickLink::Stockroom::None)
        obj[u"stockroom"_s] = stockroomToString(lot->stockroom());
    if (!lot->markerText().isEmpty())
        obj[u"marker_text"_s] = lot->markerText();
    return obj;
}

// The standard response for tools that create or return a single document.
static McpTool::Result documentResult(const Document *doc, const QJsonObject &extra = { })
{
    QJsonObject response = extra;
    response[u"document"_s] = documentToJson(doc, int(DocumentList::inst()->documents().indexOf(doc)));
    return McpTool::Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


DocumentListMcpTool::DocumentListMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentListMcpTool::name() const
{
    return u"document_list"_s;
}

QString DocumentListMcpTool::description() const
{
    return u"List all open BrickStore documents. Each entry has an index that the other "
           u"document tools use to reference the document, plus title, file path, lot count, "
           u"currency code, modification state and whether it is the active document."_s;
}

QJsonObject DocumentListMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject { } }
    };
}

McpTool::Result DocumentListMcpTool::execute(const QJsonObject & /*arguments*/)
{
    QJsonArray docs;
    const auto &documents = DocumentList::inst()->documents();
    for (qsizetype i = 0; i < documents.size(); ++i)
        docs.append(documentToJson(documents.at(i), int(i)));

    QJsonObject response;
    response[u"count"_s]     = int(documents.size());
    response[u"documents"_s] = docs;

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


DocumentReadMcpTool::DocumentReadMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentReadMcpTool::name() const
{
    return u"document_read"_s;
}

QString DocumentReadMcpTool::description() const
{
    return u"Read the lots of an open BrickStore document, given its index from document_list. "
           u"Returns up to %1 lots per call; use offset to page through larger documents. "
           u"Each lot has a row field: this is the lot's stable position in the unsorted "
           u"document and is used to reference the lot in other document tools. Prices are "
           u"in the document's currency. Lot fields still at their default or empty value "
           u"are omitted."_s.arg(MaxLotsPerRead);
}

QJsonObject DocumentReadMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"index"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"The document's index, as returned by document_list"_s }
            }},
            { u"offset"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"Start reading at this row (default: 0)"_s }
            }},
            { u"limit"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"Maximum number of lots to return (default and cap: %1)"_s
                                    .arg(MaxLotsPerRead) }
            }}
        }},
        { u"required"_s, QJsonArray { u"index"_s } }
    };
}

McpTool::Result DocumentReadMcpTool::execute(const QJsonObject &arguments)
{
    QString error;
    const Document *doc = resolveDocument(arguments, &error);
    if (!doc)
        return Result::error(error);

    const int index = arguments[u"index"_s].toInt();
    const auto &lots = doc->model()->lots();

    const int offset = std::max(0, arguments[u"offset"_s].toInt(0));
    int limit = arguments[u"limit"_s].toInt(MaxLotsPerRead);
    if ((limit <= 0) || (limit > MaxLotsPerRead))
        limit = MaxLotsPerRead;

    QJsonArray lotsJson;
    for (qsizetype row = offset; (row < lots.size()) && (lotsJson.size() < limit); ++row)
        lotsJson.append(lotToJson(lots.at(row), int(row)));

    QJsonObject response;
    response[u"document"_s] = documentToJson(doc, index);
    response[u"lots"_s]     = lotsJson;
    if (offset + lotsJson.size() < lots.size()) {
        response[u"note"_s] = u"%1 more lots available: repeat the call with offset %2"_s
                              .arg(lots.size() - offset - lotsJson.size())
                              .arg(offset + lotsJson.size());
    }

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


DocumentAddLotsMcpTool::DocumentAddLotsMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentAddLotsMcpTool::name() const
{
    return u"document_add_lots"_s;
}

QString DocumentAddLotsMcpTool::description() const
{
    return u"Add new lots to an open BrickStore document, given its index from document_list. "
           u"All lots are validated first and added as a single undo step. Returns the added "
           u"lots including their row indexes."_s;
}

QJsonObject DocumentAddLotsMcpTool::inputSchema() const
{
    QJsonObject lotProperties = lotFieldProperties();
    lotProperties[u"item_id"_s] = QJsonObject {
        { u"type"_s, u"string"_s },
        { u"description"_s, u"The item's BrickLink ID (exact match, e.g. \"3001\"): "
                            u"use catalog_query to find item IDs"_s }
    };
    lotProperties[u"item_type"_s] = QJsonObject {
        { u"type"_s, u"string"_s },
        { u"description"_s, u"Item type as full name or single-letter BrickLink ID "
                            u"(default: Part)"_s }
    };

    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"index"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"The document's index, as returned by document_list"_s }
            }},
            { u"lots"_s, QJsonObject {
                { u"type"_s, u"array"_s },
                { u"description"_s, u"The lots to add"_s },
                { u"items"_s, QJsonObject {
                    { u"type"_s, u"object"_s },
                    { u"properties"_s, lotProperties },
                    { u"required"_s, QJsonArray { u"item_id"_s } }
                }}
            }}
        }},
        { u"required"_s, QJsonArray { u"index"_s, u"lots"_s } }
    };
}

McpTool::Result DocumentAddLotsMcpTool::execute(const QJsonObject &arguments)
{
    QString error;
    Document *doc = resolveDocument(arguments, &error);
    if (!doc)
        return Result::error(error);
    if (QString blocked = mutationBlocked(doc); !blocked.isEmpty())
        return Result::error(blocked);

    const QJsonArray lotsArg = arguments[u"lots"_s].toArray();
    if (lotsArg.isEmpty())
        return Result::error(u"No lots supplied"_s);

    const auto *bl = BrickLink::core();
    DocumentModel *model = doc->model();

    // Validate everything before mutating anything.
    std::vector<BrickLink::Lot> protoLots;
    protoLots.reserve(size_t(lotsArg.size()));

    for (const QJsonValue &value : lotsArg) {
        const QJsonObject obj = value.toObject();

        const QString itemTypeArg = obj[u"item_type"_s].toString(u"P"_s).trimmed();
        const BrickLink::ItemType *itemType = nullptr;
        if (itemTypeArg.size() == 1)
            itemType = bl->itemType(itemTypeArg.at(0).toUpper().toLatin1());
        if (!itemType) {
            for (const BrickLink::ItemType &it : bl->itemTypes()) {
                if (it.name().compare(itemTypeArg, Qt::CaseInsensitive) == 0) {
                    itemType = &it;
                    break;
                }
            }
        }
        if (!itemType)
            return Result::error(u"Unknown item type \"%1\": valid types are %2"_s
                             .arg(itemTypeArg, validItemTypes()));

        const QString itemIdArg = obj[u"item_id"_s].toString().trimmed();
        const BrickLink::Item *item = bl->item(itemType->id(), itemIdArg.toLatin1());
        if (!item)
            return Result::error(u"No item with ID \"%1\" and type %2: use catalog_query to search the catalog"_s
                             .arg(itemIdArg, itemType->name()));

        BrickLink::Lot lot;
        lot.setItem(item);
        lot.setColor(bl->color(0)); // "(Not Applicable)", unless a color is supplied

        if (QString fieldError = applyLotFields(&lot, obj, model); !fieldError.isEmpty())
            return Result::error(fieldError);

        protoLots.push_back(lot);
    }

    BrickLink::LotList newLots;
    newLots.reserve(qsizetype(protoLots.size()));
    for (const BrickLink::Lot &proto : protoLots)
        newLots.append(new BrickLink::Lot(proto));

    const int firstRow = model->lotCount();
    model->appendLots(std::move(newLots));

    QJsonArray lotsJson;
    const auto &lots = model->lots();
    for (int row = firstRow; row < model->lotCount(); ++row)
        lotsJson.append(lotToJson(lots.at(row), row));

    QJsonObject response;
    response[u"document"_s]   = documentToJson(doc, arguments[u"index"_s].toInt());
    response[u"added_lots"_s] = lotsJson;

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


DocumentEditLotsMcpTool::DocumentEditLotsMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentEditLotsMcpTool::name() const
{
    return u"document_edit_lots"_s;
}

QString DocumentEditLotsMcpTool::description() const
{
    return u"Edit existing lots of an open BrickStore document, given its index from "
           u"document_list. Lots are referenced by their row index from document_read. "
           u"Only the supplied fields are changed. All edits are validated first and "
           u"applied as a single undo step. Returns the edited lots."_s;
}

QJsonObject DocumentEditLotsMcpTool::inputSchema() const
{
    QJsonObject editProperties = lotFieldProperties();
    editProperties[u"row"_s] = QJsonObject {
        { u"type"_s, u"integer"_s },
        { u"description"_s, u"The lot's row index, as returned by document_read"_s }
    };

    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"index"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"The document's index, as returned by document_list"_s }
            }},
            { u"edits"_s, QJsonObject {
                { u"type"_s, u"array"_s },
                { u"description"_s, u"The edits to apply, each referencing a lot by row"_s },
                { u"items"_s, QJsonObject {
                    { u"type"_s, u"object"_s },
                    { u"properties"_s, editProperties },
                    { u"required"_s, QJsonArray { u"row"_s } }
                }}
            }}
        }},
        { u"required"_s, QJsonArray { u"index"_s, u"edits"_s } }
    };
}

McpTool::Result DocumentEditLotsMcpTool::execute(const QJsonObject &arguments)
{
    QString error;
    Document *doc = resolveDocument(arguments, &error);
    if (!doc)
        return Result::error(error);
    if (QString blocked = mutationBlocked(doc); !blocked.isEmpty())
        return Result::error(blocked);

    const QJsonArray editsArg = arguments[u"edits"_s].toArray();
    if (editsArg.isEmpty())
        return Result::error(u"No edits supplied"_s);

    DocumentModel *model = doc->model();
    const auto &lots = model->lots();

    // Validate everything before mutating anything.
    std::vector<std::pair<BrickLink::Lot *, BrickLink::Lot>> changes;
    changes.reserve(size_t(editsArg.size()));

    for (const QJsonValue &value : editsArg) {
        const QJsonObject obj = value.toObject();

        const int row = obj[u"row"_s].toInt(-1);
        if ((row < 0) || (row >= lots.size()))
            return Result::error(u"Invalid row %1: use document_read to enumerate the lots"_s.arg(row));

        BrickLink::Lot *lot = lots.at(row);
        BrickLink::Lot newValue = *lot;

        if (QString fieldError = applyLotFields(&newValue, obj, model); !fieldError.isEmpty())
            return Result::error(fieldError);

        changes.emplace_back(lot, newValue);
    }

    model->changeLots(changes);

    QJsonArray lotsJson;
    for (const QJsonValue &value : editsArg) {
        const int row = value.toObject()[u"row"_s].toInt();
        lotsJson.append(lotToJson(lots.at(row), row));
    }

    QJsonObject response;
    response[u"document"_s]    = documentToJson(doc, arguments[u"index"_s].toInt());
    response[u"edited_lots"_s] = lotsJson;

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


DocumentRemoveLotsMcpTool::DocumentRemoveLotsMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentRemoveLotsMcpTool::name() const
{
    return u"document_remove_lots"_s;
}

QString DocumentRemoveLotsMcpTool::description() const
{
    return u"Remove lots from an open BrickStore document, given its index from document_list. "
           u"Lots are referenced by their row index from document_read. All rows are validated "
           u"first and removed as a single undo step. The row indexes of the remaining lots "
           u"shift after a removal: call document_read again before further edits."_s;
}

QJsonObject DocumentRemoveLotsMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"index"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"The document's index, as returned by document_list"_s }
            }},
            { u"rows"_s, QJsonObject {
                { u"type"_s, u"array"_s },
                { u"description"_s, u"The row indexes of the lots to remove"_s },
                { u"items"_s, QJsonObject { { u"type"_s, u"integer"_s } } }
            }}
        }},
        { u"required"_s, QJsonArray { u"index"_s, u"rows"_s } }
    };
}

McpTool::Result DocumentRemoveLotsMcpTool::execute(const QJsonObject &arguments)
{
    QString error;
    Document *doc = resolveDocument(arguments, &error);
    if (!doc)
        return Result::error(error);
    if (QString blocked = mutationBlocked(doc); !blocked.isEmpty())
        return Result::error(blocked);

    const QJsonArray rowsArg = arguments[u"rows"_s].toArray();
    if (rowsArg.isEmpty())
        return Result::error(u"No rows supplied"_s);

    DocumentModel *model = doc->model();
    const auto &lots = model->lots();

    // Validate everything before mutating anything.
    BrickLink::LotList removeLots;
    QJsonArray lotsJson;

    for (const QJsonValue &value : rowsArg) {
        const int row = value.toInt(-1);
        if ((row < 0) || (row >= lots.size()))
            return Result::error(u"Invalid row %1: use document_read to enumerate the lots"_s.arg(row));

        BrickLink::Lot *lot = lots.at(row);
        if (removeLots.contains(lot))
            return Result::error(u"Duplicate row %1"_s.arg(row));

        removeLots.append(lot);
        lotsJson.append(lotToJson(lot, row)); // serialize before the lots are gone
    }

    model->removeLots(removeLots);

    QJsonObject response;
    response[u"document"_s]     = documentToJson(doc, arguments[u"index"_s].toInt());
    response[u"removed_lots"_s] = lotsJson;

    return Result::text(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}


DocumentCreateMcpTool::DocumentCreateMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentCreateMcpTool::name() const
{
    return u"document_create"_s;
}

QString DocumentCreateMcpTool::description() const
{
    return u"Create a new, empty BrickStore document. Returns the new document, including "
           u"the index used to reference it in the other document tools."_s;
}

QJsonObject DocumentCreateMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject { } }
    };
}

McpTool::Result DocumentCreateMcpTool::execute(const QJsonObject & /*arguments*/)
{
    if (QString blocked = mutationBlocked(); !blocked.isEmpty())
        return Result::error(blocked);

    return documentResult(Document::create());
}


DocumentOpenMcpTool::DocumentOpenMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentOpenMcpTool::name() const
{
    return u"document_open"_s;
}

QString DocumentOpenMcpTool::description() const
{
    return u"Open a BrickStore (BSX) document from the filesystem. If the file is already "
           u"open, the existing document is returned instead. Use document_import_bl_xml or "
           u"document_import_ldraw for other file formats."_s;
}

QJsonObject DocumentOpenMcpTool::inputSchema() const
{
    return filePathSchema(u"Absolute path of the BSX document to open"_s);
}

McpTool::Result DocumentOpenMcpTool::execute(const QJsonObject &arguments)
{
    if (QString blocked = mutationBlocked(); !blocked.isEmpty())
        return Result::error(blocked);

    const QString fn = arguments[u"file_path"_s].toString();
    if (fn.isEmpty())
        return Result::error(u"No file_path supplied"_s);

    if (auto *existingDocument = DocumentList::inst()->documentForFile(fn)) {
        emit existingDocument->requestActivation();
        return documentResult(existingDocument, QJsonObject { { u"already_open"_s, true } });
    }

    try {
        return documentResult(Document::loadFromFile(fn));
    } catch (const Exception &e) {
        return Result::error(plainText(e.errorString()));
    }
}


DocumentImportBrickLinkXmlMcpTool::DocumentImportBrickLinkXmlMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentImportBrickLinkXmlMcpTool::name() const
{
    return u"document_import_bl_xml"_s;
}

QString DocumentImportBrickLinkXmlMcpTool::description() const
{
    return u"Import a BrickLink XML file into a new BrickStore document."_s;
}

QJsonObject DocumentImportBrickLinkXmlMcpTool::inputSchema() const
{
    return filePathSchema(u"Absolute path of the BrickLink XML file to import"_s);
}

McpTool::Result DocumentImportBrickLinkXmlMcpTool::execute(const QJsonObject &arguments)
{
    if (QString blocked = mutationBlocked(); !blocked.isEmpty())
        return Result::error(blocked);

    const QString fn = arguments[u"file_path"_s].toString();
    if (fn.isEmpty())
        return Result::error(u"No file_path supplied"_s);

    try {
        return documentResult(DocumentIO::loadBrickLinkXML(fn));
    } catch (const Exception &e) {
        return Result::error(plainText(e.errorString()));
    }
}


DocumentImportLDrawMcpTool::DocumentImportLDrawMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentImportLDrawMcpTool::name() const
{
    return u"document_import_ldraw"_s;
}

QString DocumentImportLDrawMcpTool::description() const
{
    return u"Import an LDraw model (.dat, .ldr, .mpd) or Studio model (.io) file into a new "
           u"BrickStore document."_s;
}

QJsonObject DocumentImportLDrawMcpTool::inputSchema() const
{
    return filePathSchema(u"Absolute path of the LDraw or Studio model file to import"_s);
}

McpTool::Result DocumentImportLDrawMcpTool::execute(const QJsonObject &arguments)
{
    if (QString blocked = mutationBlocked(); !blocked.isEmpty())
        return Result::error(blocked);

    const QString fn = arguments[u"file_path"_s].toString();
    if (fn.isEmpty())
        return Result::error(u"No file_path supplied"_s);

    try {
        return documentResult(DocumentIO::loadLDrawModel(fn));
    } catch (const Exception &e) {
        return Result::error(plainText(e.errorString()));
    }
}


DocumentImportPartInventoryMcpTool::DocumentImportPartInventoryMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentImportPartInventoryMcpTool::name() const
{
    return u"document_import_part_inventory"_s;
}

QString DocumentImportPartInventoryMcpTool::description() const
{
    return u"Part out a catalog item: creates a new BrickStore document containing the "
           u"item's inventory."_s;
}

QJsonObject DocumentImportPartInventoryMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"item_id"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"The item's BrickLink ID (exact match, e.g. \"75192-1\")"_s }
            }},
            { u"item_type"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Item type as full name or single-letter BrickLink ID "
                                    u"(default: Set)"_s }
            }},
            { u"color"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Color name, only relevant for items in multiple colors"_s }
            }},
            { u"quantity"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"How many times to part out the item (default: 1)"_s }
            }},
            { u"condition"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"enum"_s, QJsonArray { u"new"_s, u"used"_s } },
                { u"description"_s, u"Condition of the parts (default: new)"_s }
            }},
            { u"extra_parts"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"enum"_s, QJsonArray { u"include"_s, u"exclude"_s, u"extra"_s } },
                { u"description"_s, u"Status for extra parts (default: extra)"_s }
            }}
        }},
        { u"required"_s, QJsonArray { u"item_id"_s } }
    };
}

McpTool::Result DocumentImportPartInventoryMcpTool::execute(const QJsonObject &arguments)
{
    if (QString blocked = mutationBlocked(); !blocked.isEmpty())
        return Result::error(blocked);

    const auto *bl = BrickLink::core();

    const QString itemTypeArg = arguments[u"item_type"_s].toString(u"S"_s).trimmed();
    const BrickLink::ItemType *itemType = nullptr;
    if (itemTypeArg.size() == 1)
        itemType = bl->itemType(itemTypeArg.at(0).toUpper().toLatin1());
    if (!itemType) {
        for (const BrickLink::ItemType &it : bl->itemTypes()) {
            if (it.name().compare(itemTypeArg, Qt::CaseInsensitive) == 0) {
                itemType = &it;
                break;
            }
        }
    }
    if (!itemType)
        return Result::error(u"Unknown item type \"%1\": valid types are %2"_s
                             .arg(itemTypeArg, validItemTypes()));

    const QString itemIdArg = arguments[u"item_id"_s].toString().trimmed();
    const BrickLink::Item *item = bl->item(itemType->id(), itemIdArg.toLatin1());
    if (!item)
        return Result::error(u"No item with ID \"%1\" and type %2: use catalog_query to search the catalog"_s
                             .arg(itemIdArg, itemType->name()));
    if (!item->hasInventory())
        return Result::error(u"Item \"%1\" has no inventory"_s.arg(itemIdArg));

    const BrickLink::Color *color = nullptr;
    if (arguments.contains(u"color"_s)) {
        color = resolveColor(arguments[u"color"_s].toString());
        if (!color)
            return Result::error(u"Unknown color \"%1\": catalog_schema lists all valid color names"_s
                                 .arg(arguments[u"color"_s].toString()));
    }

    const int quantity = arguments[u"quantity"_s].toInt(1);
    if (!quantity)
        return Result::error(u"Invalid quantity: 0"_s);

    auto condition = BrickLink::Condition::New;
    if (arguments.contains(u"condition"_s)) {
        const auto c = conditionFromString(arguments[u"condition"_s].toString());
        if (!c)
            return Result::error(u"Invalid condition: use new or used"_s);
        condition = *c;
    }

    auto extraParts = BrickLink::Status::Extra;
    if (arguments.contains(u"extra_parts"_s)) {
        const auto s = statusFromString(arguments[u"extra_parts"_s].toString());
        if (!s)
            return Result::error(u"Invalid extra_parts: use include, exclude or extra"_s);
        extraParts = *s;
    }

    return documentResult(Document::fromPartInventory(item, color, quantity, condition,
                                                      extraParts));
}


DocumentSaveMcpTool::DocumentSaveMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentSaveMcpTool::name() const
{
    return u"document_save"_s;
}

QString DocumentSaveMcpTool::description() const
{
    return u"Save an open BrickStore document as BSX, given its index from document_list. "
           u"Without file_path, the document is saved to its current file. With file_path, "
           u"this behaves like \"Save As\": the document is re-targeted to the new file."_s;
}

QJsonObject DocumentSaveMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"index"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"The document's index, as returned by document_list"_s }
            }},
            { u"file_path"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Absolute path to save to (default: the document's "
                                    u"current file)"_s }
            }}
        }},
        { u"required"_s, QJsonArray { u"index"_s } }
    };
}

McpTool::Result DocumentSaveMcpTool::execute(const QJsonObject &arguments)
{
    QString error;
    Document *doc = resolveDocument(arguments, &error);
    if (!doc)
        return Result::error(error);
    if (QString blocked = mutationBlocked(doc); !blocked.isEmpty())
        return Result::error(blocked);

    QString fn = arguments[u"file_path"_s].toString();
    if (fn.isEmpty()) {
        fn = doc->filePath();
        if (fn.isEmpty())
            return Result::error(u"The document has not been saved before: supply a file_path"_s);
    } else {
        if (!fn.endsWith(u".bsx"_s, Qt::CaseInsensitive))
            fn = fn + u".bsx";

        if (auto *other = DocumentList::inst()->documentForFile(fn); other && (other != doc))
            return Result::error(u"Another open document is already using %1: choose a different file_path"_s.arg(fn));
    }

    try {
        doc->saveToFile(fn);
        return documentResult(doc);
    } catch (const Exception &e) {
        return Result::error(plainText(e.errorString()));
    }
}


DocumentExportBrickLinkXmlMcpTool::DocumentExportBrickLinkXmlMcpTool(QObject *parent)
    : McpTool(parent)
{ }

QString DocumentExportBrickLinkXmlMcpTool::name() const
{
    return u"document_export_bl_xml"_s;
}

QString DocumentExportBrickLinkXmlMcpTool::description() const
{
    return u"Export an open BrickStore document as BrickLink XML to a file, given its index "
           u"from document_list. Exports all lots by default, or only the given rows."_s;
}

QJsonObject DocumentExportBrickLinkXmlMcpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject {
            { u"index"_s, QJsonObject {
                { u"type"_s, u"integer"_s },
                { u"description"_s, u"The document's index, as returned by document_list"_s }
            }},
            { u"file_path"_s, QJsonObject {
                { u"type"_s, u"string"_s },
                { u"description"_s, u"Absolute path of the XML file to write"_s }
            }},
            { u"rows"_s, QJsonObject {
                { u"type"_s, u"array"_s },
                { u"description"_s, u"Export only these row indexes (default: all lots)"_s },
                { u"items"_s, QJsonObject { { u"type"_s, u"integer"_s } } }
            }}
        }},
        { u"required"_s, QJsonArray { u"index"_s, u"file_path"_s } }
    };
}

McpTool::Result DocumentExportBrickLinkXmlMcpTool::execute(const QJsonObject &arguments)
{
    QString error;
    const Document *doc = resolveDocument(arguments, &error);
    if (!doc)
        return Result::error(error);

    QString fn = arguments[u"file_path"_s].toString();
    if (fn.isEmpty())
        return Result::error(u"No file_path supplied"_s);
    if (!fn.endsWith(u".xml"_s, Qt::CaseInsensitive))
        fn = fn + u".xml";

    const DocumentModel *model = doc->model();
    BrickLink::LotList exportLots;

    if (arguments.contains(u"rows"_s)) {
        const auto &lots = model->lots();
        const QJsonArray rowsArg = arguments[u"rows"_s].toArray();
        for (const QJsonValue &value : rowsArg) {
            const int row = value.toInt(-1);
            if ((row < 0) || (row >= lots.size()))
                return Result::error(u"Invalid row %1: use document_read to enumerate the lots"_s.arg(row));
            exportLots.append(lots.at(row));
        }
        if (exportLots.isEmpty())
            return Result::error(u"No rows supplied"_s);
    } else {
        exportLots = model->sortedLots();
    }

    const QByteArray xml = BrickLink::IO::toBrickLinkXML(exportLots).toUtf8();

    QSaveFile f(fn);
    f.setDirectWriteFallback(true);
    if (!f.open(QIODevice::WriteOnly) || (f.write(xml) != xml.size()) || !f.commit())
        return Result::error(u"Failed to write %1: %2"_s.arg(fn, f.errorString()));

    QJsonObject extra;
    extra[u"exported_lots"_s] = int(exportLots.size());
    extra[u"file_path"_s]     = fn;
    if (int errors = model->statistics(exportLots, true /*ignoreExcluded*/).errors())
        extra[u"note"_s] = u"%1 of the exported lots have input errors"_s.arg(errors);

    return documentResult(doc, extra);
}
