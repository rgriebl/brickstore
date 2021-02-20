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
#include <QApplication>
#include <QFileDialog>
#include <QClipboard>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringBuilder>
#include <QStringView>
#include <QBuffer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "framework.h"
#include "progressdialog.h"
#include "messagebox.h"
#include "exception.h"
#include "xmlhelpers.h"
#include "config.h"
#include "utility.h"
#include "document.h"
#include "documentio.h"


#define qL1S(x) QLatin1String(x)


QString DocumentIO::s_lastDirectory { };


Document *DocumentIO::create()
{
    auto *doc = new Document();
    doc->setTitle(tr("Untitled"));
    return doc;
}

Document *DocumentIO::open()
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";
    filters << tr("All Files") + "(*.*)";

    QString dir = s_lastDirectory;
    if (dir.isEmpty())
        dir = Config::inst()->documentDir();

    auto fn = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Open File"), dir, filters.join(";;"));

    if (!fn.isNull()) {
        s_lastDirectory = QFileInfo(fn).absolutePath();

        return open(fn);
    }
    return nullptr;
}

Document *DocumentIO::open(const QString &s)
{
    if (s.isEmpty())
        return nullptr;

    QString abs_s = QFileInfo(s).absoluteFilePath();

    const auto all = Document::allDocuments();
    for (Document *doc : all) {
        if (QFileInfo(doc->fileName()).absoluteFilePath() == abs_s)
            return doc;
    }

    return loadFrom(s);
}

Document *DocumentIO::importBrickLinkInventory(const BrickLink::Item *item, int quantity,
                                               BrickLink::Condition condition,
                                               BrickLink::Status extraParts,
                                               bool includeInstructions)
{
    if (item && !item->hasInventory())
        return nullptr;

    if (item && (quantity != 0)) {
        BrickLink::InvItemList items = item->consistsOf();

        if (!items.isEmpty()) {
            for (BrickLink::InvItem *item : items) {
                item->setQuantity(item->quantity() * quantity);
                item->setCondition(condition);

                if (item->status() == BrickLink::Status::Extra)
                    item->setStatus(extraParts);
            }
            if (includeInstructions) {
                if (const auto *instructions = BrickLink::core()->item('I', item->id())) {
                    auto *ii =  new BrickLink::InvItem(BrickLink::core()->color(0), instructions);
                    ii->setQuantity(quantity);
                    ii->setCondition(condition);
                    items << ii;
                }
            }

            auto *doc = new Document(items); // Document own the items now
            doc->setTitle(tr("Inventory for %1").arg(item->id()));
            return doc;
        } else {
            MessageBox::warning(nullptr, { }, tr("Internal error: Could not create an Inventory object for item %1").arg(CMB_BOLD(item->id())));
        }
    }
    return nullptr;
}

Document *DocumentIO::importBrickLinkOrder(BrickLink::Order *order, const QByteArray &orderXml)
{
    if (!order)
        return nullptr;

    // we should really use QDomDocument here and scan ourselves
    int start = orderXml.indexOf("\n      <ITEM>");
    int end = orderXml.lastIndexOf("</ITEM>");
    QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><INVENTORY>\n" %
            orderXml.mid(start, end - start + 8) % "</INVENTORY>";

    try {
        auto result = fromBrickLinkXML(xml);
        auto *doc = new Document(result.first, order->currencyCode()); // Document owns the items now
        doc->setTitle(tr("Order %1 (%2)").arg(order->id(), order->otherParty()));
        doc->setOrder(order);
        return doc;

    } catch (const Exception &e) {
        MessageBox::warning(nullptr, { }, tr("Failed to import order %1").arg(order->id()) % u"<br><br>" % e.error());
        delete order;
    }
    return nullptr;
}

Document *DocumentIO::importBrickLinkStore()
{
    Transfer trans;
    ProgressDialog pd(tr("Import BrickLink Store Inventory"), &trans, FrameWork::inst());

    pd.setHeaderText(tr("Importing BrickLink Store"));

    pd.setMessageText(tr("Download: %p"));

    QUrl url("https://www.bricklink.com/invExcelFinal.asp");
    QUrlQuery query;
    query.addQueryItem("itemType",      "");
    query.addQueryItem("catID",         "");
    query.addQueryItem("colorID",       "");
    query.addQueryItem("invNew",        "");
    query.addQueryItem("itemYear",      "");
    query.addQueryItem("viewType",      "x");    // XML
    query.addQueryItem("invStock",      "Y");
    query.addQueryItem("invStockOnly",  "");
    query.addQueryItem("invQty",        "");
    query.addQueryItem("invQtyMin",     "0");
    query.addQueryItem("invQtyMax",     "0");
    query.addQueryItem("invBrikTrak",   "");
    query.addQueryItem("invDesc",       "");
    query.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
    query.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);
    url.setQuery(query);

    QByteArray xml;

    QObject::connect(&pd, &ProgressDialog::transferFinished,
                     &pd, [&pd, &xml]() {
        TransferJob *j = pd.job();
        QByteArray *data = j->data();
        bool ok = false;

        if (j->isFailed() || j->responseCode() != 200 || !j->data()) {
            pd.setErrorText(tr("Failed to download the store inventory."));
        } else if (data->startsWith("<HTML>") && data->contains("Invalid password")) {
            pd.setErrorText(tr("Either your username or password are incorrect."));
        } else {
            xml = *data;
            ok = true;
        }
        pd.setFinished(ok);
    });

    pd.post(url);
    pd.layout();

    if (pd.exec() == QDialog::Accepted) {
        try {
            auto result = fromBrickLinkXML(xml);
            auto *doc = new Document(result.first, result.second); // Document owns the items now
            doc->setTitle(tr("Store %1").arg(QLocale().toString(QDate::currentDate(), QLocale::ShortFormat)));
            return doc;

        } catch (const Exception &e) {
            MessageBox::warning(nullptr, { }, tr("Failed to import store inventory") % u"<br><br>" % e.error());
        }
    }
    return nullptr;
}

Document *DocumentIO::importBrickLinkCart(BrickLink::Cart *cart, const BrickLink::InvItemList &itemlist)
{
    auto *doc = new Document(itemlist, cart->currencyCode()); // Document owns the items now
    doc->setTitle(tr("Cart in store %1").arg(cart->storeName()));
    return doc;
}

Document *DocumentIO::importBrickLinkXML()
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";
    filters << tr("All Files") + "(*.*)";

    QString s = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), Config::inst()->documentDir(), filters.join(";;"));

    if (s.isEmpty())
        return nullptr;

    QFile f(s);
    if (f.open(QIODevice::ReadOnly)) {
        try {
            auto result = fromBrickLinkXML(f.readAll());
            auto *doc = new Document(result.first, result.second); // Document owns the items now
            doc->setTitle(tr("Import of %1").arg(QFileInfo(s).fileName()));
            return doc;

        } catch (const Exception &e) {
            MessageBox::warning(nullptr, { }, tr("Could not parse the XML data.") % u"<br><br>" % e.error());
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(s)));
    }
    return nullptr;
}

Document *DocumentIO::loadFrom(const QString &name)
{
    QFile f(name);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(name)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    ParseItemListResult result;

    QString emsg;
    int eline = 0, ecol = 0;
    QDomDocument domDoc;

    if (!domDoc.setContent(&f, &emsg, &eline, &ecol)) {
        QApplication::restoreOverrideCursor();

        MessageBox::warning(nullptr, { }, tr("Could not parse the XML data in file %1:<br /><i>Line %2, column %3: %4</i>").arg(CMB_BOLD(name)).arg(eline).arg(ecol).arg(emsg));
        return nullptr;
    }

    result = DocumentIO::parseBsxInventory(domDoc); // we own the items now

    QApplication::restoreOverrideCursor();

    if (result.xmlParseError) {
        MessageBox::warning(nullptr, { }, tr("This XML document is not a BrickStoreXML file."));
        return nullptr;
    }

    Document *doc = nullptr;

    if (result.invalidItemCount) {
        if (MessageBox::information(nullptr, { },
                                    tr("This file contains %n unknown item(s).<br /><br />Do you still want to open this file?",
                                       nullptr, result.invalidItemCount),
                                    QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
            result.invalidItemCount = 0;
        }
    }

    if (result.invalidItemCount) {
        qDeleteAll(result.items);
        return nullptr;
    }

    // Document owns the items now
    doc = new Document(result.items, result.currencyCode, result.differenceModeBase);
    doc->setGuiState(result.domGuiState);
    doc->setFileName(name);
    Config::inst()->addToRecentFiles(name);

    return doc;
}

Document *DocumentIO::importLDrawModel()
{
    QStringList filters;
    filters << tr("LDraw Models") + " (*.dat;*.ldr;*.mpd)";
    filters << tr("All Files") + "(*.*)";

    QString s = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), Config::inst()->documentDir(), filters.join(";;"));

    if (s.isEmpty())
        return nullptr;

    QFile f(s);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(s)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    int invalid_items = 0;
    BrickLink::InvItemList items; // we own the items

    bool b = DocumentIO::parseLDrawModel(f, items, &invalid_items);
    Document *doc = nullptr;

    QApplication::restoreOverrideCursor();

    if (b && !items.isEmpty()) {
        if (invalid_items) {
            if (MessageBox::information(nullptr, { },
                                        tr("This file contains %n unknown item(s).<br /><br />Do you still want to open this file?",
                                           nullptr, invalid_items),
                                        QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                invalid_items = 0;
            }
        }

        if (!invalid_items) {
            doc = new Document(items); // Document owns the items now
            items.clear();
            doc->setTitle(tr("Import of %1").arg(QFileInfo(s).fileName()));
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not parse the LDraw model in file %1.").arg(CMB_BOLD(s)));
    }

    qDeleteAll(items);
    return doc;
}

bool DocumentIO::parseLDrawModel(QFile &f, BrickLink::InvItemList &items, int *invalid_items)
{
    QHash<QString, BrickLink::InvItem *> mergehash;
    QStringList recursion_detection;

    return parseLDrawModelInternal(f, QString(), items, invalid_items, mergehash,
                                   recursion_detection);
}

bool DocumentIO::parseLDrawModelInternal(QFile &f, const QString &model_name, BrickLink::InvItemList &items,
                                         int *invalid_items, QHash<QString, BrickLink::InvItem *> &mergehash,
                                         QStringList &recursion_detection)
{
    if (recursion_detection.contains(model_name))
        return false;
    recursion_detection.append(model_name);

    QStringList searchpath;
    int invalid = 0;
    int linecount = 0;

    bool is_mpd = false;
    int current_mpd_index = -1;
    QString current_mpd_model;
    bool is_mpd_model_found = false;


    searchpath.append(QFileInfo(f).dir().absolutePath());
    if (!BrickLink::core()->ldrawDataPath().isEmpty()) {
        searchpath.append(BrickLink::core()->ldrawDataPath() + qL1S("/models"));
    }

    if (f.isOpen()) {
        QTextStream in(&f);
        QString line;

        while (!(line = in.readLine()).isNull()) {
            linecount++;

            if (!line.isEmpty() && line [0] == QLatin1Char('0')) {
                QStringList sl = line.simplified().split(' ');

                if ((sl.count() >= 2) && (sl [1] == qL1S("FILE"))) {
                    is_mpd = true;
                    sl.removeFirst();
                    sl.removeFirst();
                    current_mpd_model = sl.join(qL1S(" ")).toLower();
                    current_mpd_index++;

                    if (is_mpd_model_found)
                        break;

                    if ((current_mpd_model == model_name.toLower()) || (model_name.isEmpty() && (current_mpd_index == 0)))
                        is_mpd_model_found = true;

                    if (f.isSequential())
                        return false; // we need to seek!
                }
            }
            else if (!line.isEmpty() && line [0] == QLatin1Char('1')) {
                if (is_mpd && !is_mpd_model_found)
                    continue;

                QStringList sl = line.simplified().split(QLatin1Char(' '));

                if (sl.count() >= 15) {
                    int colid = sl[1].toInt();
                    QString partname = sl[14].toLower();
                    for (int i = 15; i < sl.count(); ++i) {
                        partname.append(QLatin1Char(' '));
                        partname.append(sl[i].toLower());
                    }

                    QString partid = partname;
                    partid.truncate(partid.lastIndexOf(QLatin1Char('.')));

                    const BrickLink::Color *colp = BrickLink::core()->colorFromLDrawId(colid);
                    const BrickLink::Item *itemp = BrickLink::core()->item('P', partid);

                    if (!itemp) {
                        bool got_subfile = false;

                        if (is_mpd) {
                            int sub_invalid_items = 0;

                            qint64 oldpos = f.pos();
                            f.seek(0);

                            got_subfile = parseLDrawModelInternal(f, partname, items, &sub_invalid_items, mergehash, recursion_detection);

                            invalid += sub_invalid_items;
                            f.seek(oldpos);
                        }

                        if (!got_subfile) {
                            for (const auto &path : qAsConst(searchpath)) {
                                QFile subf(path + QDir::separator() + partname);

                                if (subf.open(QIODevice::ReadOnly)) {
                                    int sub_invalid_items = 0;

                                    (void) parseLDrawModelInternal(subf, partname, items, &sub_invalid_items, mergehash, recursion_detection);

                                    invalid += sub_invalid_items;
                                    got_subfile = true;
                                    break;
                                }
                            }
                        }
                        if (got_subfile)
                            continue;
                    }

                    QString key = QString("%1@%2").arg(partid).arg(colid);

                    BrickLink::InvItem *ii = mergehash.value(key);

                    if (ii) {
                        ii->setQuantity(ii->quantity() + 1);
                    } else {
                        ii = new BrickLink::InvItem(colp, itemp);
                        ii->setQuantity(1);

                        if (!colp || !itemp) {
                            auto *inc = new BrickLink::InvItem::Incomplete;

                            if (!itemp) {
                                inc->m_item_id = partid;
                                inc->m_itemtype_id = qL1S("P");
                                inc->m_itemtype_name = qL1S("Part");
                            }
                            if (!colp) {
                                inc->m_color_name = qL1S("LDraw #") + QByteArray::number(colid);
                            }
                            ii->setIncomplete(inc);
                            invalid++;
                        }

                        items.append(ii);
                        mergehash.insert(key, ii);
                    }
                }
            }
        }
    }

    if (invalid_items)
        *invalid_items = invalid;

    recursion_detection.removeLast();

    return is_mpd ? is_mpd_model_found : true;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


bool DocumentIO::save(Document *doc)
{
    if (doc->fileName().isEmpty())
        return saveAs(doc);
    else if (doc->isModified())
        return saveTo(doc, doc->fileName(), false);
    return false;
}


bool DocumentIO::saveAs(Document *doc)
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";

    QString fn = doc->fileName();
    if (fn.isEmpty()) {
        fn = s_lastDirectory;
        if (fn.isEmpty())
            fn = Config::inst()->documentDir();

        if (!doc->title().isEmpty()) {
            QString t = Utility::sanitizeFileName(doc->title());
            fn = fn % u'/' % t;
        }
    }
    if (fn.right(4) == ".xml")
        fn.truncate(fn.length() - 4);

    fn = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Save File as"), fn, filters.join(";;"));

    if (!fn.isNull()) {
        s_lastDirectory = QFileInfo(fn).absolutePath();

        if (fn.right(4) != ".bsx")
            fn += ".bsx";

        return saveTo(doc, fn, false);
    }
    return false;
}


bool DocumentIO::saveTo(Document *doc, const QString &s, bool export_only)
{
    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        QDomDocument domDoc = createBsxInventory(doc);

        if (doc->hasGuiState())
            domDoc.firstChildElement().appendChild(doc->guiState());

        QByteArray output = domDoc.toByteArray();
        bool ok = (f.write(output.data(), output.size()) == qint64(output.size()));

        QApplication::restoreOverrideCursor();

        if (ok) {
            if (!export_only) {
                doc->unsetModified();
                doc->setFileName(s);

                Config::inst()->addToRecentFiles(s);
            }
            return true;
        }
        else
            MessageBox::warning(nullptr, { }, tr("Failed to save data to file %1.").arg(CMB_BOLD(s)));
    }
    else
        MessageBox::warning(nullptr, { }, tr("Failed to open file %1 for writing.").arg(CMB_BOLD(s)));

    return false;
}

void DocumentIO::exportBrickLinkInvReqClipboard(const BrickLink::InvItemList &itemlist)
{
    static QLocale c = QLocale::c();
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const BrickLink::InvItem *item : itemlist) {
        if (item->isIncomplete()
                || (item->status() == BrickLink::Status::Exclude)) {
            continue;
        }

        xml.createElement();
        xml.createText("ITEMID", item->item()->id());
        xml.createText("ITEMTYPE", QString(item->itemType()->id()));
        xml.createText("COLOR", QString::number(item->color()->id()));
        xml.createText("QTY", QString::number(item->quantity()));
        if (item->status() == BrickLink::Status::Extra)
            xml.createText("EXTRA", u"Y");
    }

    QGuiApplication::clipboard()->setText(xml.toString(), QClipboard::Clipboard);

    BrickLink::core()->openUrl(BrickLink::URL_InventoryRequest);
}

void DocumentIO::exportBrickLinkWantedListClipboard(const BrickLink::InvItemList &itemlist)
{
    QString wantedlist;

    if (MessageBox::getString(nullptr, { }, tr("Enter the ID number of Wanted List (leave blank for the default Wanted List)"), wantedlist)) {
        static QLocale c = QLocale::c();
        XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

        for (const BrickLink::InvItem *item : itemlist) {
            if (item->isIncomplete()
                    || (item->status() == BrickLink::Status::Exclude)) {
                continue;
            }

            xml.createElement();
            xml.createText("ITEMID", item->item()->id());
            xml.createText("ITEMTYPE", QString(item->itemType()->id()));
            xml.createText("COLOR", QString::number(item->color()->id()));

            if (item->quantity())
                xml.createText("MINQTY", QString::number(item->quantity()));
            if (!qFuzzyIsNull(item->price()))
                xml.createText("MAXPRICE", c.toCurrencyString(item->price(), { }, 3));
            if (!item->remarks().isEmpty())
                xml.createText("REMARKS", item->remarks());
            if (item->condition() == BrickLink::Condition::New)
                xml.createText("CONDITION", u"N");
            if (!wantedlist.isEmpty())
                xml.createText("WANTEDLISTID", wantedlist);
        }

        QGuiApplication::clipboard()->setText(xml.toString(), QClipboard::Clipboard);

        BrickLink::core()->openUrl(BrickLink::URL_WantedListUpload);
    }
}

void DocumentIO::exportBrickLinkUpdateClipboard(const Document *doc,
                                                const BrickLink::InvItemList &itemlist)
{
    bool withoutLotId = false;
    bool duplicateLotId = false;
    bool hasDifferences = false;
    QSet<uint> lotIds;
    for (const BrickLink::InvItem *item : itemlist) {
        const uint lotId = item->lotId();
        if (!lotId) {
            withoutLotId = true;
        } else {
            if (lotIds.contains(lotId))
                duplicateLotId = true;
            else
                lotIds.insert(lotId);
        }
        if (auto *base = doc->differenceBaseItem(item)) {
            if (*base != *item)
                hasDifferences = true;
        }
    }

    QStringList warnings;

    if (!hasDifferences)
        warnings << tr("This document has no differences that could be exported.");
    if (withoutLotId)
        warnings << tr("This list contains items without a BrickLink Lot-ID.");
    if (duplicateLotId)
        warnings << tr("This list contains items with duplicate BrickLink Lot-IDs.");

    if (!warnings.isEmpty()) {
        QString s = u"<ul><li>" % warnings.join(qL1S("</li><li>")) % u"</li></ul>";
        s = tr("There are problems: %1Do you really want to export this list?").arg(s);

        if (MessageBox::question(nullptr, { }, s) != MessageBox::Yes)
            return;
    }

    static QLocale c = QLocale::c();
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const BrickLink::InvItem *item : itemlist) {
        if (item->isIncomplete()
                || (item->status() == BrickLink::Status::Exclude)) {
            continue;
        }
        auto *base = doc->differenceBaseItem(item);
        if (!base)
            continue;

        // we don't care about reserved and status, so we have to mask it
        auto baseItem = *base;
        baseItem.setReserved(item->reserved());
        baseItem.setStatus(item->status());

        if (baseItem == *item)
            continue;

        xml.createElement();
        xml.createText("LOTID", QString::number(item->lotId()));
        int qdiff = item->quantity() - base->quantity();
        if (qdiff && (item->quantity() > 0))
            xml.createText("QTY", QString::number(qdiff).prepend(qdiff > 0 ? "+" : ""));
        else if (qdiff && (item->quantity() <= 0))
            xml.createEmpty("DELETE");

        if (!qFuzzyCompare(base->price(), item->price()))
            xml.createText("PRICE", QString::number(item->price(), {}, 3));
        if (!qFuzzyCompare(base->cost(), item->cost()))
            xml.createText("MYCOST", QString::number(item->cost(), {}, 3));
        if (base->condition() != item->condition())
            xml.createText("CONDITION", (item->condition() == BrickLink::Condition::New) ? u"N" : u"U");
        if (base->bulkQuantity() != item->bulkQuantity())
            xml.createText("BULK", QString::number(item->bulkQuantity()));
        if (base->sale() != item->sale())
            xml.createText("SALE", QString::number(item->sale()));
        if (base->comments() != item->comments())
            xml.createText("DESCRIPTION", item->comments());
        if (base->remarks() != item->remarks())
            xml.createText("REMARKS", item->remarks());
        if (base->retain() != item->retain())
            xml.createText("RETAIN", item->retain() ? u"Y" : u"N");

        if (base->tierQuantity(0) != item->tierQuantity(0))
            xml.createText("TQ1", QString::number(item->tierQuantity(0)));
        if (!qFuzzyCompare(base->tierPrice(0), item->tierPrice(0)))
            xml.createText("TP1", c.toCurrencyString(item->tierPrice(0), {}, 3));
        if (base->tierQuantity(1) != item->tierQuantity(1))
            xml.createText("TQ2", QString::number(item->tierQuantity(1)));
        if (!qFuzzyCompare(base->tierPrice(1), item->tierPrice(1)))
            xml.createText("TP2", c.toCurrencyString(item->tierPrice(1), {}, 3));
        if (base->tierQuantity(2) != item->tierQuantity(2))
            xml.createText("TQ3", QString::number(item->tierQuantity(2)));
        if (!qFuzzyCompare(base->tierPrice(2), item->tierPrice(2)))
            xml.createText("TP3", c.toCurrencyString(item->tierPrice(2), {}, 3));

        if (base->subCondition() != item->subCondition()) {
            const char16_t *st = nullptr;
            switch (item->subCondition()) {
            case BrickLink::SubCondition::Incomplete: st = u"I"; break;
            case BrickLink::SubCondition::Complete  : st = u"C"; break;
            case BrickLink::SubCondition::Sealed    : st = u"S"; break;
            default                                 : break;
            }
            if (st)
                xml.createText("SUBCONDITION", st);
        }
        if (base->stockroom() != item->stockroom()) {
            const char16_t *st = nullptr;
            switch (item->stockroom()) {
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

    QGuiApplication::clipboard()->setText(xml.toString(), QClipboard::Clipboard);

    BrickLink::core()->openUrl(BrickLink::URL_InventoryUpdate);
}

QString DocumentIO::toBrickLinkXML(const BrickLink::InvItemList &itemlist)
{
    static QLocale c = QLocale::c();
    XmlHelpers::CreateXML xml("INVENTORY", "ITEM");

    for (const BrickLink::InvItem *item : itemlist) {
        if (item->isIncomplete()
                || (item->status() == BrickLink::Status::Exclude)) {
            continue;
        }

        xml.createElement();
        xml.createText("ITEMID", item->item()->id());
        xml.createText("ITEMTYPE", QString(item->itemType()->id()));
        xml.createText("COLOR", QString::number(item->color()->id()));
        xml.createText("CATEGORY", QString::number(item->category()->id()));
        xml.createText("QTY", QString::number(item->quantity()));
        xml.createText("PRICE", c.toCurrencyString(item->price(), {}, 3));
        xml.createText("CONDITION", (item->condition() == BrickLink::Condition::New) ? u"N" : u"U");

        if (item->bulkQuantity() != 1)   xml.createText("BULK", QString::number(item->bulkQuantity()));
        if (item->sale())                xml.createText("SALE", QString::number(item->sale()));
        if (!item->comments().isEmpty()) xml.createText("DESCRIPTION", item->comments());
        if (!item->remarks().isEmpty())  xml.createText("REMARKS", item->remarks());
        if (item->retain())              xml.createText("RETAIN", u"Y");
        if (!item->reserved().isEmpty()) xml.createText("BUYERUSERNAME", item->reserved());
        if (!qFuzzyIsNull(item->cost())) xml.createText("MYCOST", c.toCurrencyString(item->cost(), {}, 3));
        if (item->hasCustomWeight())     xml.createText("MYWEIGHT", QString::number(item->weight(), 'f', 4));

        if (item->tierQuantity(0)) {
            xml.createText("TQ1", QString::number(item->tierQuantity(0)));
            xml.createText("TP1", c.toCurrencyString(item->tierPrice(0), {}, 3));
            xml.createText("TQ2", QString::number(item->tierQuantity(1)));
            xml.createText("TP2", c.toCurrencyString(item->tierPrice(1), {}, 3));
            xml.createText("TQ3", QString::number(item->tierQuantity(2)));
            xml.createText("TP3", c.toCurrencyString(item->tierPrice(2), {}, 3));
        }

        if (item->subCondition() != BrickLink::SubCondition::None) {
            const char16_t *st = nullptr;
            switch (item->subCondition()) {
            case BrickLink::SubCondition::Incomplete: st = u"I"; break;
            case BrickLink::SubCondition::Complete  : st = u"C"; break;
            case BrickLink::SubCondition::Sealed    : st = u"S"; break;
            default                                 : break;
            }
            if (st)
                xml.createText("SUBCONDITION", st);
        }
        if (item->stockroom() != BrickLink::Stockroom::None) {
            const char16_t *st = nullptr;
            switch (item->stockroom()) {
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

QPair<BrickLink::InvItemList, QString> DocumentIO::fromBrickLinkXML(const QByteArray &xml)
{
    BrickLink::InvItemList items;
    QString currencyCode;
    int incompleteCount = 0;

    auto buf = new QBuffer(const_cast<QByteArray *>(&xml), nullptr);
    buf->open(QIODevice::ReadOnly);

    XmlHelpers::ParseXML p(buf, "INVENTORY", "ITEM");
    p.parse([&p, &items, &currencyCode, &incompleteCount](QDomElement e) {
        const QString itemId = p.elementText(e, "ITEMID");
        char itemTypeId = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));
        uint colorId = p.elementText(e, "COLOR", "0").toUInt();
        uint categoryId = p.elementText(e, "CATEGORY", "0").toUInt();
        int qty = p.elementText(e, "MINQTY", "-1").toInt();
        if (qty < 0)
            qty = p.elementText(e, "QTY", "0").toInt();
        double price = p.elementText(e, "MAXPRICE", "-1").toDouble();
        if (price < 0)
            price = p.elementText(e, "PRICE", "0").toDouble();
        auto cond = p.elementText(e, "CONDITION", "N") == qL1S("N") ? BrickLink::Condition::New
                                                                             : BrickLink::Condition::Used;
        int bulk = p.elementText(e, "BULK", "1").toInt();
        int sale = p.elementText(e, "SALE", "0").toInt();
        QString comments = p.elementText(e, "DESCRIPTION", "");
        QString remarks = p.elementText(e, "REMARKS", "");
        bool retain = (p.elementText(e, "RETAIN", "") == qL1S("Y"));
        QString reserved = p.elementText(e, "BUYERUSERNAME", "");
        double cost = p.elementText(e, "MYCOST", "0").toDouble();
        double weight = p.elementText(e, "MYWEIGHT", "0").toDouble();
        if (qFuzzyIsNull(weight))
            weight = p.elementText(e, "ITEMWEIGHT", "0").toDouble();
        uint lotId = p.elementText(e, "LOTID", "").toUInt();
        QString subCondStr = p.elementText(e, "SUBCONDITION", "");
        auto subCond = (subCondStr == qL1S("I") ? BrickLink::SubCondition::Incomplete :
                        subCondStr == qL1S("C") ? BrickLink::SubCondition::Complete :
                        subCondStr == qL1S("S") ? BrickLink::SubCondition::Sealed
                                                         : BrickLink::SubCondition::None);
        auto stockroom = p.elementText(e, "STOCKROOM", "")
                == qL1S("Y") ? BrickLink::Stockroom::A : BrickLink::Stockroom::None;
        if (stockroom != BrickLink::Stockroom::None) {
            QString stockroomId = p.elementText(e, "STOCKROOMID", "");
            stockroom = (stockroomId == qL1S("A") ? BrickLink::Stockroom::A :
                         stockroomId == qL1S("B") ? BrickLink::Stockroom::B :
                         stockroomId == qL1S("C") ? BrickLink::Stockroom::C
                                                           : BrickLink::Stockroom::None);
        }
        QString ccode = p.elementText(e, "BASECURRENCYCODE", "");
        if (!ccode.isEmpty()) {
            if (currencyCode.isEmpty())
                currencyCode = ccode;
            else if (currencyCode != ccode)
                throw Exception("Multiple currencies in one XML file are not supported.");
        }

        const BrickLink::Item *item = BrickLink::core()->item(itemTypeId, itemId);
        const BrickLink::Color *color = BrickLink::core()->color(colorId);

        auto *ii = new BrickLink::InvItem(color, item);

        if (!item || !color) {
            auto *inc = new BrickLink::InvItem::Incomplete;
            if (!item) {
                inc->m_item_id = itemId;
                inc->m_itemtype_id = itemTypeId;
                if (auto cat = BrickLink::core()->category(categoryId))
                    inc->m_category_name = cat->name();
            }
            if (!color)
                inc->m_color_name = QString::number(colorId);
            ii->setIncomplete(inc);

            if (!BrickLink::core()->applyChangeLogToItem(ii))
                ++incompleteCount;
        }

        ii->setQuantity(qty);
        ii->setPrice(price);
        ii->setCondition(cond);
        ii->setBulkQuantity(bulk);
        ii->setSale(sale);
        ii->setComments(comments);
        ii->setRemarks(remarks);
        ii->setRetain(retain);
        ii->setReserved(reserved);
        ii->setCost(cost);
        ii->setWeight(weight);
        ii->setLotId(lotId);
        ii->setSubCondition(subCond);
        ii->setStockroom(stockroom);

        items << ii;
    });

    if (currencyCode.isEmpty())
        currencyCode = qL1S("USD");

    return qMakePair(items, currencyCode);
}

void DocumentIO::exportBrickLinkXML(const BrickLink::InvItemList &itemlist)
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";

    QString s = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Export File"), Config::inst()->documentDir(), filters.join(";;"));

    if (s.isNull())
        return;
    if (s.right(4) != qL1S(".xml"))
        s += qL1S(".xml");

    const QByteArray xml = toBrickLinkXML(itemlist).toUtf8();

    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        if (f.write(xml.data(), xml.size()) != qint64(xml.size())) {
            MessageBox::warning(nullptr, { }, tr("Failed to save data to file %1.").arg(CMB_BOLD(s)));
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Failed to open file %1 for writing.").arg(CMB_BOLD(s)));
    }
}

void DocumentIO::exportBrickLinkXMLClipboard(const BrickLink::InvItemList &itemlist)
{
    const QString xml = toBrickLinkXML(itemlist);

    QGuiApplication::clipboard()->setText(xml, QClipboard::Clipboard);

    BrickLink::core()->openUrl(BrickLink::URL_InventoryUpload);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


DocumentIO::ParseItemListResult DocumentIO::parseBsxInventory(const QDomDocument &domDoc)
{
    ParseItemListResult result;
    bool migrateDiffMode = false;

    QDomElement domRoot = domDoc.documentElement();

    static const QVector<QString> knownDocTypes { qL1S("BrickStoreXML"), qL1S("BrickStockXML") };

    if (!knownDocTypes.contains(domDoc.doctype().name())
            || !knownDocTypes.contains(domRoot.tagName())) {
        result.xmlParseError = true;
        return result;
    }

    QDomElement domInventory;
    QDomElement domGuiState;

    for (QDomNode n = domRoot.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (!n.isElement())
            continue;

        if (n.nodeName() == qL1S("Inventory"))
            domInventory = n.toElement();
        else if (n.nodeName() == qL1S("GuiState"))
            domGuiState = n.toElement();
    }

    if (domInventory.isNull())
        return result;

    // a bit of a hack for a clean switch from the old to the new Difference mode
    if (!domGuiState.isNull()) {
        auto list = domGuiState.elementsByTagName(qL1S("DifferenceMode"));
        if (list.size() == 1) {
            domGuiState.removeChild(list.at(0));

            migrateDiffMode = true;
        }
    }

    result.currencyCode = domInventory.attribute(qL1S("Currency"));

    // no throw beyond this point - otherwise we'd be leaking the domGuiState node clone
    result.domGuiState = domGuiState.cloneNode(true).toElement();

    BrickLink::InvItem *ii = nullptr;
    QString itemid, itemname;
    QString itemtypeid, itemtypename;
    QString colorid, colorname;
    QString categoryid, categoryname;
    QVariant legacyOrigPrice, legacyOrigQty;

    const QHash<QStringView, std::function<void(const QString &value)>> tagHash {
    { u"ItemID",       [&itemid](auto v) { itemid = v; } },
    { u"ColorID",      [&colorid](auto v) { colorid = v; } },
    { u"CategoryID",   [&categoryid](auto v) { categoryid = v; } },
    { u"ItemTypeID",   [&itemtypeid](auto v) { itemtypeid = v; } },
    { u"ItemName",     [&itemname](auto v) { itemname = v; } },
    { u"ColorName",    [&colorname](auto v) { colorname = v; } },
    { u"CategoryName", [&categoryname](auto v) { categoryname = v; } },
    { u"ItemTypeName", [&itemtypename](auto v) { itemtypename = v; } },
    { u"Price",        [&ii](auto v) { ii->setPrice(v.toDouble()); } },
    { u"Bulk",         [&ii](auto v) { ii->setBulkQuantity(v.toInt()); } },
    { u"Qty",          [&ii](auto v) { ii->setQuantity(v.toInt()); } },
    { u"Sale",         [&ii](auto v) { ii->setSale(v.toInt()); } },
    { u"Comments",     [&ii](auto v) { ii->setComments(v); } },
    { u"Remarks",      [&ii](auto v) { ii->setRemarks(v); } },
    { u"TQ1",          [&ii](auto v) { ii->setTierQuantity(0, v.toInt()); } },
    { u"TQ2",          [&ii](auto v) { ii->setTierQuantity(1, v.toInt()); } },
    { u"TQ3",          [&ii](auto v) { ii->setTierQuantity(2, v.toInt()); } },
    { u"TP1",          [&ii](auto v) { ii->setTierPrice(0, v.toDouble()); } },
    { u"TP2",          [&ii](auto v) { ii->setTierPrice(1, v.toDouble()); } },
    { u"TP3",          [&ii](auto v) { ii->setTierPrice(2, v.toDouble()); } },
    { u"LotID",        [&ii](auto v) { ii->setLotId(v.toUInt()); } },
    { u"Retain",       [&ii](auto)   { ii->setRetain(true); } },
    { u"Reserved",     [&ii](auto v) { ii->setReserved(v); } },
    { u"TotalWeight",  [&ii](auto v) { ii->setTotalWeight(v.toDouble()); } },
    { u"Cost",         [&ii](auto v) { ii->setCost(v.toDouble()); } },
    { u"Condition",    [&ii](auto v) {
        ii->setCondition(v == qL1S("N") ? BrickLink::Condition::New
                                        : BrickLink::Condition::Used); } },
    // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
    { u"SubCondition", [&ii](auto v) {
        ii->setSubCondition(v == qL1S("C") ? BrickLink::SubCondition::Complete :
                            v == qL1S("I") ? BrickLink::SubCondition::Incomplete :
                            v == qL1S("M") ? BrickLink::SubCondition::Sealed
                                           : BrickLink::SubCondition::None); } },
    { u"Status",       [&ii](auto v) {
        ii->setStatus(v == qL1S("X") ? BrickLink::Status::Exclude :
                      v == qL1S("I") ? BrickLink::Status::Include :
                      v == qL1S("E") ? BrickLink::Status::Extra :
                      v == qL1S("?") ? BrickLink::Status::Unknown
                                     : BrickLink::Status::Include); } },
    { u"Stockroom",    [&ii](auto v) {
        ii->setStockroom(v == qL1S("A") || v.isEmpty() ? BrickLink::Stockroom::A :
                         v == qL1S("B") ? BrickLink::Stockroom::B :
                         v == qL1S("C") ? BrickLink::Stockroom::C
                                        : BrickLink::Stockroom::None); } },
    { u"OrigPrice",    [&migrateDiffMode, &legacyOrigPrice](auto v) {
        if (migrateDiffMode)
            legacyOrigPrice.setValue(v.toDouble());
    } },
    { u"OrigQty",      [&migrateDiffMode, &legacyOrigQty](auto v) {
        if (migrateDiffMode)
            legacyOrigQty.setValue(v.toInt());
    } },
    { u"X-DifferenceModeBase", [&migrateDiffMode, &result, &ii](auto v) {
        if (!migrateDiffMode) {
            const auto ba = QByteArray::fromBase64(v.toLatin1());
            QDataStream ds(ba);
            if (auto base = BrickLink::InvItem::restore(ds)) {
                result.differenceModeBase.insert(ii, *base);
                delete base;
            }
        }
    } },
    };

    for (QDomNode in = domInventory.firstChild(); !in.isNull(); in = in.nextSibling()) {
        if (in.nodeName() != qL1S("Item"))
            continue;

        ii = new BrickLink::InvItem();

        itemid.clear();
        itemname.clear();
        itemtypeid.clear();
        itemtypename.clear();
        colorid.clear();
        colorname.clear();
        categoryid.clear();
        categoryname.clear();

        for (QDomNode n = in.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;

            QString tag = n.toElement().tagName();
            QString val = n.toElement().text();

            auto it = tagHash.find(tag);
            if (it != tagHash.end())
                (*it)(val);
        }

        bool ok = true;

        if (!colorid.isEmpty() && !itemid.isEmpty() && !itemtypeid.isEmpty()) {
            ii->setItem(BrickLink::core()->item(itemtypeid[0].toLatin1(), itemid.toLatin1()));
            if (!ii->item())
                ok = false;

            ii->setColor(BrickLink::core()->color(colorid.toUInt()));
            if (!ii->color())
                ok = false;
        } else {
            ok = false;
        }

        if (!ok) {
            qWarning() << "failed: insufficient data (item=" << itemid << ", itemtype=" << itemtypeid[0] << ", category=" << categoryid << ", color=" << colorid << ")";

            auto *inc = new BrickLink::InvItem::Incomplete;
            if (!ii->item()) {
                inc->m_item_id = itemid;
                inc->m_item_name = itemname;
                inc->m_itemtype_id = itemtypeid;
                inc->m_itemtype_name = itemtypename;
                inc->m_category_name = categoryname.isEmpty() ? categoryid : categoryname;
            }
            if (!ii->color())
                inc->m_color_name = colorname.isEmpty() ? colorid : colorname;

            ii->setIncomplete(inc);

            if (!BrickLink::core()->applyChangeLogToItem(ii))
                result.invalidItemCount++;
        }

        if (migrateDiffMode) {
            // create an update base item from the OrigPrice and OrigQty values
            auto base = *ii;
            if (!legacyOrigPrice.isNull())
                base.setPrice(legacyOrigPrice.toDouble());
            if (!legacyOrigQty.isNull())
                base.setQuantity(legacyOrigQty.toInt());
            result.differenceModeBase.insert(ii, base);

        } else if (!result.differenceModeBase.contains(ii)) {
            // no differences saved, so start out with none
            result.differenceModeBase.insert(ii, *ii);
        }

        result.items.append(ii);
    }

    return result;
}


QDomDocument DocumentIO::createBsxInventory(const Document *doc)
{
    if (!doc)
        return { };

    QDomDocument domDoc(qL1S("BrickStoreXML"));
    domDoc.appendChild(domDoc.createProcessingInstruction(qL1S("xml"), qL1S(R"(version="1.0" encoding="UTF-8")")));
    QDomElement domRoot = domDoc.createElement(qL1S("BrickStoreXML"));
    domDoc.appendChild(domRoot);

    QDomElement domInventory = domDoc.createElement(qL1S("Inventory"));
    domInventory.setAttribute(qL1S("Currency"), doc->currencyCode());
    domRoot.appendChild(domInventory);

    static QLocale c = QLocale::c();

    const auto items = doc->items();
    for (const BrickLink::InvItem *ii : items) {
        if (ii->isIncomplete())
            continue;

        QDomElement domItem = domDoc.createElement(qL1S("Item"));
        domInventory.appendChild(domItem);

        auto create = [&domItem, &domDoc](QStringView tagName, QStringView value) {
            return domItem.appendChild(domDoc.createElement(tagName.toString())
                                       .appendChild(domDoc.createTextNode(value.toString()))
                                       .parentNode()).toElement();
        };
        auto createEmpty = [&domItem, &domDoc](QStringView tagName) {
            return domItem.appendChild(domDoc.createElement(tagName.toString())).toElement();
        };

        create(u"ItemID", ii->item()->id());
        create(u"ItemTypeID", QString(ii->itemType()->id()));
        create(u"ColorID", QString::number(ii->color()->id()));

        // this extra information is useful, if the e.g.the color- or item-id
        // are no longer available after a database update
        create(u"ItemName", ii->item()->name());
        create(u"ItemTypeName", ii->itemType()->name());
        create(u"ColorName", ii->color()->name());
        create(u"CategoryID", QString::number(ii->category()->id()));
        create(u"CategoryName", ii->category()->name());

        {
            const char16_t *st;
            switch (ii->status()) {
            default             :
            case BrickLink::Status::Unknown: st = u"?"; break;
            case BrickLink::Status::Extra  : st = u"E"; break;
            case BrickLink::Status::Exclude: st = u"X"; break;
            case BrickLink::Status::Include: st = u"I"; break;
            }
            create(u"Status", st);
        }

        create(u"Qty", QString::number(ii->quantity()));
        create(u"Price", c.toCurrencyString(ii->price(), { }, 3));
        create(u"Condition", (ii->condition() == BrickLink::Condition::New) ? u"N" : u"U");

        if (ii->subCondition() != BrickLink::SubCondition::None) {
            // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
            const char16_t *st;
            switch (ii->subCondition()) {
            case BrickLink::SubCondition::Incomplete: st = u"I"; break;
            case BrickLink::SubCondition::Complete  : st = u"C"; break;
            case BrickLink::SubCondition::Sealed    : st = u"M"; break;
            default                                 : st = u"?"; break;
            }
            create(u"SubCondition", st);
        }

        if (ii->bulkQuantity() != 1)
            create(u"Bulk", QString::number(ii->bulkQuantity()));
        if (ii->sale())
            create(u"Sale", QString::number(ii->sale()));
        if (!ii->comments().isEmpty())
            create(u"Comments", ii->comments());
        if (!ii->remarks().isEmpty())
            create(u"Remarks", ii->remarks());
        if (ii->retain())
            createEmpty(u"Retain");
        if (ii->stockroom() != BrickLink::Stockroom::None) {
            const char16_t *st;
            switch (ii->stockroom()) {
            case BrickLink::Stockroom::A: st = u"A"; break;
            case BrickLink::Stockroom::B: st = u"B"; break;
            case BrickLink::Stockroom::C: st = u"C"; break;
            default                     : st = u""; break;
            }
            create(u"Stockroom", st);
        }
        if (!ii->reserved().isEmpty())
            create(u"Reserved", ii->reserved());
        if (ii->lotId())
            create(u"LotID", QString::number(ii->lotId()));

        if (ii->tierQuantity(0)) {
            create(u"TQ1", QString::number(ii->tierQuantity(0)));
            create(u"TP1", c.toCurrencyString(ii->tierPrice(0), { }, 3));
            create(u"TQ2", QString::number(ii->tierQuantity(1)));
            create(u"TP2", c.toCurrencyString(ii->tierPrice(1), { }, 3));
            create(u"TQ3", QString::number(ii->tierQuantity(2)));
            create(u"TP3", c.toCurrencyString(ii->tierPrice(2), { }, 3));
        }

        if (ii->hasCustomWeight())
            create(u"TotalWeight", QString::number(ii->totalWeight(), 'f', 4));
        if (!qFuzzyIsNull(ii->cost()))
            create(u"Cost", c.toCurrencyString(ii->cost(), { }, 3));

        auto base = doc->differenceBaseItem(ii);
        if (base && (*base != *ii)) {
            //TODO as soon as the new difference mode has matured and is deemed stable, this
            // should be changed into writing out minimal XML

            QByteArray ba;
            QDataStream ds(&ba, QIODevice::WriteOnly);
            base->save(ds);
            create(u"X-DifferenceModeBase", QString::fromLatin1(ba.toBase64()));
        }
    }

    return domDoc;
}
