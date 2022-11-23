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
#include <QtCore/QBuffer>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QTimeZone>

#include "utility/utility.h"
#include "utility/xmlhelpers.h"
#include "utility/exception.h"
#include "bricklink/core.h"
#include "bricklink/io.h"


static QDateTime parseESTDateTimeString(const QString &v)
{
    if (v.isEmpty())
        return { };

    //TOO SLOW: QDateTime::fromString(v % u" EST", u"M/d/yyyy h:mm:ss AP t"));
    //TOO SLOW: { QDate::fromString(v, u"M/d/yyyy"), QTime(0, 0), est };

    QStringList sl = QString(v).replace(u'/', u' ').replace(u':', u' ').split(u' ');
    if (sl.size() == 7) {
        static const QTimeZone est("EST");

        int h = sl.at(3).toInt() % 12;
        if (sl.at(6) == u"PM")
            h += 12;
        return  QDateTime({ sl.at(2).toInt(), sl.at(0).toInt(), sl.at(1).toInt() },
                          { h, sl.at(4).toInt(), sl.at(5).toInt() }, est);
    } else if (sl.size() == 3) {
        return QDateTime({ sl.at(2).toInt(), sl.at(0).toInt(), sl.at(1).toInt() },
                         { 0, 0, 0 }, Qt::UTC);
    } else {
        return { };
    }
}

namespace BrickLink {

QString IO::toBrickLinkXML(const LotList &lots)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        xml.createElement();
        xml.createText("ITEMID", QString::fromLatin1(lot->itemId()));
        xml.createText("ITEMTYPE", QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.createText("COLOR", QString::number(lot->colorId()));
        xml.createText("CATEGORY", QString::number(lot->categoryId()));
        xml.createText("QTY", QString::number(lot->quantity()));
        xml.createText("PRICE", QString::number(Utility::fixFinite(lot->price()), 'f', 3));
        xml.createText("CONDITION", (lot->condition() == Condition::New) ? u"N" : u"U");

        if (lot->bulkQuantity() != 1)   xml.createText("BULK", QString::number(lot->bulkQuantity()));
        if (lot->sale())                xml.createText("SALE", QString::number(lot->sale()));
        if (!lot->comments().isEmpty()) xml.createText("DESCRIPTION", lot->comments());
        if (!lot->remarks().isEmpty())  xml.createText("REMARKS", lot->remarks());
        if (lot->retain())              xml.createText("RETAIN", u"Y");
        if (!lot->reserved().isEmpty()) xml.createText("BUYERUSERNAME", lot->reserved());
        if (!qFuzzyIsNull(lot->cost())) xml.createText("MYCOST", QString::number(Utility::fixFinite(lot->cost()), 'f', 3));
        if (lot->hasCustomWeight())     xml.createText("MYWEIGHT", QString::number(Utility::fixFinite(lot->weight()), 'f', 4));

        if (lot->tierQuantity(0)) {
            xml.createText("TQ1", QString::number(lot->tierQuantity(0)));
            xml.createText("TP1", QString::number(Utility::fixFinite(lot->tierPrice(0)), 'f', 3));
            xml.createText("TQ2", QString::number(lot->tierQuantity(1)));
            xml.createText("TP2", QString::number(Utility::fixFinite(lot->tierPrice(1)), 'f', 3));
            xml.createText("TQ3", QString::number(lot->tierQuantity(2)));
            xml.createText("TP3", QString::number(Utility::fixFinite(lot->tierPrice(2)), 'f', 3));
        }

        if (lot->subCondition() != SubCondition::None) {
            const char16_t *st = nullptr;
            switch (lot->subCondition()) {
            case SubCondition::Incomplete: st = u"I"; break;
            case SubCondition::Complete  : st = u"C"; break;
            case SubCondition::Sealed    : st = u"S"; break;
            default                      : break;
            }
            if (st)
                xml.createText("SUBCONDITION", st);
        }
        if (lot->stockroom() != Stockroom::None) {
            const char16_t *st = nullptr;
            switch (lot->stockroom()) {
            case Stockroom::A: st = u"A"; break;
            case Stockroom::B: st = u"B"; break;
            case Stockroom::C: st = u"C"; break;
            default          : break;
            }
            if (st) {
                xml.createText("STOCKROOM", u"Y");
                xml.createText("STOCKROOMID", st);
            }
        }
    }
    return xml.toString();
}


IO::ParseResult IO::fromBrickLinkXML(const QByteArray &data, Hint hint)
{
    //stopwatch loadXMLWatch("Load XML");

    ParseResult pr;
    QXmlStreamReader xml(data);
    QString rootName = u"INVENTORY"_qs;
    if (hint == Hint::Order)
        rootName = u"ORDER"_qs;

    QHash<QStringView, std::function<void(ParseResult &pr, const QString &value)>> rootTagHash;


    // The remove(',') on QTY is a workaround for the broken Order XML generator: the QTY
    // field is generated with thousands-separators enabled (e.g. 1,752 instead of 1752)

    QHash<QStringView, std::function<void(Lot *, const QString &value)>> itemTagHash {
    { u"ITEMID",       [](auto *lot, auto &v) { lot->isIncomplete()->m_item_id = v.toLatin1(); } },
    { u"COLOR",        [](auto *lot, auto &v) { lot->isIncomplete()->m_color_id = v.toUInt(); } },
    { u"CATEGORY",     [](auto *lot, auto &v) { lot->isIncomplete()->m_category_id = v.toUInt(); } },
    { u"ITEMTYPE",     [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_id = ItemType::idFromFirstCharInString(v); } },
    { u"MAXPRICE",     [=](auto *lot, auto &v) { if (int(hint) & int(Hint::Wanted)) lot->setPrice(Utility::fixFinite(v.toDouble())); } },
    { u"PRICE",        [](auto *lot, auto &v) { lot->setPrice(Utility::fixFinite(v.toDouble())); } },
    { u"BULK",         [](auto *lot, auto &v) { lot->setBulkQuantity(v.toInt()); } },
    { u"MINQTY",       [=](auto *lot, auto &v) { if (int(hint) & int(Hint::Wanted)) lot->setQuantity(QString(v).remove(u',').toInt()); } },
    { u"QTY",          [](auto *lot, auto &v) { lot->setQuantity(QString(v).remove(u',').toInt()); } },
    { u"SALE",         [](auto *lot, auto &v) { lot->setSale(v.toInt()); } },
    { u"DESCRIPTION",  [](auto *lot, auto &v) { lot->setComments(v); } },
    { u"REMARKS",      [](auto *lot, auto &v) { lot->setRemarks(v); } },
    { u"TQ1",          [](auto *lot, auto &v) { lot->setTierQuantity(0, v.toInt()); } },
    { u"TQ2",          [](auto *lot, auto &v) { lot->setTierQuantity(1, v.toInt()); } },
    { u"TQ3",          [](auto *lot, auto &v) { lot->setTierQuantity(2, v.toInt()); } },
    { u"TP1",          [](auto *lot, auto &v) { lot->setTierPrice(0, Utility::fixFinite(v.toDouble())); } },
    { u"TP2",          [](auto *lot, auto &v) { lot->setTierPrice(1, Utility::fixFinite(v.toDouble())); } },
    { u"TP3",          [](auto *lot, auto &v) { lot->setTierPrice(2, Utility::fixFinite(v.toDouble())); } },
    { u"LOTID",        [](auto *lot, auto &v) { lot->setLotId(v.toUInt()); } },
    { u"RETAIN",       [](auto *lot, auto &v) { lot->setRetain(v == u"Y"); } },
    { u"BUYERUSERNAME",[](auto *lot, auto &v) { lot->setReserved(v); } },
    { u"MYWEIGHT",     [](auto *lot, auto &v) { lot->setWeight(Utility::fixFinite(v.toDouble())); } },
    { u"MYCOST",       [](auto *lot, auto &v) { lot->setCost(Utility::fixFinite(v.toDouble())); } },
    { u"CONDITION",    [](auto *lot, auto &v) {
        lot->setCondition(v == u"N" ? Condition::New
                                    : Condition::Used); } },
    { u"SUBCONDITION", [](auto *lot, auto &v) {
        // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
        lot->setSubCondition(v == u"C" ? SubCondition::Complete :
                             v == u"I" ? SubCondition::Incomplete :
                             v == u"M" ? SubCondition::Sealed : // legacy
                             v == u"S" ? SubCondition::Sealed
                                       : SubCondition::None); } },
    { u"STOCKROOM",    [](auto *lot, auto &v) {
        if ((v == u"Y") && (lot->stockroom() == Stockroom::None))
            lot->setStockroom(Stockroom::A); } },
    { u"STOCKROOMID",  [](auto *lot, auto &v) {
        lot->setStockroom(v == u"A" || v.isEmpty() ? Stockroom::A :
                          v == u"B" ? Stockroom::B :
                          v == u"C" ? Stockroom::C
                                    : Stockroom::None); } },
    { u"BASECURRENCYCODE", [&pr](auto *, auto &v) {
        if (!v.isEmpty()) {
            if (pr.currencyCode().isEmpty())
                pr.setCurrencyCode(v);
            else if (pr.currencyCode() != v)
                throw Exception("Multiple currencies in one XML file are not supported.");
        } } },
    };
    if (hint == Hint::Order) {
        itemTagHash.insert(u"ORDERBATCH", [](auto *lot, auto &v) { lot->setMarkerText(v); });
    }
    if (hint == Hint::Store) {
        // Both dates are in EST local time and follow DST.
        // QDateTime::fromString is slow, especially with time zones.
        // So we run a hand-crafted parser and convert to UTC right away.

        itemTagHash.insert(u"DATEADDED", [](auto *lot, auto &v) {
            // For whatever reason this is missing the time when added via the HTML or XML
            // interfaces, but has a time field, when added via the REST API
            lot->setDateAdded(parseESTDateTimeString(v));
        });
        itemTagHash.insert(u"DATELASTSOLD", [](auto *lot, auto &v) {
            lot->setDateLastSold(parseESTDateTimeString(v));
        });
    }

    try {
        bool foundRoot = false;

        while (true) {
            switch (xml.readNext()) {
            case QXmlStreamReader::StartElement: {
                auto tagName = xml.name();
                if (!foundRoot) { // check the root element
                    if (tagName.toString() != rootName)
                        throw Exception("Expected %1 as root element, but got: %2").arg(rootName).arg(tagName);
                    foundRoot = true;
                } else if (tagName == u"ITEM") {
                    auto *lot = new Lot();
                    auto inc = new Incomplete;
                    inc->m_color_id = 0;
                    inc->m_category_id = 0;
                    lot->setIncomplete(inc);

                    while (xml.readNextStartElement()) {
                        auto it = itemTagHash.find(xml.name());
                        if (it != itemTagHash.end())
                            (*it)(lot, xml.readElementText());
                        else
                            xml.skipCurrentElement();
                    }

                    switch (core()->resolveIncomplete(lot)) {
                    case Core::ResolveResult::Fail: pr.incInvalidLotCount(); break;
                    case Core::ResolveResult::ChangeLog: pr.incFixedLotCount(); break;
                    default: break;
                    }

                    pr.addLot(std::move(lot));
                } else {
                    auto it = rootTagHash.find(xml.name());
                    if (it != rootTagHash.end())
                        (*it)(pr, xml.readElementText());
                    else
                        xml.skipCurrentElement();
                }
                break;
            }
            case QXmlStreamReader::Invalid:
                throw Exception(xml.errorString());

            case QXmlStreamReader::EndDocument:
                if (!foundRoot)
                    throw Exception("Not a valid BrickLink XML file");

                if (pr.currencyCode().isEmpty())
                    pr.setCurrencyCode(u"USD"_qs);

                return pr;

            default:
                break;
            }
        }
    } catch (const Exception &e) {
        throw Exception("XML parse error at line %1, column %2: %3")
                .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.error());
    }
}

QString IO::toWantedListXML(const LotList &lots, const QString &wantedList)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        xml.createElement();
        xml.createText("ITEMID", QString::fromLatin1(lot->itemId()));
        xml.createText("ITEMTYPE", QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.createText("COLOR", QString::number(lot->colorId()));

        if (lot->quantity())
            xml.createText("MINQTY", QString::number(lot->quantity()));
        if (!qFuzzyIsNull(lot->price()))
            xml.createText("MAXPRICE", QString::number(Utility::fixFinite(lot->price()), 'f', 3));
        if (!lot->remarks().isEmpty())
            xml.createText("REMARKS", lot->remarks());
        if (lot->condition() == Condition::New)
            xml.createText("CONDITION", u"N");
        if (!wantedList.isEmpty())
            xml.createText("WANTEDLISTID", wantedList);
    }
    return xml.toString();
}

QString IO::toInventoryRequest(const LotList &lots)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        xml.createElement();
        xml.createText("ITEMID", QString::fromLatin1(lot->itemId()));
        xml.createText("ITEMTYPE", QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.createText("COLOR", QString::number(lot->colorId()));
        xml.createText("QTY", QString::number(lot->quantity()));
        if (lot->status() == Status::Extra)
            xml.createText("EXTRA", u"Y");
    }
    return xml.toString();
}

QString IO::toBrickLinkUpdateXML(const LotList &lots,
                                            std::function<const Lot *(const Lot *)> differenceBaseLot)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        auto *base = differenceBaseLot(lot);
        if (!base)
            continue;

        // we don't care about reserved and status, so we have to mask it
        auto baseLot = *base;
        baseLot.setReserved(lot->reserved());
        baseLot.setStatus(lot->status());

        if (baseLot == *lot)
            continue;

        xml.createElement();
        xml.createText("LOTID", QString::number(lot->lotId()));
        int qdiff = lot->quantity() - base->quantity();
        if (qdiff && (lot->quantity() > 0))
            xml.createText("QTY", QString::number(qdiff).prepend(QLatin1String(qdiff > 0 ? "+" : "")));
        else if (qdiff && (lot->quantity() <= 0))
            xml.createEmpty("DELETE");

        if (!qFuzzyCompare(base->price(), lot->price()))
            xml.createText("PRICE", QString::number(Utility::fixFinite(lot->price()), 'f', 3));
        if (!qFuzzyCompare(base->cost(), lot->cost()))
            xml.createText("MYCOST", QString::number(Utility::fixFinite(lot->cost()), 'f', 3));
        if (base->condition() != lot->condition())
            xml.createText("CONDITION", (lot->condition() == Condition::New) ? u"N" : u"U");
        if (base->bulkQuantity() != lot->bulkQuantity())
            xml.createText("BULK", QString::number(lot->bulkQuantity()));
        if (base->sale() != lot->sale())
            xml.createText("SALE", QString::number(lot->sale()));
        if (base->comments() != lot->comments())
            xml.createText("DESCRIPTION", lot->comments());
        if (base->remarks() != lot->remarks())
            xml.createText("REMARKS", lot->remarks());
        if (base->retain() != lot->retain())
            xml.createText("RETAIN", lot->retain() ? u"Y" : u"N");

        if (base->tierQuantity(0) != lot->tierQuantity(0))
            xml.createText("TQ1", QString::number(lot->tierQuantity(0)));
        if (!qFuzzyCompare(base->tierPrice(0), lot->tierPrice(0)))
            xml.createText("TP1", QString::number(Utility::fixFinite(lot->tierPrice(0)), 'f', 3));
        if (base->tierQuantity(1) != lot->tierQuantity(1))
            xml.createText("TQ2", QString::number(lot->tierQuantity(1)));
        if (!qFuzzyCompare(base->tierPrice(1), lot->tierPrice(1)))
            xml.createText("TP2", QString::number(Utility::fixFinite(lot->tierPrice(1)), 'f', 3));
        if (base->tierQuantity(2) != lot->tierQuantity(2))
            xml.createText("TQ3", QString::number(lot->tierQuantity(2)));
        if (!qFuzzyCompare(base->tierPrice(2), lot->tierPrice(2)))
            xml.createText("TP3", QString::number(Utility::fixFinite(lot->tierPrice(2)), 'f', 3));

        if (base->subCondition() != lot->subCondition()) {
            const char16_t *st = nullptr;
            switch (lot->subCondition()) {
            case SubCondition::Incomplete: st = u"I"; break;
            case SubCondition::Complete  : st = u"C"; break;
            case SubCondition::Sealed    : st = u"S"; break;
            default                      : break;
            }
            if (st)
                xml.createText("SUBCONDITION", st);
        }
        if (base->stockroom() != lot->stockroom()) {
            const char16_t *st = nullptr;
            switch (lot->stockroom()) {
            case Stockroom::A: st = u"A"; break;
            case Stockroom::B: st = u"B"; break;
            case Stockroom::C: st = u"C"; break;
            default          : break;
            }
            xml.createText("STOCKROOM", st ? u"Y" : u"N");
            if (st)
                xml.createText("STOCKROOMID", st);
        }

        // Ignore the weight - it's just too confusing:
        // BrickStore displays the total weight, but that is dependent on the quantity.
        // On the other hand, the update would be done on the item weight.
    }

    return xml.toString();
}

IO::ParseResult::ParseResult(const LotList &lots)
    : m_lots(lots)
    , m_ownLots(false)
{ }

IO::ParseResult::ParseResult(ParseResult &&pr)
    : m_lots(pr.m_lots)
    , m_currencyCode(pr.m_currencyCode)
    , m_ownLots(pr.m_ownLots)
    , m_invalidLotCount(pr.m_invalidLotCount)
    , m_fixedLotCount(pr.m_fixedLotCount)
    , m_differenceModeBase(pr.m_differenceModeBase)
{
    pr.m_lots.clear();
}

IO::ParseResult::~ParseResult()
{
    if (m_ownLots)
        qDeleteAll(m_lots);
}

LotList IO::ParseResult::takeLots()
{
    Q_ASSERT(m_ownLots);
    m_ownLots = false;
    LotList result;
    std::swap(result, m_lots);
    return result;
}

void IO::ParseResult::addLot(Lot * &&lot)
{
    Q_ASSERT(m_ownLots);
    m_lots << lot;
}

void IO::ParseResult::addToDifferenceModeBase(const Lot *lot, const Lot &base)
{
    m_differenceModeBase.insert(lot, base);
}

static void fromPartInventoryInternal(IO::ParseResult &pr, const Item *item, const Color *color,
                                      int quantity, Condition condition, Status extraParts,
                                      PartOutTraits partOutTraits, Status status)
{
    const auto &parts = item->consistsOf();

    if (partOutTraits.testFlag(PartOutTrait::Instructions)) {
        if (const auto *instructions = core()->item('I', item->id())) {
            auto *lot = new Lot(instructions, core()->color(0));
            lot->setQuantity(quantity);
            lot->setCondition(condition);
            lot->setStatus(status);

            pr.addLot(std::move(lot));
        }
    }
    if (partOutTraits.testFlag(PartOutTrait::OriginalBox)) {
        if (const auto *originalBox = core()->item('O', item->id())) {
            auto *lot = new Lot(originalBox, core()->color(0));
            lot->setQuantity(quantity);
            lot->setCondition(condition);
            lot->setStatus(status);

            pr.addLot(std::move(lot));
        }
    }

    for (const Item::ConsistsOf &part : parts) {
        const Item *partItem = part.item();
        const Color *partColor = part.color();

        if (!partItem)
            continue;

        if ((!partOutTraits.testFlag(PartOutTrait::Alternates) && part.isAlternate())
                || (!partOutTraits.testFlag(PartOutTrait::CounterParts) && part.isCounterPart())) {
            continue;
        }

        if (color && color->id() && partItem->itemType()->hasColors()
                && partColor && (partColor->id() == 0)) {
            partColor = color;
        }

        const auto itemTypeId = part.item()->itemTypeId();
        if ((partOutTraits.testFlag(PartOutTrait::SetsInSet) && (itemTypeId == 'S'))
                || (partOutTraits.testFlag(PartOutTrait::Minifigs) && (itemTypeId == 'M'))) {
            fromPartInventoryInternal(pr, part.item(), part.color(), quantity * part.quantity(),
                                      condition, extraParts, partOutTraits, status);
            continue;
        }

        if (part.isExtra() && (extraParts == Status::Exclude))
            continue;

        bool addAsExtra = part.isExtra() && (extraParts == Status::Extra);

        bool found = false;
        for (auto it = pr.lots().cbegin(); it != pr.lots().cend(); ++it) {
            if (partItem == (*it)->item() && (partColor == (*it)->color())
                    && (part.isAlternate() == (*it)->alternate())
                    && (part.alternateId() == (*it)->alternateId())
                    && (part.isCounterPart() == (*it)->counterPart())
                    && (addAsExtra == ((*it)->status() == Status::Extra))) {
                (*it)->setQuantity((*it)->quantity() + quantity * part.quantity());
                found = true;
                break;
            }
        }
        if (!found) {
            Lot *lot = new Lot(partItem, partColor);
            lot->setQuantity(part.quantity() * quantity);
            lot->setCondition(condition);
            if (addAsExtra)
                lot->setStatus(Status::Extra);
            else
                lot->setStatus(status);
            lot->setAlternate(part.isAlternate());
            lot->setAlternateId(part.alternateId());
            lot->setCounterPart(part.isCounterPart());

            pr.addLot(std::move(lot));
        }
    }
}

IO::ParseResult IO::fromPartInventory(const Item *item, const Color *color, int quantity,
                                      Condition condition, Status extraParts,
                                      PartOutTraits partOutTraits, Status status)
{
    ParseResult pr;
    fromPartInventoryInternal(pr, item, color, quantity, condition, extraParts, partOutTraits, status);
    return pr;
}

} // namespace BrickLink
