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
#include <cmath>

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>
#include <QFileInfo>
#include <QDir>
#include <QStringBuilder>
#include <QStringView>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "utility/exception.h"
#include "utility/xmlhelpers.h"
#include "utility/utility.h"
#include "utility/stopwatch.h"
#include "minizip/minizip.h"
#include "bricklink/cart.h"
#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/order.h"
#include "bricklink/store.h"

#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/uihelpers.h"


QStringList DocumentIO::nameFiltersForBrickLinkXML(bool includeAll)
{
    QStringList filters;
    filters << tr("BrickLink XML File") % " (*.xml)"_l1;
    if (includeAll)
        filters << tr("All Files") % "(*)"_l1;
    return filters;
}

QStringList DocumentIO::nameFiltersForBrickStoreXML(bool includeAll)
{
    QStringList filters;
    filters << tr("BrickStore XML Data") % " (*.bsx)"_l1;
    if (includeAll)
        filters << tr("All Files") % "(*)"_l1;
    return filters;
}

QStringList DocumentIO::nameFiltersForLDraw(bool includeAll)
{
    QStringList filters;
    filters << tr("All Models") % " (*.dat *.ldr *.mpd *.io)"_l1;
    filters << tr("LDraw Models") % " (*.dat *.ldr *.mpd)"_l1;
    filters << tr("BrickLink Studio Models") % " (*.io)"_l1;
    if (includeAll)
        filters << tr("All Files") % " (*)"_l1;
    return filters;
}

Document *DocumentIO::importBrickLinkStore(BrickLink::Store *store)
{
    Q_ASSERT(store);

    BrickLink::IO::ParseResult pr;
    const auto lots = store->lots();
    for (const auto *lot : lots)
        pr.addLot(new Lot(*lot));
    pr.setCurrencyCode(store->currencyCode());

    auto *document = new Document(new DocumentModel(std::move(pr)));
    document->setTitle(tr("Store %1").arg(QLocale().toString(store->lastUpdated(), QLocale::ShortFormat)));
    document->setThumbnail("bricklink-store"_l1);
    return document;
}

Document *DocumentIO::importBrickLinkOrder(BrickLink::Order *order)
{
    Q_ASSERT(order);

    BrickLink::IO::ParseResult pr;
    const auto lots = order->loadLots();
    for (auto *lot : lots)
        pr.addLot(std::move(lot));
    pr.setCurrencyCode(order->currencyCode());

    auto *document = new Document(new DocumentModel(std::move(pr)));
    document->setOrder(order);
    document->setTitle(tr("Order %1 (%2)").arg(order->id(), order->otherParty()));
    document->setThumbnail("view-financial-list"_l1);
    return document;
}

Document *DocumentIO::importBrickLinkCart(BrickLink::Cart *cart)
{
    Q_ASSERT(cart);

    BrickLink::IO::ParseResult pr;
    const auto &lots = cart->lots();
    for (const auto *lot : lots)
        pr.addLot(new Lot(*lot));
    pr.setCurrencyCode(cart->currencyCode());

    auto *document = new Document(new DocumentModel(std::move(pr)));
    document->setTitle(tr("Cart in store %1").arg(cart->storeName()));
    document->setThumbnail("bricklink-cart"_l1);
    return document;
}

QCoro::Task<Document *> DocumentIO::importBrickLinkXML(QString fileName)
{
    QString fn = fileName;
    if (fn.isEmpty()) {
        if (auto f = co_await UIHelpers::getOpenFileName(DocumentIO::nameFiltersForBrickLinkXML(true),
                                                         tr("Import File"))) {
            fn = *f;
        }
    }
    if (fn.isEmpty())
        co_return nullptr;

    QFile f(fn);
    if (f.open(QIODevice::ReadOnly)) {
        try {
            auto result = BrickLink::IO::fromBrickLinkXML(f.readAll(), BrickLink::IO::Hint::PlainOrWanted);
            auto *document = new Document(new DocumentModel(std::move(result))); // Document owns the items now
            document->setTitle(tr("Import of %1").arg(QFileInfo(fn).fileName()));
            co_return document;

        } catch (const Exception &e) {
            UIHelpers::warning(tr("Could not parse the XML data.") % u"<br><br>" % e.error());
        }
    } else {
        co_await UIHelpers::warning(tr("Could not open file %1 for reading.").arg(CMB_BOLD(fn)));
    }
    co_return nullptr;
}


QCoro::Task<Document *> DocumentIO::importLDrawModel(QString fileName)
{
    QString fn = fileName;
    if (fn.isEmpty()) {
        if (auto f = co_await UIHelpers::getOpenFileName(DocumentIO::nameFiltersForLDraw(true),
                                                         tr("Import File"))) {
            fn = *f;
        }
    }
    if (fn.isEmpty())
        co_return nullptr;

    try {
        QScopedPointer<QFile> f;
        bool isStudio = fn.endsWith(".io"_l1);

        if (isStudio) {
            stopwatch unpack("unpack/decrypt studio zip");

            // this is a zip file - unpack the encrypted model2.ldr (pw: soho0909)

            f.reset(new QTemporaryFile());

            if (f->open(QIODevice::ReadWrite)) {
                try {
                    MiniZip::unzip(fn, f.get(), "model2.ldr", "soho0909");
                    f->close();
                } catch (const Exception &e) {
                    throw Exception(tr("Could not open the Studio ZIP container") % u": " % e.error());
                }
            }
        } else {
            f.reset(new QFile(fn));
        }

        if (!f->open(QIODevice::ReadOnly))
            throw Exception(f.get(), tr("Could not open LDraw file for reading"));

        QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        BrickLink::IO::ParseResult pr;

        bool b = DocumentIO::parseLDrawModel(f.get(), isStudio, pr);
        Document *document = nullptr;

        QGuiApplication::restoreOverrideCursor();

        if (!b || !pr.hasLots())
            throw Exception(tr("Could not parse the LDraw data"));

        document = new Document(new DocumentModel(std::move(pr))); // Document owns the items now
        document->setTitle(tr("Import of %1").arg(QFileInfo(fn).fileName()));
        co_return document;

    } catch (const Exception &e) {
        UIHelpers::warning(tr("Failed to import the LDraw/Studio model %1")
                           .arg(QFileInfo(fn).fileName()) % u":<br><br>" % e.error());
    }
    co_return nullptr;
}

bool DocumentIO::parseLDrawModel(QFile *f, bool isStudio, BrickLink::IO::ParseResult &pr)
{
    QVector<QString> recursion_detection;
    QHash<QString, QVector<Lot *>> subCache;
    QVector<Lot *> ldrawLots;

    {
        stopwatch parse("parse ldraw model");

        if (!parseLDrawModelInternal(f, isStudio, QString(), ldrawLots, subCache, recursion_detection))
            return false;
    }
    {
        stopwatch consolidate("removing duplicates");
        // consolidate everything
        for (int i = 0; i < ldrawLots.count(); ++i) {
            if (auto *lot = ldrawLots[i]) {
                for (int j = i + 1; j < ldrawLots.count(); ++j) {
                    if (auto *&otherLot = ldrawLots[j]) {
                        if (lot == otherLot) {
                            lot->setQuantity(lot->quantity() + 1);
                            otherLot = nullptr;
                        }
                    }
                }
            }
        }
    }
    {
        stopwatch consolidate("consolidate ldraw model");
        // consolidate everything
        for (int i = 0; i < ldrawLots.count(); ++i) {
            if (auto *lot = ldrawLots[i]) {
                if (lot->isIncomplete())
                    pr.incInvalidLotCount();

                for (int j = i + 1; j < ldrawLots.count(); ++j) {
                    if (auto *&otherLot = ldrawLots[j]) {
                        bool mergeable = false;
                        if (!lot->isIncomplete() && !otherLot->isIncomplete()) {
                            mergeable = (lot->item() == otherLot->item())
                                    && (lot->color() == otherLot->color());
                        } else if (lot->isIncomplete() && otherLot->isIncomplete()) {
                            mergeable = (*lot->isIncomplete() == *otherLot->isIncomplete());
                        }

                        if (mergeable) {
                            lot->setQuantity(lot->quantity() + otherLot->quantity());
                            delete otherLot;
                            otherLot = nullptr;
                        }
                    }
                }
                pr.addLot(std::move(lot));
            }
        }
    }
    return true;
}

bool DocumentIO::parseLDrawModelInternal(QFile *f, bool isStudio, const QString &modelName,
                                         QVector<Lot *> &lots,
                                         QHash<QString, QVector<Lot *>> &subCache,
                                         QVector<QString> &recursionDetection)
{
    auto it = subCache.constFind(modelName);
    if (it != subCache.cend()) {
        lots = it.value();
        return true;
    }

    if (recursionDetection.contains(modelName))
        return false;
    recursionDetection.append(modelName);

    QStringList searchpath;
    int linecount = 0;

    bool is_mpd = false;
    int current_mpd_index = -1;
    QString current_mpd_model;
    bool is_mpd_model_found = false;


    searchpath.append(QFileInfo(*f).dir().absolutePath());
    if (!BrickLink::core()->ldrawDataPath().isEmpty()) {
        searchpath.append(BrickLink::core()->ldrawDataPath() + "/models"_l1);
    }

    if (f->isOpen()) {
        QTextStream in(f);
        QString line;

        while (!(line = in.readLine()).isNull()) {
            linecount++;

            if (line.isEmpty())
                continue;

            if (line.at(0) == QLatin1Char('0')) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                const auto split = line.splitRef(QLatin1Char(' '), Qt::SkipEmptyParts);
                auto strPosition = [](const QStringRef &sr) { return sr.position(); };
#else
                const auto split = QStringView{line}.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                auto strPosition = [line](QStringView sv) { return line.constData() - sv.constData(); };
#endif
                if ((split.count() >= 2) && (split.at(1) == "FILE"_l1)) {
                    is_mpd = true;
                    current_mpd_model = line.mid(strPosition(split.at(2))).toLower();
                    current_mpd_index++;

                    if (is_mpd_model_found)
                        break;

                    if ((current_mpd_model == modelName.toLower()) || (modelName.isEmpty() && (current_mpd_index == 0)))
                        is_mpd_model_found = true;

                    if (f->isSequential())
                        return false; // we need to seek!
                }

            } else if (line.at(0) == QLatin1Char('1')) {
                if (is_mpd && !is_mpd_model_found)
                    continue;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                const auto split = line.splitRef(QLatin1Char(' '), Qt::SkipEmptyParts);
                auto strPosition = [](const QStringRef &sr) { return sr.position(); };
#else
                const auto split = QStringView{line}.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                auto strPosition = [line](QStringView sv) { return sv.constData() - line.constData(); };
#endif

                if (split.count() >= 15) {
                    uint colid = split.at(1).toUInt();
                    QString partname = line.mid(strPosition(split.at(14))).toLower();

                    QString partid = partname;
                    partid.truncate(partid.lastIndexOf(QLatin1Char('.')));

                    const BrickLink::Item *itemp = BrickLink::core()->item('P', partid.toLatin1());

                    if (!itemp && !partname.endsWith(".dat"_l1)) {
                        bool got_subfile = false;
                        QVector<Lot *> subLots;

                        if (is_mpd) {
                            qint64 oldpos = f->pos();
                            f->seek(0);

                            got_subfile = parseLDrawModelInternal(f, isStudio, partname, subLots, subCache, recursionDetection);

                            f->seek(oldpos);
                        }

                        if (!got_subfile) {
                            for (const auto &path : qAsConst(searchpath)) {
                                QFile subf(path % u'/' % partname);

                                if (subf.open(QIODevice::ReadOnly)) {

                                    (void) parseLDrawModelInternal(&subf, isStudio, partname, subLots, subCache, recursionDetection);

                                    got_subfile = true;
                                    break;
                                }
                            }
                        }
                        if (got_subfile) {
                            subCache.insert(partname, subLots);
                            lots.append(subLots);
                            continue;
                        }
                    }

                    const BrickLink::Color *colp = isStudio ? BrickLink::core()->color(colid)
                                                            : BrickLink::core()->colorFromLDrawId(colid);

                    auto *lot = new Lot(colp, itemp);
                    lot->setQuantity(1);

                    if (!colp || !itemp) {
                        auto *inc = new BrickLink::Incomplete;

                        if (!itemp) {
                            inc->m_item_id = partid.toLatin1();
                            inc->m_itemtype_id = 'P';
                            inc->m_itemtype_name = "Part"_l1;
                        }
                        if (!colp) {
                            if (isStudio)
                                inc->m_color_id = colid;
                            else
                                inc->m_color_name = "LDraw #"_l1 + QString::number(colid);
                        }
                        lot->setIncomplete(inc);
                    }
                    lots.append(lot);
                }
            }
        }
    }

    recursionDetection.removeLast();

    return is_mpd ? is_mpd_model_found : true;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////




Document *DocumentIO::parseBsxInventory(QIODevice *in)
{
    //stopwatch loadBsxWatch("Load BSX");

    Q_ASSERT(in);
    QXmlStreamReader xml(in);
    BsxContents bsx;

    try {
        bsx.setCurrencyCode("$$$"_l1);  // flag as legacy currency

        bool foundRoot = false;
        bool foundInventory = false;

        auto parseGuiState = [&]() {
            while (xml.readNextStartElement()) {
                const auto tag = xml.name();
                bool isColumnLayout = (tag == "ColumnLayout"_l1);
                bool isSortFilter = (tag == "SortFilterState"_l1);

                if (isColumnLayout || isSortFilter) {
                    bool compressed = (xml.attributes().value("Compressed"_l1).toInt() == 1);
                    auto ba = QByteArray::fromBase64(xml.readElementText().toLatin1());
                    if (compressed)
                        ba = qUncompress(ba);

                    if (!ba.isEmpty()) {
                        if (isColumnLayout)
                            bsx.guiColumnLayout = ba;
                        if (isSortFilter)
                            bsx.guiSortFilterState = ba;
                    }
                }
            }
        };

        auto parseInventory = [&]() {
            Lot *lot = nullptr;
            QVariant legacyOrigPrice, legacyOrigQty;
            bool hasBaseValues = false;
            QXmlStreamAttributes baseValues;

            const QHash<QStringView, std::function<void(Lot *, const QString &value)>> tagHash {
            { u"ItemID",       [](auto *lot, auto &v) { lot->isIncomplete()->m_item_id = v.toLatin1(); } },
            { u"ColorID",      [](auto *lot, auto &v) { lot->isIncomplete()->m_color_id = v.toUInt(); } },
            { u"CategoryID",   [](auto *lot, auto &v) { lot->isIncomplete()->m_category_id = v.toUInt(); } },
            { u"ItemTypeID",   [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_id = XmlHelpers::firstCharInString(v); } },
            { u"ItemName",     [](auto *lot, auto &v) { lot->isIncomplete()->m_item_name = v; } },
            { u"ColorName",    [](auto *lot, auto &v) { lot->isIncomplete()->m_color_name = v; } },
            { u"CategoryName", [](auto *lot, auto &v) { lot->isIncomplete()->m_category_name = v; } },
            { u"ItemTypeName", [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_name = v; } },
            { u"Price",        [](auto *lot, auto &v) { lot->setPrice(fixFinite(v.toDouble())); } },
            { u"Bulk",         [](auto *lot, auto &v) { lot->setBulkQuantity(v.toInt()); } },
            { u"Qty",          [](auto *lot, auto &v) { lot->setQuantity(v.toInt()); } },
            { u"Sale",         [](auto *lot, auto &v) { lot->setSale(v.toInt()); } },
            { u"Comments",     [](auto *lot, auto &v) { lot->setComments(v); } },
            { u"Remarks",      [](auto *lot, auto &v) { lot->setRemarks(v); } },
            { u"TQ1",          [](auto *lot, auto &v) { lot->setTierQuantity(0, v.toInt()); } },
            { u"TQ2",          [](auto *lot, auto &v) { lot->setTierQuantity(1, v.toInt()); } },
            { u"TQ3",          [](auto *lot, auto &v) { lot->setTierQuantity(2, v.toInt()); } },
            { u"TP1",          [](auto *lot, auto &v) { lot->setTierPrice(0, fixFinite(v.toDouble())); } },
            { u"TP2",          [](auto *lot, auto &v) { lot->setTierPrice(1, fixFinite(v.toDouble())); } },
            { u"TP3",          [](auto *lot, auto &v) { lot->setTierPrice(2, fixFinite(v.toDouble())); } },
            { u"LotID",        [](auto *lot, auto &v) { lot->setLotId(v.toUInt()); } },
            { u"Retain",       [](auto *lot, auto &v) { lot->setRetain(v.isEmpty() || (v == "Y"_l1)); } },
            { u"Reserved",     [](auto *lot, auto &v) { lot->setReserved(v); } },
            { u"TotalWeight",  [](auto *lot, auto &v) { lot->setTotalWeight(fixFinite(v.toDouble())); } },
            { u"Cost",         [](auto *lot, auto &v) { lot->setCost(fixFinite(v.toDouble())); } },
            { u"Condition",    [](auto *lot, auto &v) {
                lot->setCondition(v == "N"_l1 ? BrickLink::Condition::New
                                              : BrickLink::Condition::Used); } },
            { u"SubCondition", [](auto *lot, auto &v) {
                // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
                lot->setSubCondition(v == "C"_l1 ? BrickLink::SubCondition::Complete :
                                     v == "I"_l1 ? BrickLink::SubCondition::Incomplete :
                                     v == "M"_l1 ? BrickLink::SubCondition::Sealed
                                                 : BrickLink::SubCondition::None); } },
            { u"Status",       [](auto *lot, auto &v) {
                lot->setStatus(v == "X"_l1 ? BrickLink::Status::Exclude :
                                                 v == "I"_l1 ? BrickLink::Status::Include :
                                                                  v == "E"_l1 ? BrickLink::Status::Extra
                                                                                 : BrickLink::Status::Include); } },
            { u"Stockroom",    [](auto *lot, auto &v) {
                lot->setStockroom(v == "A"_l1 || v.isEmpty() ? BrickLink::Stockroom::A :
                                  v == "B"_l1 ? BrickLink::Stockroom::B :
                                  v == "C"_l1 ? BrickLink::Stockroom::C
                                              : BrickLink::Stockroom::None); } },
            { u"MarkerText",   [](auto *lot, auto &v) { lot->setMarkerText(v); } },
            { u"MarkerColor",  [](auto *lot, auto &v) { lot->setMarkerColor(QColor(v)); } },
            { u"DateAdded",    [](auto *lot, auto &v) {
                if (!v.isEmpty())
                    lot->setDateAdded(QDateTime::fromString(v, Qt::ISODate)); } },
            { u"DateLastSold", [](auto *lot, auto &v) {
                if (!v.isEmpty())
                    lot->setDateLastSold(QDateTime::fromString(v, Qt::ISODate)); } },
            { u"OrigPrice",    [&legacyOrigPrice](auto *lot, auto &v) {
                Q_UNUSED(lot)
                legacyOrigPrice.setValue(fixFinite(v.toDouble()));
            } },
            { u"OrigQty",      [&legacyOrigQty](auto *lot, auto &v) {
                Q_UNUSED(lot)
                legacyOrigQty.setValue(v.toInt());
            } }, };

            while (xml.readNextStartElement()) {
                if (xml.name() != "Item"_l1)
                    throw Exception("Expected Item element, but got: %1").arg(xml.name());

                lot = new Lot();
                lot->setIncomplete(new BrickLink::Incomplete);
                baseValues.clear();

                while (xml.readNextStartElement()) {
                    auto tag = xml.name();

                    if (tag != "DifferenceBaseValues"_l1) {
                        auto it = tagHash.find(tag);
                        if (it != tagHash.end())
                            (*it)(lot, xml.readElementText());
                        else
                            xml.skipCurrentElement();
                    } else {
                        hasBaseValues = true;
                        baseValues = xml.attributes();
                        xml.skipCurrentElement();
                    }
                }

                switch (BrickLink::core()->resolveIncomplete(lot)) {
                case BrickLink::Core::ResolveResult::Fail: bsx.incInvalidLotCount(); break;
                case BrickLink::Core::ResolveResult::ChangeLog: bsx.incFixedLotCount(); break;
                default: break;
                }

                // convert the legacy OrigQty / OrigPrice fields
                if (!hasBaseValues && (legacyOrigPrice.isValid() || legacyOrigQty.isValid())) {
                    if (legacyOrigQty.isValid())
                        baseValues.append("Qty"_l1, QString::number(legacyOrigQty.toInt()));
                    if (!legacyOrigPrice.isNull())
                        baseValues.append("Price"_l1, QString::number(legacyOrigPrice.toDouble(), 'f', 3));
                }

                Lot base = *lot;
                if (!baseValues.isEmpty()) {
                    base.setIncomplete(new BrickLink::Incomplete);

                    for (int i = 0; i < baseValues.size(); ++i) {
                        auto attr = baseValues.at(i);
                        auto it = tagHash.find(attr.name());
                        if (it != tagHash.end()) {
                            (*it)(&base, attr.value().toString());
                        }
                    }
                    if (BrickLink::core()->resolveIncomplete(&base) == BrickLink::Core::ResolveResult::Fail) {
                        if (!base.item() && lot->item())
                            base.setItem(lot->item());
                        if (!base.color() && lot->color())
                            base.setColor(lot->color());
                        base.setIncomplete(nullptr);
                    }
                }
                bsx.addToDifferenceModeBase(lot, base);

                bsx.addLot(std::move(lot));
            }
        };


        // #################### XML PARSING STARTS HERE #####################

        // In an ideal world, BrickStock wouldn't have changed the root tag.
        // DTDs are not required anymore, but if there is a DTD, it has to be correct

        static const QVector<QString> knownTypes { "BrickStoreXML"_l1, "BrickStockXML"_l1 };

        while (true) {
            switch (xml.readNext()) {
            case QXmlStreamReader::DTD: {
                auto dtd = xml.text().toString().trimmed();

                if (!dtd.startsWith("<!DOCTYPE "_l1)
                        || !dtd.endsWith(">"_l1)
                        || !knownTypes.contains(dtd.mid(10).chopped(1))) {
                    throw Exception("Expected BrickStoreXML as DOCTYPE, but got: %1").arg(dtd);
                }
                break;
            }
            case QXmlStreamReader::StartElement:
                if (!foundRoot) { // check the root element
                    if (!knownTypes.contains(xml.name().toString()))
                        throw Exception("Expected BrickStoreXML as root element, but got: %1").arg(xml.name());
                    foundRoot = true;
                } else {
                    if (xml.name() == "Inventory"_l1) {
                        foundInventory = true;
                        bsx.setCurrencyCode(xml.attributes().value("Currency"_l1).toString());
                        parseInventory();
                    } else if ((xml.name() == "GuiState"_l1)
                                && (xml.attributes().value("Application"_l1) == "BrickStore"_l1)
                                && (xml.attributes().value("Version"_l1).toInt() == 2)) {
                        parseGuiState();
                    } else {
                        xml.skipCurrentElement();
                    }
                }
                break;

            case QXmlStreamReader::Invalid:
                throw Exception(xml.errorString());

            case QXmlStreamReader::EndDocument: {
                if (!foundRoot || !foundInventory)
                    throw Exception("Not a valid BrickStoreXML file");

                auto model = std::make_unique<DocumentModel>(std::move(bsx), (bsx.fixedLotCount() != 0) /*forceModified*/);
                if (!bsx.guiSortFilterState.isEmpty())
                    model->restoreSortFilterState(bsx.guiSortFilterState);
                return new Document(model.release(), bsx.guiColumnLayout);
            }
            default:
                break;
            }
        }
    } catch (const Exception &e) {
        throw Exception("XML parse error at line %1, column %2: %3")
                .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.error());
    }
}


bool DocumentIO::createBsxInventory(QIODevice *out, const Document *doc)
{
    if (!out)
        return false;

    QXmlStreamWriter xml(out);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(1);
    xml.writeStartDocument();

    // We don't write a DOCTYPE anymore, because RelaxNG wants the schema independent from
    // the actual XML file. As a side effect, BSX files can now be opened directly in Excel.

    xml.writeStartElement("BrickStoreXML"_l1);
    xml.writeStartElement("Inventory"_l1);
    xml.writeAttribute("Currency"_l1, doc->model()->currencyCode());

    const Lot *lot;
    const Lot *base;
    QXmlStreamAttributes baseValues;

    enum CreateFlags { Required = 0, Optional = 1, Constant = 2, WriteEmpty = 8 };

    auto create = [&](QStringView tagName, auto getter, auto stringify,
            int flags = CreateFlags::Required, const QVariant &def = { }) {

        auto v = (lot->*getter)();
        const QString t = tagName.toString();
        const bool writeEmpty = (flags & WriteEmpty);
        const bool optional = ((flags & ~WriteEmpty) == Optional);
        const bool constant = ((flags & ~WriteEmpty) == Constant);

        if (!optional || (QVariant::fromValue(v) != def)) {
            if (writeEmpty)
                xml.writeEmptyElement(t);
            else
                xml.writeTextElement(t, stringify(v));
        }
        if (!constant && base) {
            auto bv = (base->*getter)();
            if (QVariant::fromValue(v) != QVariant::fromValue(bv))
                baseValues.append(t, stringify(bv));
        }
    };
    static auto asString   = [](const QString &s)      { return s; };
    static auto asCurrency = [](double d)              { return QString::number(fixFinite(d), 'f', 3); };
    static auto asInt      = [](auto i)                { return QString::number(i); };
    static auto asDateTime = [](const QDateTime &dt)   { return dt.toString(Qt::ISODate); };

    const auto lots = doc->model()->lots();
    const auto diffModeBase = doc->model()->differenceBase();
    for (const auto *loopLot : lots) {
        lot = loopLot;
        auto &baseRef = diffModeBase[lot];
        base = &baseRef;
        baseValues.clear();

        xml.writeStartElement("Item"_l1);

        // vvv Required Fields (part 1)
        create(u"ItemID",       &Lot::itemId,       [](auto l1s) {
            return QString::fromLatin1(l1s); });
        create(u"ItemTypeID",   &Lot::itemTypeId,   [](auto c) {
            QChar qc = QLatin1Char(c);
            return qc.isPrint() ? QString(qc) :QString();
        });
        create(u"ColorID",      &Lot::colorId,      asInt);

        // vvv Redundancy Fields

        // this extra information is useful, if the e.g.the color- or item-id
        // are no longer available after a database update
        create(u"ItemName",     &Lot::itemName,     asString);
        create(u"ItemTypeName", &Lot::itemTypeName, asString);
        create(u"ColorName",    &Lot::colorName,    asString);
        create(u"CategoryID",   &Lot::categoryId,   asInt);
        create(u"CategoryName", &Lot::categoryName, asString);

        // vvv Required Fields (part 2)

        create(u"Status", &Lot::status, [](auto st) {
            switch (st) {
            default                        :
            case BrickLink::Status::Exclude: return "X"_l1;
            case BrickLink::Status::Include: return "I"_l1;
            case BrickLink::Status::Extra  : return "E"_l1;
            }
        });

        create(u"Qty",       &Lot::quantity,     asInt);
        create(u"Price",     &Lot::price,        asCurrency);
        create(u"Condition", &Lot::condition, [](auto c) {
            return (c == BrickLink::Condition::New) ? "N"_l1 : "U"_l1; });

        // vvv Optional Fields (part 2)

        create(u"SubCondition", &Lot::subCondition,
                [](auto sc) {
            // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
            switch (sc) {
            case BrickLink::SubCondition::Incomplete: return "I"_l1;
            case BrickLink::SubCondition::Complete  : return "C"_l1;
            case BrickLink::SubCondition::Sealed    : return "M"_l1;
            default                                 : return "N"_l1;
            }
        }, Optional, QVariant::fromValue(BrickLink::SubCondition::None));

        create(u"Bulk",      &Lot::bulkQuantity,  asInt,      Optional, 1);
        create(u"Sale",      &Lot::sale,          asInt,      Optional, 0);
        create(u"Cost",      &Lot::cost,          asCurrency, Optional, 0);
        create(u"Comments",  &Lot::comments,      asString,   Optional, QString());
        create(u"Remarks",   &Lot::remarks,       asString,   Optional, QString());
        create(u"Reserved",  &Lot::reserved,      asString,   Optional, QString());
        create(u"LotID",     &Lot::lotId,         asInt,      Optional, 0);
        create(u"TQ1",       &Lot::tierQuantity0, asInt,      Optional, 0);
        create(u"TP1",       &Lot::tierPrice0,    asCurrency, Optional, 0);
        create(u"TQ2",       &Lot::tierQuantity1, asInt,      Optional, 0);
        create(u"TP2",       &Lot::tierPrice1,    asCurrency, Optional, 0);
        create(u"TQ3",       &Lot::tierQuantity2, asInt,      Optional, 0);
        create(u"TP3",       &Lot::tierPrice2,    asCurrency, Optional, 0);

        create(u"Retain", &Lot::retain, [](bool b) {
            return b ? "Y"_l1 : "N"_l1; }, Optional | WriteEmpty, false);

        create(u"Stockroom", &Lot::stockroom, [](auto st) {
            switch (st) {
            case BrickLink::Stockroom::A: return "A"_l1;
            case BrickLink::Stockroom::B: return "B"_l1;
            case BrickLink::Stockroom::C: return "C"_l1;
            default                     : return "N"_l1;
            }
        }, Optional, QVariant::fromValue(BrickLink::Stockroom::None));

        if (lot->hasCustomWeight()) {
            create(u"TotalWeight", &Lot::totalWeight, [](double d) {
                return QString::number(fixFinite(d), 'f', 4); }, Required);
        }
        if (!lot->markerText().isEmpty())
            create(u"MarkerText", &Lot::markerText, asString, Constant);
        if (lot->markerColor().isValid())
            create(u"MarkerColor", &Lot::markerColor, [](QColor c) { return c.name(); }, Constant);

        if (lot->dateAdded().isValid())
            create(u"DateAdded", &Lot::dateAdded, asDateTime, Constant);
        if (lot->dateLastSold().isValid())
            create(u"DateLastSold", &Lot::dateLastSold, asDateTime, Constant);

        if (base && !baseValues.isEmpty()) {
            xml.writeStartElement("DifferenceBaseValues"_l1);
            xml.writeAttributes(baseValues);
            xml.writeEndElement(); // DifferenceBaseValues
        }
        xml.writeEndElement(); // Item
    }

    xml.writeEndElement(); // Inventory

    xml.writeStartElement("GuiState"_l1);
    xml.writeAttribute("Application"_l1, "BrickStore"_l1);
    xml.writeAttribute("Version"_l1, QString::number(2));
    QByteArray columnLayout = doc->saveColumnsState();
    if (!columnLayout.isEmpty()) {
        xml.writeStartElement("ColumnLayout"_l1);
        xml.writeAttribute("Compressed"_l1, "1"_l1);
        xml.writeCDATA(QLatin1String(qCompress(columnLayout).toBase64()));
        xml.writeEndElement(); // ColumnLayout
    }
    QByteArray sortFilterState = doc->model()->saveSortFilterState();
    if (!sortFilterState.isEmpty()) {
        xml.writeStartElement("SortFilterState"_l1);
        xml.writeAttribute("Compressed"_l1, "1"_l1);
        xml.writeCDATA(QLatin1String(qCompress(sortFilterState).toBase64()));
        xml.writeEndElement(); // SortFilterState
    }
    xml.writeEndElement(); // GuiState

    xml.writeEndElement(); // BrickStoreXML
    xml.writeEndDocument();
    return !xml.hasError();
}
