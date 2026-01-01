// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QBuffer>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtCore/QTimeZone>

#include "utility/utility.h"
#include "utility/exception.h"
#include "bricklink/core.h"
#include "bricklink/io.h"


static QDateTime parseESTDateTimeString(const QString &v)
{
    if (v.isEmpty())
        return { };

    //TOO SLOW: QDateTime::fromString(v + u" EST", u"M/d/yyyy h:mm:ss AP t"));
    //TOO SLOW: { QDate::fromString(v, u"M/d/yyyy"), QTime(0, 0), est };

    QStringList sl = QString(v).replace(u'/', u' ').replace(u':', u' ').split(u' ');
    if (sl.size() == 7) {
#if QT_CONFIG(timezone)
        static const QTimeZone est("EST");
#else
        static const QTimeZone est = QTimeZone::fromSecondsAheadOfUtc(-5 * 3600);
#endif

        int h = sl.at(3).toInt() % 12;
        if (sl.at(6) == u"PM")
            h += 12;
        return QDateTime({ sl.at(2).toInt(), sl.at(0).toInt(), sl.at(1).toInt() },
                         { h, sl.at(4).toInt(), sl.at(5).toInt() }, est);
    } else if (sl.size() == 3) {
        return QDateTime({ sl.at(2).toInt(), sl.at(0).toInt(), sl.at(1).toInt() },
                         { 0, 0, 0 }, Qt::UTC);
    } else {
        return { };
    }
}

// '<' and '>' need to be double escaped, but not '&'
static QString escapeLtGt(const QString &str, bool enabled = true)
{
    if (enabled)
        return QString(str).replace(u'<', u"&lt;"_qs).replace(u'>', u"&gt;"_qs);
    return str;
}

// '<' and '>' are double escaped, but '&' isn't
static QString unescapeLtGt(const QString &str, bool enabled = true)
{
    if (enabled)
        return QString(str).replace(u"&lt;"_qs, u"<"_qs).replace(u"&gt;"_qs, u">"_qs);
    return str;
}


namespace BrickLink {

QString IO::toBrickLinkXML(const LotList &lots)
{
    bool doubleEscapedComments = core()->isApiQuirkActive(ApiQuirk::InventoryCommentsAreDoubleEscaped);
    bool doubleEscapedRemarks = core()->isApiQuirkActive(ApiQuirk::InventoryRemarksAreDoubleEscaped);

    QString out;
    QXmlStreamWriter xml(&out);
    xml.writeStartElement(u"INVENTORY"_qs);

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        xml.writeStartElement(u"ITEM"_qs);
        xml.writeTextElement(u"ITEMID"_qs, QString::fromLatin1(lot->itemId()));
        xml.writeTextElement(u"ITEMTYPE"_qs, QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.writeTextElement(u"COLOR"_qs, QString::number(lot->colorId()));
        xml.writeTextElement(u"CATEGORY"_qs, QString::number(lot->categoryId()));
        xml.writeTextElement(u"QTY"_qs, QString::number(lot->quantity()));
        xml.writeTextElement(u"PRICE"_qs, QString::number(Utility::fixFinite(lot->price()), 'f', 3));
        xml.writeTextElement(u"CONDITION"_qs, (lot->condition() == Condition::New) ? u"N"_qs : u"U"_qs);

        if (lot->bulkQuantity() != 1)   xml.writeTextElement(u"BULK"_qs, QString::number(lot->bulkQuantity()));
        if (lot->sale())                xml.writeTextElement(u"SALE"_qs, QString::number(lot->sale()));
        if (!lot->comments().isEmpty()) xml.writeTextElement(u"DESCRIPTION"_qs, doubleEscapedComments ? escapeLtGt(lot->comments()) : lot->comments());
        if (!lot->remarks().isEmpty())  xml.writeTextElement(u"REMARKS"_qs, doubleEscapedRemarks ? escapeLtGt(lot->remarks()) : lot->remarks());
        if (lot->retain())              xml.writeTextElement(u"RETAIN"_qs, u"Y"_qs);
        if (!lot->reserved().isEmpty()) xml.writeTextElement(u"BUYERUSERNAME"_qs, lot->reserved());
        if (!qFuzzyIsNull(lot->cost())) xml.writeTextElement(u"MYCOST"_qs, QString::number(Utility::fixFinite(lot->cost()), 'f', 3));
        if (lot->hasCustomWeight())     xml.writeTextElement(u"MYWEIGHT"_qs, QString::number(Utility::fixFinite(lot->weight()), 'f', 4));

        if (lot->tierQuantity(0)) {
            xml.writeTextElement(u"TQ1"_qs, QString::number(lot->tierQuantity(0)));
            xml.writeTextElement(u"TP1"_qs, QString::number(Utility::fixFinite(lot->tierPrice(0)), 'f', 3));
            xml.writeTextElement(u"TQ2"_qs, QString::number(lot->tierQuantity(1)));
            xml.writeTextElement(u"TP2"_qs, QString::number(Utility::fixFinite(lot->tierPrice(1)), 'f', 3));
            xml.writeTextElement(u"TQ3"_qs, QString::number(lot->tierQuantity(2)));
            xml.writeTextElement(u"TP3"_qs, QString::number(Utility::fixFinite(lot->tierPrice(2)), 'f', 3));
        }

        if (lot->subCondition() != SubCondition::None) {
            QChar sc;
            switch (lot->subCondition()) {
            case SubCondition::Incomplete: sc = u'I'; break;
            case SubCondition::Complete  : sc = u'C'; break;
            case SubCondition::Sealed    : sc = u'S'; break;
            default                      : break;
            }
            if (!sc.isNull())
                xml.writeTextElement(u"SUBCONDITION"_qs, QString(sc));
        }
        if (lot->stockroom() != Stockroom::None) {
            QChar sr;
            switch (lot->stockroom()) {
            case Stockroom::A: sr = u'A'; break;
            case Stockroom::B: sr = u'B'; break;
            case Stockroom::C: sr = u'C'; break;
            default          : break;
            }
            if (!sr.isNull()) {
                xml.writeTextElement(u"STOCKROOM"_qs, u"Y"_qs);
                xml.writeTextElement(u"STOCKROOMID"_qs, QString(sr));
            }
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return out;
}


IO::ParseResult IO::fromBrickLinkXML(const QByteArray &data, Hint hint, const QDateTime &creationTime)
{
    //stopwatch loadXMLWatch("Load XML");

    const bool doubleEscapedComments = core()->isApiQuirkActive(ApiQuirk::InventoryCommentsAreDoubleEscaped);
    const bool doubleEscapedRemarks = core()->isApiQuirkActive(ApiQuirk::InventoryRemarksAreDoubleEscaped);
    // The remove(',') on QTY is a workaround for the broken Order XML generator: the QTY
    // field is generated with thousands-separators enabled (e.g. 1,752 instead of 1752)
    const bool qtyHasComma = (hint == Hint::Order) && core()->isApiQuirkActive(ApiQuirk::OrderQtyHasComma);

    ParseResult pr;
    QXmlStreamReader xml(data);
    QString rootName = u"INVENTORY"_qs;
    if (hint == Hint::Order)
        rootName = u"ORDER"_qs;

    QHash<QStringView, std::function<void(ParseResult &pr, const QString &value)>> rootTagHash;

    QHash<QStringView, std::function<void(Lot *, const QString &value)>> itemTagHash {
    { u"ITEMID",       [](auto *lot, auto &v) { lot->isIncomplete()->m_item_id = v.toLatin1(); } },
    { u"COLOR",        [](auto *lot, auto &v) { lot->isIncomplete()->m_color_id = v.toUInt(); } },
    { u"CATEGORY",     [](auto *lot, auto &v) { lot->isIncomplete()->m_category_id = v.toUInt(); } },
    { u"ITEMTYPE",     [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_id = ItemType::idFromFirstCharInString(v); } },
    { u"MAXPRICE",     [=](auto *lot, auto &v) { if (int(hint) & int(Hint::Wanted)) lot->setPrice(Utility::fixFinite(v.toDouble())); } },
    { u"PRICE",        [](auto *lot, auto &v) { lot->setPrice(Utility::fixFinite(v.toDouble())); } },
    { u"BULK",         [](auto *lot, auto &v) { lot->setBulkQuantity(v.toInt()); } },
    { u"MINQTY",       [=](auto *lot, auto &v) { if (int(hint) & int(Hint::Wanted)) lot->setQuantity(QString(v).remove(u',').toInt()); } },
    { u"QTY",          [=](auto *lot, auto &v) { lot->setQuantity(qtyHasComma ? QString(v).remove(u',').toInt() : v.toInt()); } },
    { u"SALE",         [](auto *lot, auto &v) { lot->setSale(v.toInt()); } },
    { u"DESCRIPTION",  [=](auto *lot, auto &v) { lot->setComments(doubleEscapedComments ? unescapeLtGt(v) : v); } },
    { u"REMARKS",      [=](auto *lot, auto &v) { lot->setRemarks(doubleEscapedRemarks ? unescapeLtGt(v) : v); } },
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
        // 'M' for sealed is an historic artifact. BL called this 'MISB' back in the day
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

                    switch (core()->resolveIncomplete(lot, 0, creationTime)) {
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
        qsizetype pos = xml.characterOffset();
        QString context = QString::fromUtf8(data);
        auto lpos = context.lastIndexOf(u'\n', pos ? pos - 1 : 0) + 1;
        auto rpos = context.indexOf(u'\n', pos);
        context = context.mid(lpos, rpos == -1 ? context.size() : rpos - lpos);
        auto contextPos = pos - lpos - 1;

        QString msg = u"XML parse error at line %1, column %2: %3"_qs
                          .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.errorString());

        qDebug().noquote().nospace() << msg << "\n\n  " << context << "\n  "
                                     << QString(contextPos, u' ') << u'^';

        throw Exception(msg.toHtmlEscaped() + u"<br><br><tt>"
                        + context.left(contextPos).toHtmlEscaped()
                        + u"<span style=\"background-color: " + QColor(Qt::red).name() + u"\">"
                        + context.mid(contextPos, 1).toHtmlEscaped()
                        + u"</span>"
                        + context.mid(contextPos + 1).toHtmlEscaped()
                        + u"</tt>");
    }
}

QString IO::toWantedListXML(const LotList &lots, const QString &wantedList)
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml.writeStartElement(u"INVENTORY"_qs);

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        xml.writeStartElement(u"ITEM"_qs);
        xml.writeTextElement(u"ITEMID"_qs, QString::fromLatin1(lot->itemId()));
        xml.writeTextElement(u"ITEMTYPE"_qs, QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.writeTextElement(u"COLOR"_qs, QString::number(lot->colorId()));

        if (lot->quantity())
            xml.writeTextElement(u"MINQTY"_qs, QString::number(lot->quantity()));
        if (!qFuzzyIsNull(lot->price()))
            xml.writeTextElement(u"MAXPRICE"_qs, QString::number(Utility::fixFinite(lot->price()), 'f', 3));
        if (!lot->remarks().isEmpty())
            xml.writeTextElement(u"REMARKS"_qs, escapeLtGt(lot->remarks()));
        if (lot->condition() == Condition::New)
            xml.writeTextElement(u"CONDITION"_qs, u"N"_qs);
        if (!wantedList.isEmpty())
            xml.writeTextElement(u"WANTEDLISTID"_qs, wantedList);

        xml.writeEndElement();
    }
    xml.writeEndElement();
    return out;
}

QString IO::toInventoryRequest(const LotList &lots)
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml.writeStartElement(u"INVENTORY"_qs);

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        xml.writeStartElement(u"ITEM"_qs);
        xml.writeTextElement(u"ITEMID"_qs, QString::fromLatin1(lot->itemId()));
        xml.writeTextElement(u"ITEMTYPE"_qs, QString(QChar::fromLatin1(lot->itemTypeId())));
        xml.writeTextElement(u"COLOR"_qs, QString::number(lot->colorId()));
        xml.writeTextElement(u"QTY"_qs, QString::number(lot->quantity()));
        if (lot->status() == Status::Extra)
            xml.writeTextElement(u"EXTRA"_qs, u"Y"_qs);

        xml.writeEndElement();
    }
    xml.writeEndElement();
    return out;
}

QString IO::toBrickLinkUpdateXML(const LotList &lots,
                                 const std::function<const Lot *(const Lot *)> &differenceBaseLot)
{
    bool doubleEscapedComments = core()->isApiQuirkActive(ApiQuirk::InventoryCommentsAreDoubleEscaped);
    bool doubleEscapedRemarks = core()->isApiQuirkActive(ApiQuirk::InventoryRemarksAreDoubleEscaped);

    QString out;
    QXmlStreamWriter xml(&out);
    xml.writeStartElement(u"INVENTORY"_qs);

    for (const Lot *lot : lots) {
        if (lot->isIncomplete() || (lot->status() == Status::Exclude))
            continue;

        auto *base = differenceBaseLot(lot);
        if (!base)
            continue;

        // we don't care about reserved, status and marker, so we have to mask it
        auto baseLot = *base;
        baseLot.setReserved(lot->reserved());
        baseLot.setStatus(lot->status());
        baseLot.setMarkerColor(lot->markerColor());
        baseLot.setMarkerText(lot->markerText());

        if (baseLot == *lot)
            continue;

        xml.writeStartElement(u"ITEM"_qs);
        xml.writeTextElement(u"LOTID"_qs, QString::number(lot->lotId()));
        int qdiff = lot->quantity() - base->quantity();
        if (qdiff && (lot->quantity() > 0))
            xml.writeTextElement(u"QTY"_qs, QString::number(qdiff).prepend(qdiff > 0 ? u"+"_qs : u""_qs));
        else if (qdiff && (lot->quantity() <= 0))
            xml.writeEmptyElement(u"DELETE"_qs);

        if (!qFuzzyCompare(base->price(), lot->price()))
            xml.writeTextElement(u"PRICE"_qs, QString::number(Utility::fixFinite(lot->price()), 'f', 3));
        if (!qFuzzyCompare(base->cost(), lot->cost()))
            xml.writeTextElement(u"MYCOST"_qs, QString::number(Utility::fixFinite(lot->cost()), 'f', 3));
        if (base->condition() != lot->condition())
            xml.writeTextElement(u"CONDITION"_qs, (lot->condition() == Condition::New) ? u"N"_qs : u"U"_qs);
        if (base->bulkQuantity() != lot->bulkQuantity())
            xml.writeTextElement(u"BULK"_qs, QString::number(lot->bulkQuantity()));
        if (base->sale() != lot->sale())
            xml.writeTextElement(u"SALE"_qs, QString::number(lot->sale()));
        if (base->comments() != lot->comments())
            xml.writeTextElement(u"DESCRIPTION"_qs, doubleEscapedComments ? escapeLtGt(lot->comments()) : lot->comments());
        if (base->remarks() != lot->remarks())
            xml.writeTextElement(u"REMARKS"_qs, doubleEscapedRemarks ? escapeLtGt(lot->remarks()) : lot->remarks());
        if (base->retain() != lot->retain())
            xml.writeTextElement(u"RETAIN"_qs, lot->retain() ? u"Y"_qs : u"N"_qs);

        if ((base->tierQuantity(0) != lot->tierQuantity(0))
            || !qFuzzyCompare(base->tierPrice(0), lot->tierPrice(0))
            || (base->tierQuantity(1) != lot->tierQuantity(1))
            || !qFuzzyCompare(base->tierPrice(1), lot->tierPrice(1))
            || (base->tierQuantity(2) != lot->tierQuantity(2))
            || !qFuzzyCompare(base->tierPrice(2), lot->tierPrice(2))) {
            xml.writeTextElement(u"TQ1"_qs, QString::number(lot->tierQuantity(0)));
            xml.writeTextElement(u"TP1"_qs, QString::number(Utility::fixFinite(lot->tierPrice(0)), 'f', 3));
            xml.writeTextElement(u"TQ2"_qs, QString::number(lot->tierQuantity(1)));
            xml.writeTextElement(u"TP2"_qs, QString::number(Utility::fixFinite(lot->tierPrice(1)), 'f', 3));
            xml.writeTextElement(u"TQ3"_qs, QString::number(lot->tierQuantity(2)));
            xml.writeTextElement(u"TP3"_qs, QString::number(Utility::fixFinite(lot->tierPrice(2)), 'f', 3));
        }

        if (base->subCondition() != lot->subCondition()) {
            QChar sc;
            switch (lot->subCondition()) {
            case SubCondition::Incomplete: sc = u'I'; break;
            case SubCondition::Complete  : sc = u'C'; break;
            case SubCondition::Sealed    : sc = u'S'; break;
            default                      : break;
            }
            if (!sc.isNull())
                xml.writeTextElement(u"SUBCONDITION"_qs, QString(sc));
        }
        if (base->stockroom() != lot->stockroom()) {
            QChar sr;
            switch (lot->stockroom()) {
            case Stockroom::A: sr = u'A'; break;
            case Stockroom::B: sr = u'B'; break;
            case Stockroom::C: sr = u'C'; break;
            default          : break;
            }
            xml.writeTextElement(u"STOCKROOM"_qs, !sr.isNull() ? u"Y"_qs : u"N"_qs);
            if (!sr.isNull())
                xml.writeTextElement(u"STOCKROOMID"_qs, QString(sr));
        }

        // Ignore the weight - it's just too confusing:
        // BrickStore displays the total weight, but that is dependent on the quantity.
        // On the other hand, the update would be done on the item weight.
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return out;
}

IO::ParseResult::ParseResult(const LotList &lots)
    : m_lots(lots)
    , m_ownLots(false)
{ }

IO::ParseResult::ParseResult(ParseResult &&pr) noexcept
    : m_lots(std::move(pr.m_lots))
    , m_currencyCode(std::move(pr.m_currencyCode))
    , m_ownLots(pr.m_ownLots)
    , m_invalidLotCount(pr.m_invalidLotCount)
    , m_fixedLotCount(pr.m_fixedLotCount)
    , m_differenceModeBase(std::move(pr.m_differenceModeBase))
{ }

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
        for (auto *lot : pr.lots()) {
            if (partItem == lot->item() && (partColor == lot->color())
                    && (part.isAlternate() == lot->alternate())
                    && (part.alternateId() == lot->alternateId())
                    && (part.isCounterPart() == lot->counterPart())
                    && (addAsExtra == (lot->status() == Status::Extra))) {
                lot->setQuantity(lot->quantity() + quantity * part.quantity());
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
