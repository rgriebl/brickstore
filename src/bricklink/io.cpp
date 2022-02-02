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
#include "utility/stopwatch.h"
#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/order.h"


QString BrickLink::IO::toBrickLinkXML(const LotList &lots)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == BrickLink::Status::Exclude))
            continue;

        xml.createElement();
        xml.createText("ITEMID", QString::fromLatin1(lot->itemId()));
        xml.createText("ITEMTYPE", QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.createText("COLOR", QString::number(lot->colorId()));
        xml.createText("CATEGORY", QString::number(lot->categoryId()));
        xml.createText("QTY", QString::number(lot->quantity()));
        xml.createText("PRICE", QString::number(fixFinite(lot->price()), 'f', 3));
        xml.createText("CONDITION", (lot->condition() == BrickLink::Condition::New) ? u"N" : u"U");

        if (lot->bulkQuantity() != 1)   xml.createText("BULK", QString::number(lot->bulkQuantity()));
        if (lot->sale())                xml.createText("SALE", QString::number(lot->sale()));
        if (!lot->comments().isEmpty()) xml.createText("DESCRIPTION", lot->comments());
        if (!lot->remarks().isEmpty())  xml.createText("REMARKS", lot->remarks());
        if (lot->retain())              xml.createText("RETAIN", u"Y");
        if (!lot->reserved().isEmpty()) xml.createText("BUYERUSERNAME", lot->reserved());
        if (!qFuzzyIsNull(lot->cost())) xml.createText("MYCOST", QString::number(fixFinite(lot->cost()), 'f', 3));
        if (lot->hasCustomWeight())     xml.createText("MYWEIGHT", QString::number(fixFinite(lot->weight()), 'f', 4));

        if (lot->tierQuantity(0)) {
            xml.createText("TQ1", QString::number(lot->tierQuantity(0)));
            xml.createText("TP1", QString::number(fixFinite(lot->tierPrice(0)), 'f', 3));
            xml.createText("TQ2", QString::number(lot->tierQuantity(1)));
            xml.createText("TP2", QString::number(fixFinite(lot->tierPrice(1)), 'f', 3));
            xml.createText("TQ3", QString::number(lot->tierQuantity(2)));
            xml.createText("TP3", QString::number(fixFinite(lot->tierPrice(2)), 'f', 3));
        }

        if (lot->subCondition() != BrickLink::SubCondition::None) {
            const char16_t *st = nullptr;
            switch (lot->subCondition()) {
            case BrickLink::SubCondition::Incomplete: st = u"I"; break;
            case BrickLink::SubCondition::Complete  : st = u"C"; break;
            case BrickLink::SubCondition::Sealed    : st = u"S"; break;
            default                                 : break;
            }
            if (st)
                xml.createText("SUBCONDITION", st);
        }
        if (lot->stockroom() != BrickLink::Stockroom::None) {
            const char16_t *st = nullptr;
            switch (lot->stockroom()) {
            case BrickLink::Stockroom::A: st = u"A"; break;
            case BrickLink::Stockroom::B: st = u"B"; break;
            case BrickLink::Stockroom::C: st = u"C"; break;
            default                     : break;
            }
            if (st) {
                xml.createText("STOCKROOM", u"Y");
                xml.createText("STOCKROOMID", st);
            }
        }
    }
    return xml.toString();
}


BrickLink::IO::ParseResult BrickLink::IO::fromBrickLinkXML(const QByteArray &data, Hint hint)
{
    //stopwatch loadXMLWatch("Load XML");

    ParseResult pr;
    QXmlStreamReader xml(data);
    QString rootName = "INVENTORY"_l1;
    if (hint == Hint::Order)
        rootName = "ORDER"_l1;

    QHash<QStringView, std::function<void(ParseResult &pr, const QString &value)>> rootTagHash;


    // The remove(',') on QTY is a workaround for the broken Order XML generator: the QTY
    // field is generated with thousands-separators enabled (e.g. 1,752 instead of 1752)

    QHash<QStringView, std::function<void(Lot *, const QString &value)>> itemTagHash {
    { u"ITEMID",       [](auto *lot, auto &v) { lot->isIncomplete()->m_item_id = v.toLatin1(); } },
    { u"COLOR",        [](auto *lot, auto &v) { lot->isIncomplete()->m_color_id = v.toUInt(); } },
    { u"CATEGORY",     [](auto *lot, auto &v) { lot->isIncomplete()->m_category_id = v.toUInt(); } },
    { u"ITEMTYPE",     [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_id = XmlHelpers::firstCharInString(v); } },
    { u"MAXPRICE",     [=](auto *lot, auto &v) { if (int(hint) & int(Hint::Wanted)) lot->setPrice(fixFinite(v.toDouble())); } },
    { u"PRICE",        [](auto *lot, auto &v) { lot->setPrice(fixFinite(v.toDouble())); } },
    { u"BULK",         [](auto *lot, auto &v) { lot->setBulkQuantity(v.toInt()); } },
    { u"MINQTY",       [=](auto *lot, auto &v) { if (int(hint) & int(Hint::Wanted)) lot->setQuantity(QString(v).remove(u',').toInt()); } },
    { u"QTY",          [](auto *lot, auto &v) { lot->setQuantity(QString(v).remove(u',').toInt()); } },
    { u"SALE",         [](auto *lot, auto &v) { lot->setSale(v.toInt()); } },
    { u"DESCRIPTION",  [](auto *lot, auto &v) { lot->setComments(v); } },
    { u"REMARKS",      [](auto *lot, auto &v) { lot->setRemarks(v); } },
    { u"TQ1",          [](auto *lot, auto &v) { lot->setTierQuantity(0, v.toInt()); } },
    { u"TQ2",          [](auto *lot, auto &v) { lot->setTierQuantity(1, v.toInt()); } },
    { u"TQ3",          [](auto *lot, auto &v) { lot->setTierQuantity(2, v.toInt()); } },
    { u"TP1",          [](auto *lot, auto &v) { lot->setTierPrice(0, fixFinite(v.toDouble())); } },
    { u"TP2",          [](auto *lot, auto &v) { lot->setTierPrice(1, fixFinite(v.toDouble())); } },
    { u"TP3",          [](auto *lot, auto &v) { lot->setTierPrice(2, fixFinite(v.toDouble())); } },
    { u"LOTID",        [](auto *lot, auto &v) { lot->setLotId(v.toUInt()); } },
    { u"RETAIN",       [](auto *lot, auto &v) { lot->setRetain(v == "Y"_l1); } },
    { u"BUYERUSERNAME",[](auto *lot, auto &v) { lot->setReserved(v); } },
    { u"MYWEIGHT",     [](auto *lot, auto &v) { lot->setWeight(fixFinite(v.toDouble())); } },
    { u"MYCOST",       [](auto *lot, auto &v) { lot->setCost(fixFinite(v.toDouble())); } },
    { u"CONDITION",    [](auto *lot, auto &v) {
        lot->setCondition(v == "N"_l1 ? Condition::New
                                      : Condition::Used); } },
    { u"SUBCONDITION", [](auto *lot, auto &v) {
        // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
        lot->setSubCondition(v == "C"_l1 ? SubCondition::Complete :
                             v == "I"_l1 ? SubCondition::Incomplete :
                             v == "M"_l1 ? SubCondition::Sealed : // legacy
                             v == "S"_l1 ? SubCondition::Sealed
                                         : SubCondition::None); } },
    { u"STOCKROOM",    [](auto *lot, auto &v) {
        if ((v == "Y"_l1) && (lot->stockroom() == Stockroom::None))
            lot->setStockroom(Stockroom::A); } },
    { u"STOCKROOMID",  [](auto *lot, auto &v) {
        lot->setStockroom(v == "A"_l1 || v.isEmpty() ? Stockroom::A :
                          v == "B"_l1 ? Stockroom::B :
                          v == "C"_l1 ? Stockroom::C
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
            if (!v.isEmpty()) {
                //TOO SLOW lot->setDateAdded({ QDate::fromString(v, "M/d/yyyy"_l1), QTime(0, 0), est });
                QStringList sl = v.split('/'_l1);
                if (sl.size() == 3) {
                    lot->setDateAdded(QDateTime({ sl.at(2).toInt(), sl.at(0).toInt(), sl.at(1).toInt() },
                                                { 0, 0, 0 }, Qt::UTC));
                }
            }
        });
        itemTagHash.insert(u"DATELASTSOLD", [](auto *lot, auto &v) {
            if (!v.isEmpty()) {
                //TOO SLOW lot->setDateLastSold(QDateTime::fromString(v % u" EST", "M/d/yyyy h:mm:ss AP t"_l1));
                QStringList sl = QString(v).replace('/'_l1, ' '_l1).replace(':'_l1, ' '_l1).split(' '_l1);
                if (sl.size() == 7) {
                    static const QTimeZone est("EST");

                    int h = sl.at(3).toInt() % 12;
                    if (sl.at(6) == "PM"_l1)
                        h += 12;
                    auto dt = QDateTime({ sl.at(2).toInt(), sl.at(0).toInt(), sl.at(1).toInt() },
                                        { h, sl.at(4).toInt(), sl.at(5).toInt() }, est);
                    lot->setDateLastSold(dt);
                }
            }
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
                } else if (tagName == "ITEM"_l1) {
                    auto *lot = new Lot();
                    auto inc = new BrickLink::Incomplete;
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
                    pr.setCurrencyCode("USD"_l1);

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

QString BrickLink::IO::toWantedListXML(const LotList &lots, const QString &wantedList)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == BrickLink::Status::Exclude))
            continue;

        xml.createElement();
        xml.createText("ITEMID", QString::fromLatin1(lot->itemId()));
        xml.createText("ITEMTYPE", QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.createText("COLOR", QString::number(lot->colorId()));

        if (lot->quantity())
            xml.createText("MINQTY", QString::number(lot->quantity()));
        if (!qFuzzyIsNull(lot->price()))
            xml.createText("MAXPRICE", QString::number(fixFinite(lot->price()), 'f', 3));
        if (!lot->remarks().isEmpty())
            xml.createText("REMARKS", lot->remarks());
        if (lot->condition() == BrickLink::Condition::New)
            xml.createText("CONDITION", u"N");
        if (!wantedList.isEmpty())
            xml.createText("WANTEDLISTID", wantedList);
    }
    return xml.toString();
}

QString BrickLink::IO::toInventoryRequest(const LotList &lots)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == BrickLink::Status::Exclude))
            continue;

        xml.createElement();
        xml.createText("ITEMID", QString::fromLatin1(lot->itemId()));
        xml.createText("ITEMTYPE", QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.createText("COLOR", QString::number(lot->colorId()));
        xml.createText("QTY", QString::number(lot->quantity()));
        if (lot->status() == BrickLink::Status::Extra)
            xml.createText("EXTRA", u"Y");
    }
    return xml.toString();
}

QString BrickLink::IO::toBrickLinkUpdateXML(const LotList &lots,
                                            std::function<const Lot *(const Lot *)> differenceBaseLot)
{
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == BrickLink::Status::Exclude))
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
            xml.createText("PRICE", QString::number(fixFinite(lot->price()), 'f', 3));
        if (!qFuzzyCompare(base->cost(), lot->cost()))
            xml.createText("MYCOST", QString::number(fixFinite(lot->cost()), 'f', 3));
        if (base->condition() != lot->condition())
            xml.createText("CONDITION", (lot->condition() == BrickLink::Condition::New) ? u"N" : u"U");
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
            xml.createText("TP1", QString::number(fixFinite(lot->tierPrice(0)), 'f', 3));
        if (base->tierQuantity(1) != lot->tierQuantity(1))
            xml.createText("TQ2", QString::number(lot->tierQuantity(1)));
        if (!qFuzzyCompare(base->tierPrice(1), lot->tierPrice(1)))
            xml.createText("TP2", QString::number(fixFinite(lot->tierPrice(1)), 'f', 3));
        if (base->tierQuantity(2) != lot->tierQuantity(2))
            xml.createText("TQ3", QString::number(lot->tierQuantity(2)));
        if (!qFuzzyCompare(base->tierPrice(2), lot->tierPrice(2)))
            xml.createText("TP3", QString::number(fixFinite(lot->tierPrice(2)), 'f', 3));

        if (base->subCondition() != lot->subCondition()) {
            const char16_t *st = nullptr;
            switch (lot->subCondition()) {
            case BrickLink::SubCondition::Incomplete: st = u"I"; break;
            case BrickLink::SubCondition::Complete  : st = u"C"; break;
            case BrickLink::SubCondition::Sealed    : st = u"S"; break;
            default                                 : break;
            }
            if (st)
                xml.createText("SUBCONDITION", st);
        }
        if (base->stockroom() != lot->stockroom()) {
            const char16_t *st = nullptr;
            switch (lot->stockroom()) {
            case BrickLink::Stockroom::A: st = u"A"; break;
            case BrickLink::Stockroom::B: st = u"B"; break;
            case BrickLink::Stockroom::C: st = u"C"; break;
            default                     : break;
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

BrickLink::IO::ParseResult::ParseResult(const LotList &lots)
    : m_lots(lots)
    , m_ownLots(false)
{ }

BrickLink::IO::ParseResult::ParseResult(ParseResult &&pr)
    : m_lots(pr.m_lots)
    , m_currencyCode(pr.m_currencyCode)
    , m_ownLots(pr.m_ownLots)
    , m_invalidLotCount(pr.m_invalidLotCount)
    , m_fixedLotCount(pr.m_fixedLotCount)
    , m_differenceModeBase(pr.m_differenceModeBase)
{
    pr.m_lots.clear();
}

BrickLink::IO::ParseResult::~ParseResult()
{
    if (m_ownLots)
        qDeleteAll(m_lots);
}

BrickLink::LotList BrickLink::IO::ParseResult::takeLots()
{
    Q_ASSERT(m_ownLots);
    m_ownLots = false;
    LotList result;
    std::swap(result, m_lots);
    return result;
}

void BrickLink::IO::ParseResult::addLot(Lot * &&lot)
{
    Q_ASSERT(m_ownLots);
    m_lots << lot;
}

void BrickLink::IO::ParseResult::addToDifferenceModeBase(const Lot *lot, const Lot &base)
{
    m_differenceModeBase.insert(lot, base);
}
