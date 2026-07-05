// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <memory>

#include "csvimport.h"

#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/color.h"
#include "bricklink/lot.h"
#include "bricklink/itemtype.h"
#include "utility/utility.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace CsvImport {

// Locale-aware helpers: try the user's locale first (handles "1,50" / "1.234,50"),
// fall back to C locale so plain machine-generated files still parse.
static double toDouble(const QString &s, const QLocale &loc)
{
    bool ok = false;
    double d = loc.toDouble(s, &ok);
    if (!ok)
        d = s.trimmed().toDouble(&ok);
    return Utility::fixFinite(ok ? d : 0.);
}

static int toInt(const QString &s, const QLocale &loc)
{
    bool ok = false;
    int i = loc.toInt(s, &ok);
    if (!ok)
        i = s.trimmed().toInt(&ok);
    return ok ? i : 0;
}

BrickLink::IO::ParseResult toLots(const QList<QStringList> &rows, const QList<Field> &mapping,
                                  const Options &options)
{
    using namespace BrickLink;
    IO::ParseResult pr;

    for (qsizetype r = (options.firstRowIsHeader ? 1 : 0); r < rows.size(); ++r) {
        const QStringList &row = rows.at(r);

        auto lot = std::make_unique<Lot>();
        auto inc = std::make_unique<Incomplete>();
        inc->m_color_id = 0;
        inc->m_category_id = 0;
        inc->m_itemtype_id = ItemType::idFromFirstCharInString(QString(QChar::fromLatin1(options.defaultItemType)));

        bool hasIdentity = false;   // did this row name an item at all?

        for (qsizetype c = 0; c < row.size() && c < mapping.size(); ++c) {
            const Field field = mapping.at(c);
            if (field == Field::Ignore)
                continue;
            const QString cell = row.at(c).trimmed();
            if (cell.isEmpty())
                continue;

            switch (field) {
            case Field::ItemId:
                inc->m_item_id = cell.toLatin1();
                hasIdentity = true;
                break;
            case Field::ItemType:
                inc->m_itemtype_id = ItemType::idFromFirstCharInString(cell);
                break;
            case Field::ColorId:
                inc->m_color_id = cell.toUInt();
                break;
            case Field::ColorName:
                if (const Color *col = core()->colorFromName(cell))
                    inc->m_color_id = col->id();
                break;
            case Field::Pcc: {
                const auto [item, color] = core()->partColorCode(cell.toUInt());
                if (item && color) {
                    inc->m_item_id = item->id();
                    inc->m_color_id = color->id();
                    inc->m_itemtype_id = ItemType::idFromFirstCharInString(u"P"_qs);
                }
                hasIdentity = true;
                break;
            }
            case Field::Condition:
                lot->setCondition(cell.startsWith(u'N', Qt::CaseInsensitive) ? Condition::New
                                                                             : Condition::Used);
                break;
            case Field::Quantity: lot->setQuantity(toInt(cell, options.locale)); break;
            case Field::Bulk:     lot->setBulkQuantity(toInt(cell, options.locale)); break;
            case Field::Sale:     lot->setSale(toInt(cell, options.locale)); break;
            case Field::Price:    lot->setPrice(toDouble(cell, options.locale)); break;
            case Field::Cost:     lot->setCost(toDouble(cell, options.locale)); break;
            case Field::Comments: lot->setComments(cell); break;
            case Field::Remarks:  lot->setRemarks(cell); break;
            case Field::Ignore:   break;
            }
        }

        if (!hasIdentity)
            continue;

        lot->setIncomplete(inc.release());

        switch (core()->resolveIncomplete(lot.get(), 0, options.creationTime)) {
        case Core::ResolveResult::Fail:      pr.incInvalidLotCount(); break;
        case Core::ResolveResult::ChangeLog: pr.incFixedLotCount(); break;
        case Core::ResolveResult::Direct:    break;
        }
        pr.addLot(lot.release());
    }
    return pr;
}

QString displayName(Field field)
{
    switch (field) {
    case Field::Ignore:    return QCoreApplication::translate("CsvImport", "(ignore)");
    case Field::ItemId:    return QCoreApplication::translate("CsvImport", "Item Id");
    case Field::ItemType:  return QCoreApplication::translate("CsvImport", "Item Type");
    case Field::ColorId:   return QCoreApplication::translate("CsvImport", "Color Id");
    case Field::ColorName: return QCoreApplication::translate("CsvImport", "Color Name");
    case Field::Pcc:       return QCoreApplication::translate("CsvImport", "LEGO Element Id (PCC)");
    case Field::Condition: return QCoreApplication::translate("CsvImport", "Condition");
    case Field::Quantity:  return QCoreApplication::translate("CsvImport", "Quantity");
    case Field::Price:     return QCoreApplication::translate("CsvImport", "Price");
    case Field::Cost:      return QCoreApplication::translate("CsvImport", "Cost");
    case Field::Bulk:      return QCoreApplication::translate("CsvImport", "Bulk Quantity");
    case Field::Sale:      return QCoreApplication::translate("CsvImport", "Sale Percent");
    case Field::Comments:  return QCoreApplication::translate("CsvImport", "Comments");
    case Field::Remarks:   return QCoreApplication::translate("CsvImport", "Remarks");
    }
    return { };
}

QList<Field> allFields()
{
    return { Field::Ignore, Field::ItemId, Field::ItemType, Field::ColorId, Field::ColorName,
             Field::Pcc, Field::Condition, Field::Quantity, Field::Price, Field::Cost,
             Field::Bulk, Field::Sale, Field::Comments, Field::Remarks };
}

} // namespace CsvImport
