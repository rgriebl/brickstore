// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <memory>
#include <cmath>

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>
#include <QFileInfo>
#include <QDir>
#include <QStringView>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "utility/exception.h"
#include "utility/utility.h"
#include "utility/stopwatch.h"
#include "minizip/minizip.h"
#include "bricklink/cart.h"
#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/order.h"
#include "bricklink/store.h"
#include "bricklink/wantedlist.h"

#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/uihelpers.h"


QStringList DocumentIO::nameFiltersForBrickLinkXML(bool includeAll)
{
    QStringList filters;
    filters << tr("BrickLink XML File") + u" (*.xml)";
    if (includeAll)
        filters << tr("All Files") + u"(*)";
    return filters;
}

QStringList DocumentIO::nameFiltersForBrickStoreXML(bool includeAll)
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + u" (*.bsx)";
    if (includeAll)
        filters << tr("All Files") + u"(*)";
    return filters;
}

QStringList DocumentIO::nameFiltersForLDraw(bool includeAll)
{
    QStringList filters;
    filters << tr("All Models") + u" (*.dat *.ldr *.mpd *.io)";
    filters << tr("LDraw Models") + u" (*.dat *.ldr *.mpd)";
    filters << tr("BrickLink Studio Models") + u" (*.io)";
    if (includeAll)
        filters << tr("All Files") + u" (*)";
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
    document->setThumbnail(u"bricklink-store"_qs);
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
    document->setThumbnail(u"view-financial-list"_qs);
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
    document->setThumbnail(u"bricklink-cart"_qs);
    return document;
}

Document *DocumentIO::importBrickLinkWantedList(BrickLink::WantedList *wantedList)
{
    Q_ASSERT(wantedList);

    BrickLink::IO::ParseResult pr;
    const auto &lots = wantedList->lots();
    for (const auto *lot : lots)
        pr.addLot(new Lot(*lot));

    auto *document = new Document(new DocumentModel(std::move(pr)));
    QString name = wantedList->name().isEmpty() ? QString::number(wantedList->id())
                                                : wantedList->name();
    document->setTitle(tr("Wanted List %1").arg(name));
    document->setThumbnail(u"love-amarok"_qs);
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
            auto result = BrickLink::IO::fromBrickLinkXML(f.readAll(),
                                                          BrickLink::IO::Hint::PlainOrWanted,
                                                          f.fileTime(QFile::FileModificationTime));
            auto *document = new Document(new DocumentModel(std::move(result))); // Document owns the items now
            document->setTitle(tr("Import of %1").arg(QFileInfo(fn).fileName()));
            co_return document;

        } catch (const Exception &e) {
            UIHelpers::warning(tr("Could not parse the XML data.") + u"<br><br>" + e.errorString());
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
        std::unique_ptr<QFile> f;
        bool isStudio = fn.endsWith(u".io");

        if (isStudio) {
            stopwatch unpack("unpack/decrypt studio zip");

            // this is a zip file - unpack the encrypted model2.ldr (pw: soho0909)

            f = std::make_unique<QTemporaryFile>();

            if (f->open(QIODevice::ReadWrite)) {
                try {
                    MiniZip::unzip(fn, f.get(), "model2.ldr", "soho0909");
                    f->close();
                } catch (const Exception &e) {
                    throw Exception(tr("Could not open the Studio ZIP container") + u": " + e.errorString());
                }
            }
        } else {
            f = std::make_unique<QFile>(fn);
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
                           .arg(QFileInfo(fn).fileName()) + u":<br><br>" + e.errorString());
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
        // remove duplicate lot pointers that get added due to sub-module caching
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
                            // item AND color are non-null and match
                            mergeable = (lot->item() == otherLot->item())
                                        && (lot->color() == otherLot->color());

                        } else if (lot->isIncomplete() && otherLot->isIncomplete()) {
                            // either item AND color are null on both, or one is non-null and matches
                            if ((lot->item() == otherLot->item()) && (lot->color() == otherLot->color()))
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
    bool is_mpd = false;
    int current_mpd_index = -1;
    QString current_mpd_model;
    bool is_mpd_model_found = false;

    searchpath.append(QFileInfo(*f).dir().absolutePath());

    if (f->isOpen()) {
        QTextStream in(f);
        QString line;

        while (!(line = in.readLine()).isNull()) {
            if (line.isEmpty())
                continue;

            if (line.at(0) == u'0') {
                const auto split = QStringView{line}.split(u' ', Qt::SkipEmptyParts);
                auto strPosition = [line](QStringView sv) { return sv.constData() - line.constData(); };

                if ((split.count() >= 2) && (split.at(1) == u"FILE")) {
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

            } else if (line.at(0) == u'1') {
                if (is_mpd && !is_mpd_model_found)
                    continue;
                const auto split = QStringView{line}.split(u' ', Qt::SkipEmptyParts);
                auto strPosition = [line](QStringView sv) { return sv.constData() - line.constData(); };

                if (split.count() >= 15) {
                    uint colid = split.at(1).toUInt();
                    QString partname = line.mid(strPosition(split.at(14))).toLower();

                    QString partid = partname;
                    partid.truncate(partid.lastIndexOf(u'.'));

                    const BrickLink::Item *itemp = BrickLink::core()->item('P', partid.toLatin1());

                    if (!itemp && !partname.endsWith(u".dat")) {
                        bool got_subfile = false;
                        QVector<Lot *> subLots;

                        if (is_mpd) {
                            qint64 oldpos = f->pos();
                            f->seek(0);

                            got_subfile = parseLDrawModelInternal(f, isStudio, partname, subLots, subCache, recursionDetection);

                            f->seek(oldpos);
                        }

                        if (!got_subfile) {
                            for (const auto &path : std::as_const(searchpath)) {
                                QFile subf(path + u'/' + partname);

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
                                                            : BrickLink::core()->colorFromLDrawId(int(colid));

                    if (colp && (colp->id() == BrickLink::Color::InvalidId)) // LDraw-only color
                        colp = nullptr;

                    auto *lot = new Lot(itemp, colp);
                    lot->setQuantity(1);

                    if (!colp || !itemp) {
                        auto *inc = new BrickLink::Incomplete;

                        if (!itemp) {
                            inc->m_item_id = partid.toLatin1();
                            inc->m_itemtype_id = 'P';
                            inc->m_itemtype_name = u"Part"_qs;
                        }
                        if (!colp) {
                            if (isStudio)
                                inc->m_color_id = colid;
                            else
                                inc->m_color_name = u"LDraw #"_qs + QString::number(colid);
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




Document *DocumentIO::parseBsxInventory(QFile *in)
{
    //stopwatch loadBsxWatch("Load BSX");

    Q_ASSERT(in);
    QXmlStreamReader xml(in);
    BsxContents bsx;
    QDateTime creationTime = in->fileTime(QFile::FileModificationTime);
    uint startAtChangelogId = 0;

    try {
        bsx.setCurrencyCode(u"$$$"_qs);  // flag as legacy currency

        bool foundRoot = false;
        bool foundInventory = false;

        auto parseGuiState = [&]() {
            while (xml.readNextStartElement()) {
                const auto tag = xml.name();
                bool isColumnLayout = (tag == u"ColumnLayout");
                bool isSortFilter = (tag == u"SortFilterState");

                if (isColumnLayout || isSortFilter) {
                    bool compressed = (xml.attributes().value(u"Compressed"_qs).toInt() == 1);
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
            QVariant legacyOrigPrice, legacyOrigQty;
            bool hasBaseValues = false;
            QXmlStreamAttributes baseValues;

            const QHash<QStringView, std::function<void(Lot *, const QString &value)>> tagHash {
            { u"ItemID",       [](auto *lot, auto &v) { lot->isIncomplete()->m_item_id = v.toLatin1(); } },
            { u"ColorID",      [](auto *lot, auto &v) { lot->isIncomplete()->m_color_id = v.toUInt(); } },
            { u"CategoryID",   [](auto *lot, auto &v) { lot->isIncomplete()->m_category_id = v.toUInt(); } },
            { u"ItemTypeID",   [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_id = BrickLink::ItemType::idFromFirstCharInString(v); } },
            { u"ItemName",     [](auto *lot, auto &v) { lot->isIncomplete()->m_item_name = v; } },
            { u"ColorName",    [](auto *lot, auto &v) { lot->isIncomplete()->m_color_name = v; } },
            { u"CategoryName", [](auto *lot, auto &v) { lot->isIncomplete()->m_category_name = v; } },
            { u"ItemTypeName", [](auto *lot, auto &v) { lot->isIncomplete()->m_itemtype_name = v; } },
            { u"Price",        [](auto *lot, auto &v) { lot->setPrice(Utility::fixFinite(v.toDouble())); } },
            { u"Bulk",         [](auto *lot, auto &v) { lot->setBulkQuantity(v.toInt()); } },
            { u"Qty",          [](auto *lot, auto &v) { lot->setQuantity(v.toInt()); } },
            { u"Sale",         [](auto *lot, auto &v) { lot->setSale(v.toInt()); } },
            { u"Comments",     [](auto *lot, auto &v) { lot->setComments(v); } },
            { u"Remarks",      [](auto *lot, auto &v) { lot->setRemarks(v); } },
            { u"TQ1",          [](auto *lot, auto &v) { lot->setTierQuantity(0, v.toInt()); } },
            { u"TQ2",          [](auto *lot, auto &v) { lot->setTierQuantity(1, v.toInt()); } },
            { u"TQ3",          [](auto *lot, auto &v) { lot->setTierQuantity(2, v.toInt()); } },
            { u"TP1",          [](auto *lot, auto &v) { lot->setTierPrice(0, Utility::fixFinite(v.toDouble())); } },
            { u"TP2",          [](auto *lot, auto &v) { lot->setTierPrice(1, Utility::fixFinite(v.toDouble())); } },
            { u"TP3",          [](auto *lot, auto &v) { lot->setTierPrice(2, Utility::fixFinite(v.toDouble())); } },
            { u"LotID",        [](auto *lot, auto &v) { lot->setLotId(v.toUInt()); } },
            { u"Retain",       [](auto *lot, auto &v) { lot->setRetain(v.isEmpty() || (v == u"Y")); } },
            { u"Reserved",     [](auto *lot, auto &v) { lot->setReserved(v); } },
            { u"TotalWeight",  [](auto *lot, auto &v) { lot->setTotalWeight(Utility::fixFinite(v.toDouble())); } },
            { u"Cost",         [](auto *lot, auto &v) { lot->setCost(Utility::fixFinite(v.toDouble())); } },
            { u"Condition",    [](auto *lot, auto &v) {
                lot->setCondition(v == u"N" ? BrickLink::Condition::New
                                            : BrickLink::Condition::Used); } },
            { u"SubCondition", [](auto *lot, auto &v) {
                // 'M' for sealed is an historic artifact. BL called this 'MISB' back in the day
                lot->setSubCondition(v == u"C" ? BrickLink::SubCondition::Complete :
                                     v == u"I" ? BrickLink::SubCondition::Incomplete :
                                     v == u"M" ? BrickLink::SubCondition::Sealed
                                               : BrickLink::SubCondition::None); } },
            { u"Status",       [](auto *lot, auto &v) {
                lot->setStatus(v == u"X" ? BrickLink::Status::Exclude :
                               v == u"I" ? BrickLink::Status::Include :
                               v == u"E" ? BrickLink::Status::Extra
                                         : BrickLink::Status::Include); } },
            { u"Stockroom",    [](auto *lot, auto &v) {
                lot->setStockroom(v == u"A" || v.isEmpty() ? BrickLink::Stockroom::A :
                                  v == u"B" ? BrickLink::Stockroom::B :
                                  v == u"C" ? BrickLink::Stockroom::C
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
                legacyOrigPrice.setValue(Utility::fixFinite(v.toDouble()));
            } },
            { u"OrigQty",      [&legacyOrigQty](auto *lot, auto &v) {
                Q_UNUSED(lot)
                legacyOrigQty.setValue(v.toInt());
            } }, };

            while (xml.readNextStartElement()) {
                if (xml.name() != u"Item")
                    throw Exception("Expected Item element, but got: %1").arg(xml.name());

                auto lot = new Lot();
                lot->setIncomplete(new BrickLink::Incomplete);
                baseValues.clear();

                while (xml.readNextStartElement()) {
                    auto tag = xml.name();

                    if (tag != u"DifferenceBaseValues") {
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

                BrickLink::Incomplete lotIncomplete = *lot->isIncomplete();

                switch (BrickLink::core()->resolveIncomplete(lot, startAtChangelogId, creationTime)) {
                case BrickLink::Core::ResolveResult::Fail: bsx.incInvalidLotCount(); break;
                case BrickLink::Core::ResolveResult::ChangeLog: bsx.incFixedLotCount(); break;
                default: break;
                }

                // convert the legacy OrigQty / OrigPrice fields
                if (!hasBaseValues && (legacyOrigPrice.isValid() || legacyOrigQty.isValid())) {
                    if (legacyOrigQty.isValid())
                        baseValues.append(u"Qty"_qs, QString::number(legacyOrigQty.toInt()));
                    if (!legacyOrigPrice.isNull())
                        baseValues.append(u"Price"_qs, QString::number(legacyOrigPrice.toDouble(), 'f', 3));
                }

                Lot base = *lot;
                if (!baseValues.isEmpty()) {
                    base.setIncomplete(new BrickLink::Incomplete(lotIncomplete));

                    for (const auto &attr : std::as_const(baseValues)) {
                        auto it = tagHash.find(attr.name());
                        if (it != tagHash.end()) {
                            (*it)(&base, attr.value().toString());
                        }
                    }

                    if (lot->item() && lot->color() && (*base.isIncomplete() == lotIncomplete)) {
                        // skip doing the same resolve a second time
                        base.setIncomplete(nullptr);
                    } else {
                        BrickLink::core()->resolveIncomplete(&base, startAtChangelogId, creationTime);
                    }
                }
                bsx.addToDifferenceModeBase(lot, base);

                bsx.addLot(std::move(lot));
            }
        };


        // #################### XML PARSING STARTS HERE #####################

        // In an ideal world, BrickStock wouldn't have changed the root tag.
        // DTDs are not required anymore, but if there is a DTD, it has to be correct

        static const QVector<QString> knownTypes { u"BrickStoreXML"_qs, u"BrickStockXML"_qs };

        while (true) {
            switch (xml.readNext()) {
            case QXmlStreamReader::DTD: {
                auto dtd = xml.text().toString().trimmed();

                if (!dtd.startsWith(u"<!DOCTYPE ")
                        || !dtd.endsWith(u">")
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
                    if (xml.name() == u"Inventory") {
                        foundInventory = true;
                        bsx.setCurrencyCode(xml.attributes().value(u"Currency"_qs).toString());
                        startAtChangelogId = xml.attributes().value(u"BrickLinkChangelogId"_qs).toUInt();
                        parseInventory();
                    } else if ((xml.name() == u"GuiState")
                                && (xml.attributes().value(u"Application"_qs) == u"BrickStore")
                                && (xml.attributes().value(u"Version"_qs).toInt() == 2)) {
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
                .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.errorString());
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

    xml.writeStartElement(u"BrickStoreXML"_qs);
    xml.writeStartElement(u"Inventory"_qs);
    xml.writeAttribute(u"Currency"_qs, doc->model()->currencyCode());
    xml.writeAttribute(u"BrickLinkChangelogId"_qs, QString::number(BrickLink::core()->latestChangelogId()));

    const Lot *lot;
    const Lot *base;
    QXmlStreamAttributes baseValues;

    enum CreateFlags {
        Required = 0,           // needs to be serialized
        Optional = 1,           // only needs to be serialized if different from the default value
        Constant = 2,           // does not need a 'base' attribute to track changes
        SkipBaseIfDefault = 4,  // does not need a 'base' attribute if it's the default value
        EmptyElement = 8        // if serialized, write an empty element only (no text)
    };

    auto create = [&](QStringView tagName, auto getter, auto stringify,
            int flags = CreateFlags::Required, const QVariant &def = { }) {

        auto v = (lot->*getter)();
        const QString t = tagName.toString();
        const bool emptyElement = (flags & EmptyElement);
        const bool optional = (flags & Optional);
        const bool constant = (flags & Constant);
        const bool skipBaseIfDefault = (flags & SkipBaseIfDefault);

        if (!optional || (QVariant::fromValue(v) != def)) {
            if (emptyElement)
                xml.writeEmptyElement(t);
            else
                xml.writeTextElement(t, stringify(v));
        }
        if (!constant && base) {
            auto bv = (base->*getter)();
            QVariant vbv = QVariant::fromValue(bv);
            if (QVariant::fromValue(v) != vbv) {
                if (!(skipBaseIfDefault && (vbv == def)))
                    baseValues.append(t, stringify(bv));
            }
        }
    };
    static auto asString   = [](const QString &s)      { return s; };
    static auto asCurrency = [](double d)              { return QString::number(Utility::fixFinite(d), 'f', 3); };
    static auto asInt      = [](auto i)                { return QString::number(i); };
    static auto asDateTime = [](const QDateTime &dt)   { return dt.toString(Qt::ISODate); };

    const auto lots = doc->model()->lots();
    const auto diffModeBase = doc->model()->differenceBase();
    for (const auto *loopLot : lots) {
        lot = loopLot;
        auto &baseRef = diffModeBase[lot];
        base = &baseRef;
        baseValues.clear();

        xml.writeStartElement(u"Item"_qs);

        // vvv Required Fields (part 1)
        create(u"ItemID",       &Lot::itemId,       [](auto l1s) {
                return QString::fromLatin1(l1s); }, Required | SkipBaseIfDefault);
        create(u"ItemTypeID",   &Lot::itemTypeId,   [](auto c) {
            QChar qc = QLatin1Char(c);
            return qc.isPrint() ? QString(qc) : QString();
            }, Required | SkipBaseIfDefault);
        create(u"ColorID",      &Lot::colorId,      asInt, Required | SkipBaseIfDefault);

        // vvv Redundancy Fields

        // this extra information is useful, if the e.g.the color- or item-id
        // are no longer available after a database update
        create(u"ItemName",     &Lot::itemName,     asString, Required | Constant);
        create(u"ItemTypeName", &Lot::itemTypeName, asString, Required | Constant);
        create(u"ColorName",    &Lot::colorName,    asString, Required | Constant);
        create(u"CategoryID",   &Lot::categoryId,   asInt,    Required | Constant);
        create(u"CategoryName", &Lot::categoryName, asString, Required | Constant);

        // vvv Required Fields (part 2)

        create(u"Status", &Lot::status, [](auto st) {
            switch (st) {
            default                        :
            case BrickLink::Status::Exclude: return u"X"_qs;
            case BrickLink::Status::Include: return u"I"_qs;
            case BrickLink::Status::Extra  : return u"E"_qs;
            }
        });

        create(u"Qty",       &Lot::quantity,     asInt);
        create(u"Price",     &Lot::price,        asCurrency);
        create(u"Condition", &Lot::condition, [](auto c) {
            return (c == BrickLink::Condition::New) ? u"N"_qs : u"U"_qs; });

        // vvv Optional Fields (part 2)

        create(u"SubCondition", &Lot::subCondition,
                [](auto sc) {
            // 'M' for sealed is an historic artifact. BL called this 'MISB' back in the day
            switch (sc) {
            case BrickLink::SubCondition::Incomplete: return u"I"_qs;
            case BrickLink::SubCondition::Complete  : return u"C"_qs;
            case BrickLink::SubCondition::Sealed    : return u"M"_qs;
            default                                 : return u"N"_qs;
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
            return b ? u"Y"_qs : u"N"_qs; }, Optional | EmptyElement, false);

        create(u"Stockroom", &Lot::stockroom, [](auto st) {
            switch (st) {
            case BrickLink::Stockroom::A: return u"A"_qs;
            case BrickLink::Stockroom::B: return u"B"_qs;
            case BrickLink::Stockroom::C: return u"C"_qs;
            default                     : return u"N"_qs;
            }
        }, Optional, QVariant::fromValue(BrickLink::Stockroom::None));

        if (lot->hasCustomWeight()) {
            create(u"TotalWeight", &Lot::totalWeight, [](double d) {
                return QString::number(Utility::fixFinite(d), 'f', 4); }, Required);
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
            xml.writeStartElement(u"DifferenceBaseValues"_qs);
            xml.writeAttributes(baseValues);
            xml.writeEndElement(); // DifferenceBaseValues
        }
        xml.writeEndElement(); // Item
    }

    xml.writeEndElement(); // Inventory

    xml.writeStartElement(u"GuiState"_qs);
    xml.writeAttribute(u"Application"_qs, u"BrickStore"_qs);
    xml.writeAttribute(u"Version"_qs, QString::number(2));
    QByteArray columnLayout = doc->saveColumnsState();
    if (!columnLayout.isEmpty()) {
        xml.writeStartElement(u"ColumnLayout"_qs);
        xml.writeAttribute(u"Compressed"_qs, u"1"_qs);
        xml.writeCDATA(QString::fromLatin1(qCompress(columnLayout).toBase64()));
        xml.writeEndElement(); // ColumnLayout
    }
    QByteArray sortFilterState = doc->model()->saveSortFilterState();
    if (!sortFilterState.isEmpty()) {
        xml.writeStartElement(u"SortFilterState"_qs);
        xml.writeAttribute(u"Compressed"_qs, u"1"_qs);
        xml.writeCDATA(QString::fromLatin1(qCompress(sortFilterState).toBase64()));
        xml.writeEndElement(); // SortFilterState
    }
    xml.writeEndElement(); // GuiState

    xml.writeEndElement(); // BrickStoreXML
    xml.writeEndDocument();
    return !xml.hasError();
}
