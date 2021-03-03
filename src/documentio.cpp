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
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "framework.h"
#include "progressdialog.h"
#include "messagebox.h"
#include "exception.h"
#include "xmlhelpers.h"
#include "config.h"
#include "utility.h"
#include "document.h"
#include "window.h"
#include "stopwatch.h"
#include "documentio.h"

#include "unzip.h"


QString DocumentIO::s_lastDirectory { };

QString DocumentIO::lastDirectory()
{
    return s_lastDirectory.isEmpty() ? Config::inst()->documentDir() : s_lastDirectory;
}

void DocumentIO::setLastDirectory(const QString &dir)
{
    if (!dir.isEmpty())
        s_lastDirectory = dir;
}


Document *DocumentIO::create()
{
    auto *doc = new Document();
    doc->setTitle(tr("Untitled"));
    return doc;
}

Window *DocumentIO::open()
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";
    filters << tr("All Files") + "(*)";

    auto fn = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Open File"), lastDirectory(),
                                           filters.join(";;"));
    if (fn.isEmpty())
        return nullptr;
    setLastDirectory(QFileInfo(fn).absolutePath());
    return open(fn);
}

Window *DocumentIO::open(const QString &s)
{
    if (s.isEmpty())
        return nullptr;

    QString fn = QFileInfo(s).absoluteFilePath();

    const auto all = Window::allWindows();
    for (Window *win : all) {
        if (QFileInfo(win->document()->fileName()).absoluteFilePath() == fn)
            return win;
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

            DocumentIO::BsxContents bsx;
            bsx.items = items;

            auto *doc = new Document(bsx); // Document own the items now
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
        auto *doc = new Document(result); // Document owns the items now
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
        } else if ((data->left(30).contains("<html") || data->left(30).contains("<HTML"))
                   && data->contains("there was a problem during login")) {
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
            auto *doc = new Document(result); // Document owns the items now
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
    BsxContents bsx;
    bsx.items = itemlist;
    bsx.currencyCode = cart->currencyCode();

    auto *doc = new Document(bsx); // Document owns the items now
    doc->setTitle(tr("Cart in store %1").arg(cart->storeName()));
    return doc;
}

Document *DocumentIO::importBrickLinkXML()
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";
    filters << tr("All Files") + "(*)";

    QString fn = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), lastDirectory(),
                                             filters.join(";;"));
    if (fn.isEmpty())
        return nullptr;
    setLastDirectory(QFileInfo(fn).absolutePath());

    QFile f(fn);
    if (f.open(QIODevice::ReadOnly)) {
        try {
            auto result = fromBrickLinkXML(f.readAll());
            auto *doc = new Document(result); // Document owns the items now
            doc->setTitle(tr("Import of %1").arg(QFileInfo(fn).fileName()));
            return doc;

        } catch (const Exception &e) {
            MessageBox::warning(nullptr, { }, tr("Could not parse the XML data.") % u"<br><br>" % e.error());
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(fn)));
    }
    return nullptr;
}

Window *DocumentIO::loadFrom(const QString &name)
{
    QFile f(name);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(name)));
        return nullptr;
    }

    try {
        BsxContents result = DocumentIO::parseBsxInventory(&f); // we own the items now

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

        if (result.currencyCode.isEmpty()) // flag as legacy currency
            result.currencyCode = qL1S("$$$");

        // Document owns the items now
        auto doc = new Document(result);
        doc->setFileName(name);
        Config::inst()->addToRecentFiles(name);

        return new Window(doc, result.guiColumnLayout, result.guiSortFilterState);

    } catch (const Exception &e) {
        MessageBox::warning(nullptr, { }, tr("This XML document is not a valid BrickStoreXML file.")
                            % u"<br><br>" % e.error());
        return nullptr;
    }
}

Document *DocumentIO::importLDrawModel()
{
    QStringList filters;
    filters << tr("All Models") + " (*.dat *.ldr *.mpd *.io)";
    filters << tr("LDraw Models") + " (*.dat *.ldr *.mpd)";
    filters << tr("BrickLink Studio Models") + " (*.io)";
    filters << tr("All Files") + " (*)";

    QString fn = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), lastDirectory(),
                                             filters.join(";;"));
    if (fn.isEmpty())
        return nullptr;
    setLastDirectory(QFileInfo(fn).absolutePath());

    QScopedPointer<QFile> f;

    if (fn.endsWith(QLatin1String(".io"))) {
        stopwatch("unpack/decrypt studio zip");

        // this is a zip file - unpack the encrypted model.ldr (pw: soho0909)

        f.reset(new QTemporaryFile());
        QString errorMsg;

        if (f->open(QIODevice::ReadWrite)) {
            QByteArray su8 = fn.toUtf8();
            if (auto zip = unzOpen64(su8.constData())) {
                if (unzLocateFile(zip, "model.ldr", 2 /*case insensitive*/) == UNZ_OK) {
                    if (unzOpenCurrentFilePassword(zip, "soho0909") == UNZ_OK) {
                        QByteArray block;
                        block.resize(1024*1024);
                        int bytesRead;
                        do {
                            bytesRead = unzReadCurrentFile(zip, block.data(), block.size());
                            if (bytesRead > 0)
                                f->write(block.constData(), bytesRead);
                        } while (bytesRead > 0);
                        if (bytesRead < 0)
                            errorMsg = tr("Could not extract model.ldr from the Studio ZIP file.");
                        unzCloseCurrentFile(zip);
                    } else {
                        errorMsg = tr("Could not decrypt the model.ldr within the Studio ZIP file.");
                    }
                } else {
                    errorMsg = tr("Could not locate the model.ldr file within the Studio ZIP file.");
                }
                unzClose(zip);
            } else {
                errorMsg = tr("Could not open the Studio ZIP file");
            }
            f->close();
        }
        if (!errorMsg.isEmpty()) {
            MessageBox::warning(nullptr, { }, tr("Could not parse the Studio model:")
                                % u"<br><br>" % errorMsg);
            return nullptr;
        }
    } else {
        f.reset(new QFile(fn));
    }

    if (!f->open(QIODevice::ReadOnly)) {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(fn)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    int invalid_items = 0;
    BrickLink::InvItemList items; // we own the items

    bool b = DocumentIO::parseLDrawModel(f.get(), items, &invalid_items);
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
            DocumentIO::BsxContents bsx;
            bsx.items = items;
            doc = new Document(bsx); // Document owns the items now
            items.clear();
            doc->setTitle(tr("Import of %1").arg(QFileInfo(fn).fileName()));
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not parse the LDraw model in file %1.").arg(CMB_BOLD(fn)));
    }

    qDeleteAll(items);
    return doc;
}

bool DocumentIO::parseLDrawModel(QFile *f, BrickLink::InvItemList &items, int *invalid_items)
{
    QHash<uint, BrickLink::InvItem *> mergehash;
    QStringList recursion_detection;

    stopwatch("parse ldraw model");

    return parseLDrawModelInternal(f, QString(), items, invalid_items, mergehash,
                                   recursion_detection);
}

bool DocumentIO::parseLDrawModelInternal(QFile *f, const QString &model_name, BrickLink::InvItemList &items,
                                         int *invalid_items, QHash<uint, BrickLink::InvItem *> &mergehash,
                                         QStringList &recursion_detection)
{
    if (recursion_detection.contains(model_name))
        return false;
    recursion_detection.append(model_name);

    stopwatch("parse sub-dat");

    QStringList searchpath;
    int invalid = 0;
    int linecount = 0;

    bool is_mpd = false;
    int current_mpd_index = -1;
    QString current_mpd_model;
    bool is_mpd_model_found = false;


    searchpath.append(QFileInfo(*f).dir().absolutePath());
    if (!BrickLink::core()->ldrawDataPath().isEmpty()) {
        searchpath.append(BrickLink::core()->ldrawDataPath() + qL1S("/models"));
    }

    if (f->isOpen()) {
        QTextStream in(f);
        QString line;

        while (!(line = in.readLine()).isNull()) {
            linecount++;

            if (line.isEmpty())
                continue;

            if (line.at(0) == QLatin1Char('0')) {
                const auto split = line.splitRef(QLatin1Char(' '), Qt::SkipEmptyParts);

                if ((split.count() >= 2) && (split.at(1) == qL1S("FILE"))) {
                    is_mpd = true;
                    current_mpd_model = line.mid(split.at(2).position()).toLower();
                    current_mpd_index++;

                    if (is_mpd_model_found)
                        break;

                    if ((current_mpd_model == model_name.toLower()) || (model_name.isEmpty() && (current_mpd_index == 0)))
                        is_mpd_model_found = true;

                    if (f->isSequential())
                        return false; // we need to seek!
                }

            } else if (line.at(0) == QLatin1Char('1')) {
                if (is_mpd && !is_mpd_model_found)
                    continue;

                const auto split = line.splitRef(QLatin1Char(' '), Qt::SkipEmptyParts);

                if (split.count() >= 15) {
                    int colid = split.at(1).toInt();
                    QString partname = line.mid(split.at(14).position()).toLower();

                    QString partid = partname;
                    partid.truncate(partid.lastIndexOf(QLatin1Char('.')));

                    const BrickLink::Color *colp = BrickLink::core()->colorFromLDrawId(colid);
                    const BrickLink::Item *itemp = BrickLink::core()->item('P', partid);

                    if (!itemp) {
                        bool got_subfile = false;

                        if (is_mpd) {
                            int sub_invalid_items = 0;

                            qint64 oldpos = f->pos();
                            f->seek(0);

                            got_subfile = parseLDrawModelInternal(f, partname, items, &sub_invalid_items, mergehash, recursion_detection);

                            invalid += sub_invalid_items;
                            f->seek(oldpos);
                        }

                        if (!got_subfile) {
                            for (const auto &path : qAsConst(searchpath)) {
                                QFile subf(path % u'/' % partname);

                                if (subf.open(QIODevice::ReadOnly)) {
                                    int sub_invalid_items = 0;

                                    (void) parseLDrawModelInternal(&subf, partname, items, &sub_invalid_items, mergehash, recursion_detection);

                                    invalid += sub_invalid_items;
                                    got_subfile = true;
                                    break;
                                }
                            }
                        }
                        if (got_subfile)
                            continue;
                    }

                    uint key = qHash(partid) ^ uint(colid);
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


bool DocumentIO::save(Window *win)
{
    if (win->document()->fileName().isEmpty())
        return saveAs(win);
    else if (win->document()->isModified())
        return saveTo(win, win->document()->fileName());
    return false;
}


bool DocumentIO::saveAs(Window *win)
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";

    QString fn = win->document()->fileName();
    if (fn.isEmpty()) {
        fn = lastDirectory();

        if (!win->document()->title().isEmpty()) {
            QString t = Utility::sanitizeFileName(win->document()->title());
            fn = fn % u'/' % t;
        }
    }
    if (fn.right(4) == ".xml")
        fn.truncate(fn.length() - 4);

    fn = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Save File as"), fn, filters.join(";;"));

    if (fn.isEmpty())
        return false;
    setLastDirectory(QFileInfo(fn).absolutePath());

    if (fn.right(4) != ".bsx")
        fn += ".bsx";

    return saveTo(win, fn);
}


bool DocumentIO::saveTo(Window *win, const QString &s)
{
    Document *doc = win->document();

    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        BsxContents bsx;
        bsx.items = doc->items();
        bsx.currencyCode = doc->currencyCode();
        bsx.differenceModeBase = doc->differenceBase();
        bsx.guiSortFilterState = doc->saveSortFilterState();
        bsx.guiColumnLayout = win->currentColumnLayout();

        bool ok = createBsxInventory(&f, bsx);

        QApplication::restoreOverrideCursor();

        if (ok) {
            doc->unsetModified();
            doc->setFileName(s);

            Config::inst()->addToRecentFiles(s);
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

    if (Config::inst()->openBrowserOnExport())
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

        if (Config::inst()->openBrowserOnExport())
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

    if (Config::inst()->openBrowserOnExport())
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

DocumentIO::BsxContents DocumentIO::fromBrickLinkXML(const QByteArray &xml)
{
    BsxContents bsx;

    auto buf = new QBuffer(const_cast<QByteArray *>(&xml), nullptr);
    buf->open(QIODevice::ReadOnly);

    XmlHelpers::ParseXML p(buf, "INVENTORY", "ITEM");
    p.parse([&p, &bsx](QDomElement e) {
        const QString itemId = p.elementText(e, "ITEMID");
        char itemTypeId = XmlHelpers::firstCharInString(p.elementText(e, "ITEMTYPE"));
        uint colorId = p.elementText(e, "COLOR", "0").toUInt();
        uint categoryId = p.elementText(e, "CATEGORY", "0").toUInt();
        int qty = p.elementText(e, "MINQTY", "-1").toInt();
        if (qty < 0) {
            // The remove(',') stuff is a workaround for the broken Order XML generator: the QTY
            // field is generated with thousands-separators enabled (e.g. 1,752 instead of 1752)
            qty = p.elementText(e, "QTY", "0").remove(QChar(',')).toInt();
        }
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
            if (bsx.currencyCode.isEmpty())
                bsx.currencyCode = ccode;
            else if (bsx.currencyCode != ccode)
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
                ++bsx.invalidItemCount;
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

        bsx.items << ii;
    });

    if (bsx.currencyCode.isEmpty())
        bsx.currencyCode = qL1S("USD");

    return bsx;
}

void DocumentIO::exportBrickLinkXML(const BrickLink::InvItemList &itemlist)
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";

    QString fn = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Export File"), lastDirectory(),
                                              filters.join(";;"));
    if (fn.isEmpty())
        return;
    setLastDirectory(QFileInfo(fn).absolutePath());

    if (fn.right(4) != qL1S(".xml"))
        fn += qL1S(".xml");

    const QByteArray xml = toBrickLinkXML(itemlist).toUtf8();

    QFile f(fn);
    if (f.open(QIODevice::WriteOnly)) {
        if (f.write(xml.data(), xml.size()) != qint64(xml.size())) {
            MessageBox::warning(nullptr, { }, tr("Failed to save data to file %1.").arg(CMB_BOLD(fn)));
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Failed to open file %1 for writing.").arg(CMB_BOLD(fn)));
    }
}

void DocumentIO::exportBrickLinkXMLClipboard(const BrickLink::InvItemList &itemlist)
{
    const QString xml = toBrickLinkXML(itemlist);

    QGuiApplication::clipboard()->setText(xml, QClipboard::Clipboard);

    if (Config::inst()->openBrowserOnExport())
        BrickLink::core()->openUrl(BrickLink::URL_InventoryUpload);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////



bool DocumentIO::resolveIncomplete(BrickLink::InvItem *item)
{
    auto ic = item->isIncomplete();

    if (!ic->m_item_id.isEmpty() && !ic->m_itemtype_id.isEmpty()) {
        item->setItem(BrickLink::core()->item(ic->m_itemtype_id[0].toLatin1(),
                      ic->m_item_id.toLatin1()));
        if (item->item()) {
            ic->m_item_id.clear();
            ic->m_item_name.clear();
            ic->m_itemtype_id.clear();
            ic->m_itemtype_name.clear();
            ic->m_category_id.clear();
            ic->m_category_name.clear();
        }
    }

    if (!ic->m_color_id.isEmpty()) {
        item->setColor(BrickLink::core()->color(ic->m_color_id.toUInt()));
        if (item->color()) {
            ic->m_color_id.clear();
            ic->m_color_name.clear();
        }
    }

    if (item->item() && item->color()) {
        item->setIncomplete(nullptr);
        return true;

    } else {
        qWarning() << "failed: insufficient data (item=" << ic->m_item_id << ", itemtype="
           << ic->m_itemtype_id << ", color=" << ic->m_color_id << ")";

        return BrickLink::core()->applyChangeLogToItem(item);
    }
}

DocumentIO::BsxContents DocumentIO::parseBsxInventory(QIODevice *in)
{
    Q_ASSERT(in);
    QXmlStreamReader xml(in);

    try {
        BsxContents bsx;
        bool foundRoot = false;
        bool foundInventory = false;

        auto parseGuiState = [&]() {
            while (xml.readNextStartElement()) {
                const auto tag = xml.name();
                bool isColumnLayout = (tag == qL1S("ColumnLayout"));
                bool isSortFilter = (tag == qL1S("SortFilterState"));

                if (isColumnLayout || isSortFilter) {
                    bool compressed = (xml.attributes().value("Compressed").toInt() == 1);
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
            BrickLink::InvItem *item = nullptr;
            QVariant legacyOrigPrice, legacyOrigQty;
            bool hasBaseValues = false;
            QXmlStreamAttributes baseValues;

            const QHash<QStringView, std::function<void(Document::Item *, const QString &value)>> tagHash {
            { u"ItemID",       [](auto item, auto v) { item->isIncomplete()->m_item_id = v; } },
            { u"ColorID",      [](auto item, auto v) { item->isIncomplete()->m_color_id = v; } },
            { u"CategoryID",   [](auto item, auto v) { item->isIncomplete()->m_category_id = v; } },
            { u"ItemTypeID",   [](auto item, auto v) { item->isIncomplete()->m_itemtype_id = v; } },
            { u"ItemName",     [](auto item, auto v) { item->isIncomplete()->m_item_name = v; } },
            { u"ColorName",    [](auto item, auto v) { item->isIncomplete()->m_color_name = v; } },
            { u"CategoryName", [](auto item, auto v) { item->isIncomplete()->m_category_name = v; } },
            { u"ItemTypeName", [](auto item, auto v) { item->isIncomplete()->m_itemtype_name = v; } },
            { u"Price",        [](auto item, auto v) { item->setPrice(v.toDouble()); } },
            { u"Bulk",         [](auto item, auto v) { item->setBulkQuantity(v.toInt()); } },
            { u"Qty",          [](auto item, auto v) { item->setQuantity(v.toInt()); } },
            { u"Sale",         [](auto item, auto v) { item->setSale(v.toInt()); } },
            { u"Comments",     [](auto item, auto v) { item->setComments(v); } },
            { u"Remarks",      [](auto item, auto v) { item->setRemarks(v); } },
            { u"TQ1",          [](auto item, auto v) { item->setTierQuantity(0, v.toInt()); } },
            { u"TQ2",          [](auto item, auto v) { item->setTierQuantity(1, v.toInt()); } },
            { u"TQ3",          [](auto item, auto v) { item->setTierQuantity(2, v.toInt()); } },
            { u"TP1",          [](auto item, auto v) { item->setTierPrice(0, v.toDouble()); } },
            { u"TP2",          [](auto item, auto v) { item->setTierPrice(1, v.toDouble()); } },
            { u"TP3",          [](auto item, auto v) { item->setTierPrice(2, v.toDouble()); } },
            { u"LotID",        [](auto item, auto v) { item->setLotId(v.toUInt()); } },
            { u"Retain",       [](auto item, auto v) { item->setRetain(v.isEmpty() || (v == qL1S("Y"))); } },
            { u"Reserved",     [](auto item, auto v) { item->setReserved(v); } },
            { u"TotalWeight",  [](auto item, auto v) { item->setTotalWeight(v.toDouble()); } },
            { u"Cost",         [](auto item, auto v) { item->setCost(v.toDouble()); } },
            { u"Condition",    [](auto item, auto v) {
                item->setCondition(v == qL1S("N") ? BrickLink::Condition::New
                                                  : BrickLink::Condition::Used); } },
            { u"SubCondition", [](auto item, auto v) {
                // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
                item->setSubCondition(v == qL1S("C") ? BrickLink::SubCondition::Complete :
                                                       v == qL1S("I") ? BrickLink::SubCondition::Incomplete :
                                                                        v == qL1S("M") ? BrickLink::SubCondition::Sealed
                                                                                       : BrickLink::SubCondition::None); } },
            { u"Status",       [](auto item, auto v) {
                item->setStatus(v == qL1S("X") ? BrickLink::Status::Exclude :
                                                 v == qL1S("I") ? BrickLink::Status::Include :
                                                                  v == qL1S("E") ? BrickLink::Status::Extra
                                                                                 : BrickLink::Status::Include); } },
            { u"Stockroom",    [](auto item, auto v) {
                item->setStockroom(v == qL1S("A") || v.isEmpty() ? BrickLink::Stockroom::A :
                                                                   v == qL1S("B") ? BrickLink::Stockroom::B :
                                                                                    v == qL1S("C") ? BrickLink::Stockroom::C
                                                                                                   : BrickLink::Stockroom::None); } },
            { u"OrigPrice",    [&legacyOrigPrice](auto item, auto v) {
                Q_UNUSED(item)
                legacyOrigPrice.setValue(v.toDouble());
            } },
            { u"OrigQty",      [&legacyOrigQty](auto item, auto v) {
                Q_UNUSED(item)
                legacyOrigQty.setValue(v.toInt());
            } },
            { u"X-DifferenceModeBase", [&bsx](auto item, auto v) {
                //TODO: Remove in 2021.4.1
                const auto ba = QByteArray::fromBase64(v.toLatin1());
                QDataStream ds(ba);
                if (auto base = BrickLink::InvItem::restore(ds)) {
                    bsx.differenceModeBase.insert(item, *base);
                    delete base;
                }
            } } };

            while (xml.readNextStartElement()) {
                if (xml.name() != qL1S("Item"))
                    throw Exception("Expected Item element, but got: %1").arg(xml.name());

                item = new BrickLink::InvItem();
                item->setIncomplete(new BrickLink::InvItem::Incomplete);
                baseValues.clear();

                while (xml.readNextStartElement()) {
                    auto tag = xml.name();

                    if (tag != qL1S("DifferenceBaseValues")) {
                        auto it = tagHash.find(tag);
                        if (it != tagHash.end())
                            (*it)(item, xml.readElementText());
                        else
                            xml.skipCurrentElement();
                    } else {
                        hasBaseValues = true;
                        baseValues = xml.attributes();
                        xml.skipCurrentElement();
                    }
                }

                if (!resolveIncomplete(item))
                    bsx.invalidItemCount++;

                // convert the legacy OrigQty / OrigPrice fields
                if (!hasBaseValues && (legacyOrigPrice.isValid() || legacyOrigQty.isValid())) {
                    if (legacyOrigQty.isValid())
                        baseValues.append("Qty", QString::number(legacyOrigQty.toInt()));
                    if (!legacyOrigPrice.isNull())
                        baseValues.append("Price", QString::number(legacyOrigPrice.toDouble(), 'f', 3));
                }

                if (!bsx.differenceModeBase.contains(item)) { // remove condition in 2021.4.1
                    BrickLink::InvItem base = *item;
                    if (!baseValues.isEmpty()) {
                        base.setIncomplete(new BrickLink::InvItem::Incomplete);

                        for (int i = 0; i < baseValues.size(); ++i) {
                            auto attr = baseValues.at(i);
                            auto it = tagHash.find(attr.name());
                            if (it != tagHash.end()) {
                                (*it)(&base, attr.value().toString());
                            }
                        }
                        if (!resolveIncomplete(&base)) {
                            if (!base.item() && item->item())
                                base.setItem(item->item());
                            if (!base.color() && item->color())
                                base.setColor(item->color());
                            base.setIncomplete(nullptr);
                        }
                    }
                    bsx.differenceModeBase.insert(item, base);
                }

                bsx.items.append(item);
            }
        };


        // #################### XML PARSING STARTS HERE #####################


        // in an ideal world, BrickStock wouldn't have changed the root tag and everyone would add
        // valid DOCTYPE declarations ... sadly that's not the world we're living in
        static const QVector<QString> knownTypes { qL1S("BrickStoreXML"), qL1S("BrickStockXML") };

        while (true) {
            switch (xml.readNext()) {
            case QXmlStreamReader::DTD: {
                // DTDs are optional, but if there is a DTD, it has to be correct
                auto dtd = xml.text().toString().trimmed();

                if (!dtd.startsWith(qL1S("<!DOCTYPE "))
                        || !dtd.endsWith(qL1S(">"))
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
                    if (xml.name() == qL1S("Inventory")) {
                        foundInventory = true;
                        bsx.currencyCode = xml.attributes().value(qL1S("Currency")).toString();
                        parseInventory();
                    } else if ((xml.name() == qL1S("GuiState"))
                                && (xml.attributes().value(qL1S("Application")) == qL1S("BrickStore"))
                                && (xml.attributes().value(qL1S("Version")).toInt() == 2)) {
                        parseGuiState();
                    } else {
                        xml.skipCurrentElement();
                    }
                }
                break;

            case QXmlStreamReader::Invalid:
                throw Exception(xml.errorString());

            case QXmlStreamReader::EndDocument:
                if (!foundRoot || !foundInventory)
                    throw Exception("Not a valid BrickStoreXML file");
                return bsx;

            default:
                break;
            }
        }
    } catch (const Exception &e) {
        throw Exception("XML parse error at line %1, column %2: %3")
                .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(e.error());
    }
}


bool DocumentIO::createBsxInventory(QIODevice *out, const BsxContents &bsx)
{
    if (!out)
        return false;

    QXmlStreamWriter xml(out);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(1);
    xml.writeStartDocument();
    xml.writeDTD(qL1S("<!DOCTYPE BrickStoreXML>"));
    xml.writeStartElement(qL1S("BrickStoreXML"));
    xml.writeStartElement(qL1S("Inventory"));
    xml.writeAttribute(qL1S("Currency"), bsx.currencyCode);

    const BrickLink::InvItem *item;
    const BrickLink::InvItem *base;
    QXmlStreamAttributes baseValues;

    enum { Required = 0, Optional = 1, Constant = 2, WriteEmpty = 8 };

    auto create = [&](QStringView tagName, auto getter, auto stringify,
            int flags = Required, const QVariant &def = { }) {

        auto v = (item->*getter)();
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
    static auto asString   = [](const QString &s) { return s; };
    static auto asInt      = [](auto i)           { return QString::number(i); };
    static auto asCurrency = [](double d)         { return QString::number(d, 'f', 3); };


    for (const auto *ii : bsx.items) {
        item = ii;
        auto &baseRef = bsx.differenceModeBase.value(item);
        base = &baseRef;
        baseValues.clear();

        xml.writeStartElement(qL1S("Item"));

        // vvv Required Fields (part 1)
        create(u"ItemID",       &Document::Item::itemId,       asString);
        create(u"ItemTypeID",   &Document::Item::itemTypeId,   asString);
        create(u"ColorID",      &Document::Item::colorId,      asString);

        // vvv Redundancy Fields

        // this extra information is useful, if the e.g.the color- or item-id
        // are no longer available after a database update
        create(u"ItemName",     &Document::Item::itemName,     asString);
        create(u"ItemTypeName", &Document::Item::itemTypeName, asString);
        create(u"ColorName",    &Document::Item::colorName,    asString);
        create(u"CategoryID",   &Document::Item::categoryId,   asString);
        create(u"CategoryName", &Document::Item::categoryName, asString);

        // vvv Required Fields (part 2)

        create(u"Status", &Document::Item::status, [](auto st) {
            switch (st) {
            default                        :
            case BrickLink::Status::Exclude: return qL1S("X");
            case BrickLink::Status::Include: return qL1S("I");
            case BrickLink::Status::Extra  : return qL1S("E");
            }
        });

        create(u"Qty",       &Document::Item::quantity,     asInt);
        create(u"Price",     &Document::Item::price,        asCurrency);
        create(u"Condition", &Document::Item::condition, [](auto c) {
            return qL1S((c == BrickLink::Condition::New) ? "N" : "U"); });

        // vvv Optional Fields (part 2)

        create(u"SubCondition", &Document::Item::subCondition,
                [](auto sc) {
            // 'M' for sealed is an historic artefact. BL called this 'MISB' back in the day
            switch (sc) {
            case BrickLink::SubCondition::Incomplete: return qL1S("I");
            case BrickLink::SubCondition::Complete  : return qL1S("C");
            case BrickLink::SubCondition::Sealed    : return qL1S("M");
            default                                 : return qL1S("N");
            }
        }, Optional, QVariant::fromValue(BrickLink::SubCondition::None));

        create(u"Bulk",      &Document::Item::bulkQuantity,  asInt,      Optional, 1);
        create(u"Sale",      &Document::Item::sale,          asInt,      Optional, 0);
        create(u"Cost",      &Document::Item::cost,          asCurrency, Optional, 0);
        create(u"Comments",  &Document::Item::comments,      asString,   Optional, QString());
        create(u"Remarks",   &Document::Item::remarks,       asString,   Optional, QString());
        create(u"Reserved",  &Document::Item::reserved,      asString,   Optional, QString());
        create(u"LotID",     &Document::Item::lotId,         asInt,      Optional, 0);
        create(u"TQ1",       &Document::Item::tierQuantity0, asInt,      Optional, 0);
        create(u"TP1",       &Document::Item::tierPrice0,    asCurrency, Optional, 0);
        create(u"TQ2",       &Document::Item::tierQuantity1, asInt,      Optional, 0);
        create(u"TP2",       &Document::Item::tierPrice1,    asCurrency, Optional, 0);
        create(u"TQ3",       &Document::Item::tierQuantity2, asInt,      Optional, 0);
        create(u"TP3",       &Document::Item::tierPrice2,    asCurrency, Optional, 0);

        create(u"Retain", &Document::Item::retain, [](bool b) {
            return qL1S(b ? "Y" : "N"); }, Optional | WriteEmpty, false);

        create(u"Stockroom", &Document::Item::stockroom, [](auto st) {
            switch (st) {
            case BrickLink::Stockroom::A: return qL1S("A");
            case BrickLink::Stockroom::B: return qL1S("B");
            case BrickLink::Stockroom::C: return qL1S("C");
            default                     : return qL1S("N");
            }
        }, Optional, QVariant::fromValue(BrickLink::Stockroom::None));

        if (ii->hasCustomWeight()) {
            create(u"TotalWeight", &Document::Item::totalWeight, [](double d) {
                return QString::number(d, 'f', 4); }, Required);
        }

        if (base && !baseValues.isEmpty()) {
            xml.writeStartElement(qL1S("DifferenceBaseValues"));
            xml.writeAttributes(baseValues);
            xml.writeEndElement(); // DifferenceBaseValues
        }
        xml.writeEndElement(); // Item
    }

    xml.writeEndElement(); // Inventory

    xml.writeStartElement(qL1S("GuiState"));
    xml.writeAttribute(qL1S("Application"), qL1S("BrickStore"));
    xml.writeAttribute(qL1S("Version"), QString::number(2));
    if (!bsx.guiColumnLayout.isEmpty()) {
        xml.writeStartElement("ColumnLayout");
        xml.writeAttribute("Compressed", "1");
        xml.writeCDATA(QLatin1String(qCompress(bsx.guiColumnLayout).toBase64()));
        xml.writeEndElement(); // ColumnLayout
    }
    if (!bsx.guiSortFilterState.isEmpty()) {
        xml.writeStartElement("SortFilterState");
        xml.writeAttribute("Compressed", "1");
        xml.writeCDATA(QLatin1String(qCompress(bsx.guiSortFilterState).toBase64()));
        xml.writeEndElement(); // SortFilterState
    }
    xml.writeEndElement(); // GuiState

    xml.writeEndElement(); // BrickStoreXML
    xml.writeEndDocument();
    return !xml.hasError();
}
