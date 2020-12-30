/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <cfloat>

#include <QApplication>
#include <QCursor>
#include <QFileDialog>
#include <QClipboard>
#include <QRegExp>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QtMath>

#if defined( MODELTEST )
#  include "modeltest.h"
#  define MODELTEST_ATTACH(x)   { (void) new ModelTest(x, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "utility.h"
#include "config.h"
#include "framework.h"
#include "messagebox.h"
#include "undo.h"
#include "progressdialog.h"

#include "importorderdialog.h"
#include "importinventorydialog.h"
//#include "incompleteitemdialog.h"

#include "import.h"
#include "currency.h"

#include "document.h"
#include "document_p.h"


template <typename T> static inline T pack(typename T::const_reference item)
{
    T list;
    list.append(item);
    return list;
}

enum {
    CID_Change,
    CID_AddRemove,
    CID_Currency
};


CurrencyCmd::CurrencyCmd(Document *doc, const QString &ccode, qreal crate)
    : QUndoCommand(qApp->translate("CurrencyCmd", "Changed currency"))
    , m_doc(doc)
    , m_ccode(ccode)
    , m_crate(crate)
    , m_prices(nullptr)
{ }

CurrencyCmd::~CurrencyCmd()
{
    delete [] m_prices;
}

int CurrencyCmd::id() const
{
    return CID_Currency;
}

void CurrencyCmd::redo()
{
    Q_ASSERT(!m_prices);

    QString oldccode = m_doc->currencyCode();
    m_doc->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}

void CurrencyCmd::undo()
{
    Q_ASSERT(m_prices);

    QString oldccode = m_doc->currencyCode();
    m_doc->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}


ChangeCmd::ChangeCmd(Document *doc, int pos, const Document::Item &item, bool merge_allowed)
    : QUndoCommand(qApp->translate("ChangeCmd", "Modified item")), m_doc(doc), m_position(pos), m_item(item), m_merge_allowed(merge_allowed)
{ }

int ChangeCmd::id() const
{
    return CID_Change;
}

void ChangeCmd::redo()
{
    m_doc->changeItemDirect(m_position, m_item);
}

void ChangeCmd::undo()
{
    redo();
}

bool ChangeCmd::mergeWith(const QUndoCommand *other)
{
#if 0 // untested
    const ChangeCmd *that = static_cast <const ChangeCmd *>(other);

    if ((m_merge_allowed && that->m_merge_allowed) &&
        (m_doc == that->m_doc) &&
        (m_position == that->m_position))
    {
        m_item = that->m_item;
        return true;
    }
#else
    Q_UNUSED(other)
    Q_UNUSED(m_merge_allowed)
#endif
    return false;
}


AddRemoveCmd::AddRemoveCmd(Type t, Document *doc, const QVector<int> &positions, const Document::ItemList &items, bool merge_allowed)
    : QUndoCommand(genDesc(t == Add, qMax(items.count(), positions.count()))),
      m_doc(doc), m_positions(positions), m_items(items), m_type(t), m_merge_allowed(merge_allowed)
{
    // for add: specify items and optionally also positions
    // for remove: specify items only
}

AddRemoveCmd::~AddRemoveCmd()
{
    if (m_type == Add)
        qDeleteAll(m_items);
}

int AddRemoveCmd::id() const
{
    return CID_AddRemove;
}

void AddRemoveCmd::redo()
{
    if (m_type == Add) {
        // Document::insertItemsDirect() adds all m_items at the positions given in m_positions
        // (or append them to the document in case m_positions is empty)
        m_doc->insertItemsDirect(m_items, m_positions);
        m_positions.clear();
        m_type = Remove;
    }
    else {
        // Document::removeItemsDirect() removes all m_items and records the positions in m_positions
        m_doc->removeItemsDirect(m_items, m_positions);
        m_type = Add;
    }
}

void AddRemoveCmd::undo()
{
    redo();
}

bool AddRemoveCmd::mergeWith(const QUndoCommand *other)
{
    const auto *that = static_cast <const AddRemoveCmd *>(other);

    if ((m_merge_allowed && that->m_merge_allowed) &&
        (m_doc == that->m_doc) &&
        (m_type == that->m_type)) {
        m_items     += that->m_items;
        m_positions += that->m_positions;
        setText(genDesc(m_type == Remove, qMax(m_items.count(), m_positions.count())));

        const_cast<AddRemoveCmd *>(that)->m_items.clear();
        const_cast<AddRemoveCmd *>(that)->m_positions.clear();
        return true;
    }
    return false;
}

QString AddRemoveCmd::genDesc(bool is_add, int count)
{
    if (is_add)
        return Document::tr("Added %n item(s)", nullptr, count);
    else
        return Document::tr("Removed %n item(s)", nullptr, count);
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

Document::Statistics::Statistics(const Document *doc, const ItemList &list)
{
    m_lots = list.count();
    m_items = 0;
    m_val = m_minval = 0.;
    m_weight = .0;
    m_errors = 0;
    m_incomplete = 0;
    bool weight_missing = false;

    for (const Item *item : list) {
        int qty = item->quantity();
        double price = item->price();

        m_val += (qty * price);

        for (int i = 0; i < 3; i++) {
            if (item->tierQuantity(i) && !qFuzzyIsNull(item->tierPrice(i)))
                price = item->tierPrice(i);
        }
        m_minval += (qty * price * (1.0 - double(item->sale()) / 100.0));
        m_items += qty;

        if (item->weight() > 0)
            m_weight += item->weight();
        else
            weight_missing = true;

        if (item->errors()) {
            quint64 errors = item->errors() & doc->m_error_mask;

            for (uint i = 1ULL << (FieldCount - 1); i;  i >>= 1) {
                if (errors & i)
                    m_errors++;
            }
        }

        if (item->isIncomplete())
            m_incomplete++;
    }
    if (weight_missing)
        m_weight = (m_weight == 0.) ? -DBL_MIN : -m_weight;
    m_ccode = doc->currencyCode();
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************


Document::Item::Item(const BrickLink::InvItem &copy)
    : BrickLink::InvItem(copy)
{ }

Document::Item &Document::Item::operator = (const Item &copy)
{
    BrickLink::InvItem::operator = (copy);

    m_errors = copy.m_errors;
    return *this;
}

bool Document::Item::operator == (const Item &cmp) const
{
    // ignore errors for now!
    return BrickLink::InvItem::operator == (cmp);
}

QImage Document::Item::image() const
{
    BrickLink::Picture *pic = BrickLink::core()->picture(item(), color());

    if (pic && pic->valid()) {
        return pic->image();
    } else {
        QSize s = BrickLink::core()->standardPictureSize();
        QImage img(s, QImage::Format_Mono);
        img.fill(Qt::white);
        return img;
    }
}

QDataStream &operator << (QDataStream &ds, const Document::Item &item)
{
    return operator<<(ds, static_cast<const BrickLink::InvItem &>(item));
}

QDataStream &operator >> (QDataStream &ds, Document::Item &item)
{
    return operator>>(ds, static_cast<BrickLink::InvItem &>(item));
}

// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

QList<Document *> Document::s_documents;

Document *Document::createTemporary(const BrickLink::InvItemList &list)
{
    auto *doc = new Document(1 /*dummy*/);
    doc->setBrickLinkItems(list, 1); // the caller owns the items
    return doc;
}

Document::Document(int /*is temporary*/)
    : m_currencycode(Config::inst()->defaultCurrencyCode())
    , m_uuid(QUuid::createUuid())
{
    MODELTEST_ATTACH(this)

    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            this, &Document::pictureUpdated);
}

Document::Document()
    : Document(0)
{
    m_undo = new UndoStack(this);
    connect(m_undo, &QUndoStack::cleanChanged,
            this, &Document::clean2Modified);

    connect(&m_autosave_timer, &QTimer::timeout,
            this, &Document::autosave);
    m_autosave_timer.start(5000);

    s_documents.append(this);
}

Document::~Document()
{
    m_autosave_timer.stop();
    deleteAutosave();

    delete m_order;
    qDeleteAll(m_items);

    s_documents.removeAll(this);
}

static const char *autosavemagic = "||BRICKSTORE AUTOSAVE MAGIC||";

void Document::deleteAutosave()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString filename = QString("brickstore_%1.autosave").arg(m_uuid.toString());
    temp.remove(filename);
}

void Document::autosave() const
{
    if (m_uuid.isNull() || !isModified())
        return;

    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString filename = QString("brickstore_%1.autosave").arg(m_uuid.toString());

    QFile f(temp.filePath(filename));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QDataStream ds(&f);
        ds << QByteArray(autosavemagic);
        ds << m_items.count();
        for (const Item *item : m_items)
            ds << *item;
        ds << QByteArray(autosavemagic);
    }
}

QList<Document::ItemList> Document::restoreAutosave()
{
    QList<ItemList> restored;

    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const QStringList ondisk = temp.entryList(QStringList(QLatin1String("brickstore_*.autosave")));

    for (const QString &filename : ondisk) {
        QFile f(temp.filePath(filename));
        if (f.open(QIODevice::ReadOnly)) {
            ItemList items;
            QByteArray magic;
            int count = 0;

            QDataStream ds(&f);
            ds >> magic >> count;

            if (count > 0 && magic == QByteArray(autosavemagic)) {
                for (int i = 0; i < count; ++i) {
                    Item *item = new Item();
                    ds >> *item;
                    items.append(item);
                }
                ds >> magic;

                if (magic == QByteArray(autosavemagic))
                    restored.append(items);
            }
            f.close();
            f.remove();
        }
    }
    return restored;
}


const QList<Document *> &Document::allDocuments()
{
    return s_documents;
}

const Document::ItemList &Document::items() const
{
    return m_items;
}

Document::Statistics Document::statistics(const ItemList &list) const
{
    return Statistics(this, list);
}

void Document::beginMacro(const QString &label)
{
    m_undo->beginMacro(label);
}

void Document::endMacro(const QString &label)
{
    m_undo->endMacro(label);
}

QUndoStack *Document::undoStack() const
{
    return m_undo;
}


bool Document::clear()
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, QVector<int>(), m_items));
    return true;
}

int Document::positionOf(Item *item) const
{
    return m_items.indexOf(item);
}

const Document::Item *Document::itemAt(int position) const
{
    return (position >= 0 && position < m_items.count()) ? m_items.at(position) : nullptr;
}

bool Document::insertItems(const QVector<int> &positions, const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, items /*, true*/));
    return true;
}

bool Document::removeItem(Item *item)
{
    return removeItems(pack<ItemList>(item));
}

bool Document::removeItems(const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, QVector<int>(), items /*, true*/));
    return true;
}

bool Document::insertItem(int position, Item *item)
{
    return insertItems(pack<QVector<int> > (position), pack<ItemList> (item));
}

bool Document::changeItem(Item *item, const Item &value)
{
    return changeItem(positionOf(item), value);
}

bool Document::changeItem(int position, const Item &value)
{
    m_undo->push(new ChangeCmd(this, position, value /*, true*/));
    return true;
}

void Document::insertItemsDirect(ItemList &items, QVector<int> &positions)
{
    auto pos = positions.constBegin();
    QModelIndex root;

    for (Item *item : qAsConst(items)) {
        int rows = rowCount(root);

        if (pos != positions.constEnd()) {
            beginInsertRows(root, *pos, *pos);
            m_items.insert(*pos, item);
            ++pos;
        }
        else {
            beginInsertRows(root, rows, rows);
            m_items.append(item);
        }
        updateErrors(item);
        endInsertRows();
    }

//    emit itemsAdded(items);
    emit statisticsChanged();
}

void Document::removeItemsDirect(ItemList &items, QVector<int> &positions)
{
    positions.resize(items.count());

    for (int i = items.count() - 1; i >= 0; --i) {
        Item *item = items[i];
        int idx = m_items.indexOf(item);
        beginRemoveRows(QModelIndex(), idx, idx);
        positions[i] = idx;
        m_items.removeAt(idx);
        endRemoveRows();
    }

    emit statisticsChanged();
}

void Document::changeItemDirect(int position, Item &item)
{
    Item *olditem = m_items[position];
    std::swap(*olditem, item);

    QModelIndex idx1 = index(olditem);
    QModelIndex idx2 = createIndex(idx1.row(), columnCount(idx1.parent()) - 1, idx1.internalPointer());

    emit dataChanged(idx1, idx2);
    updateErrors(olditem);
    emit statisticsChanged();

    bool grave = (olditem->item() != item.item()) || (olditem->color() != item.color());
    emit itemsChanged(pack<ItemList>(olditem), grave);
}

void Document::changeCurrencyDirect(const QString &ccode, qreal crate, double *&prices)
{
    m_currencycode = ccode;

    if (!qFuzzyCompare(crate, qreal(1))) {
        bool createPrices = (prices == nullptr);
        if (createPrices)
            prices = new double[5 * m_items.count()];

        for (int i = 0; i < m_items.count(); ++i) {
            Item *item = m_items[i];
            if (createPrices) {
                prices[i * 5] = item->origPrice();
                prices[i * 5 + 1] = item->price();
                prices[i * 5 + 2] = item->tierPrice(0);
                prices[i * 5 + 3] = item->tierPrice(1);
                prices[i * 5 + 4] = item->tierPrice(2);

                item->setOrigPrice(prices[i * 5] * crate);
                item->setPrice(prices[i * 5 + 1] * crate);
                item->setTierPrice(0, prices[i * 5 + 2] * crate);
                item->setTierPrice(1, prices[i * 5 + 3] * crate);
                item->setTierPrice(2, prices[i * 5 + 4] * crate);
            } else {
                item->setOrigPrice(prices[i * 5]);
                item->setPrice(prices[i * 5 + 1]);
                item->setTierPrice(0, prices[i * 5 + 2]);
                item->setTierPrice(1, prices[i * 5 + 3]);
                item->setTierPrice(2, prices[i * 5 + 4]);
            }
        }

        if (!createPrices) {
            delete [] prices;
            prices = nullptr;
        }

        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        emit statisticsChanged();
        emit itemsChanged(m_items, false);
    }
    emit currencyCodeChanged(m_currencycode);
}

void Document::updateErrors(Item *item)
{
    quint64 errors = 0;

    if (item->price() <= 0)
        errors |= (1ULL << Price);

    if (item->quantity() <= 0)
        errors |= (1ULL << Quantity);

    if (item->color() && item->itemType() && ((item->color()->id() != 0) && !item->itemType()->hasColors()))
        errors |= (1ULL << Color);

    if (item->tierQuantity(0) && ((item->tierPrice(0) <= 0) || (item->tierPrice(0) >= item->price())))
        errors |= (1ULL << TierP1);

    if (item->tierQuantity(1) && ((item->tierPrice(1) <= 0) || (item->tierPrice(1) >= item->tierPrice(0))))
        errors |= (1ULL << TierP2);

    if (item->tierQuantity(1) && (item->tierQuantity(1) <= item->tierQuantity(0)))
        errors |= (1ULL << TierQ2);

    if (item->tierQuantity(2) && ((item->tierPrice(2) <= 0) || (item->tierPrice(2) >= item->tierPrice(1))))
        errors |= (1ULL << TierP3);

    if (item->tierQuantity(2) && (item->tierQuantity(2) <= item->tierQuantity(1)))
        errors |= (1ULL << TierQ3);

    if (errors != item->errors()) {
        item->setErrors(errors);
        emit errorsChanged(item);
        emit statisticsChanged();
    }
}

QString Document::currencyCode() const
{
    return m_currencycode;
}

void Document::setCurrencyCode(const QString &ccode, qreal crate)
{
    if (ccode != m_currencycode)
        m_undo->push(new CurrencyCmd(this, ccode, crate));
}

Document *Document::fileNew()
{
    auto *doc = new Document();
    doc->setTitle(tr("Untitled"));
    return doc;
}

Document *Document::fileOpen()
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";
    filters << tr("All Files") + "(*.*)";

    return fileOpen(QFileDialog::getOpenFileName(FrameWork::inst(), tr("Open File"), Config::inst()->documentDir(), filters.join(";;")));
}

Document *Document::fileOpen(const QString &s)
{
    if (s.isEmpty())
        return nullptr;

    QString abs_s = QFileInfo(s).absoluteFilePath();

    for (Document *doc : qAsConst(s_documents)) {
        if (QFileInfo(doc->fileName()).absoluteFilePath() == abs_s)
            return doc;
    }

    return fileLoadFrom(s, "bsx");
}

Document *Document::fileImportBrickLinkInventory(const BrickLink::Item *item)
{
    if (item && !item->hasInventory())
        return nullptr;

    int qty = 1;

    if (!item) {
        ImportInventoryDialog dlg(FrameWork::inst());

        QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/Geometry")).toByteArray();
        if (!ba.isEmpty())
            dlg.restoreGeometry(ba);

        if (dlg.exec() == QDialog::Accepted) {
            item = dlg.item();
            qty = dlg.quantity();
        }
        Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Geometry", dlg.saveGeometry());
    }

    if (item && (qty > 0)) {
        BrickLink::InvItemList items = item->consistsOf();

        if (!items.isEmpty()) {
            auto *doc = new Document();

            doc->setBrickLinkItems(items, uint(qty)); // we own the items
            qDeleteAll(items);
            doc->setTitle(tr("Inventory for %1").arg(item->id()));
            return doc;
        } else {
            MessageBox::warning(FrameWork::inst(), tr("Internal error: Could not create an Inventory object for item %1").arg(CMB_BOLD(item->id())));
        }
    }
    return nullptr;
}

QList<Document *> Document::fileImportBrickLinkOrders()
{
    QList<Document *> docs;

    ImportOrderDialog dlg(FrameWork::inst());

    if (dlg.exec() == QDialog::Accepted) {
        QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > orders = dlg.orders();

        for (auto it = orders.constBegin(); it != orders.constEnd(); ++it) {
            const auto &order = *it;

            if (order.first && order.second) {
                auto *doc = new Document();

                doc->setTitle(tr("Order #%1").arg(order.first->id()));
                doc->m_currencycode = order.first->currencyCode();
                doc->setBrickLinkItems(*order.second); // ImportOrderDialog owns the items
                doc->m_order = new BrickLink::Order(*order. first);
                docs.append(doc);
            }
        }
    }
    return docs;
}

Document *Document::fileImportBrickLinkStore()
{
    Transfer trans;
    ProgressDialog d(&trans, FrameWork::inst());
    ImportBLStore import(&d);

    if (d.exec() == QDialog::Accepted) {
        auto *doc = new Document();

        doc->setTitle(tr("Store %1").arg(QDate::currentDate().toString(Qt::LocalDate)));
        doc->m_currencycode = import.currencyCode();
        doc->setBrickLinkItems(import.items()); // ImportBLStore owns the items
        return doc;
    }
    return nullptr;
}

Document *Document::fileImportBrickLinkCart()
{
    QString url = QApplication::clipboard()->text(QClipboard::Clipboard);
    QRegExp rx_valid(QLatin1String(R"(https://www\.bricklink\.com/storeCart\.asp\?h=[0-9]+&b=[-0-9]+)"));

    if (!rx_valid.exactMatch(url))
        url = QLatin1String("https://www.bricklink.com/storeCart.asp?h=______&b=______");

    if (MessageBox::getString(FrameWork::inst(), tr("Enter the URL of your current BrickLink shopping cart:"
                               "<br /><br />Right-click on the <b>View Cart</b> button "
                               "in your browser and copy the URL to the clipboard by choosing "
                               "<b>Copy Link Location</b> (Firefox), <b>Copy Link</b> (Safari) "
                               "or <b>Copy Shortcut</b> (Internet Explorer).<br /><br />"
                               "<em>Super-lots and custom items are <b>not</b> supported</em>."), url)) {
        QRegExp rx(QLatin1String("\\?h=([0-9]+)&b=([-0-9]+)"));
        (void) rx.indexIn(url);
        int shopid = rx.cap(1).toInt();
        int cartid = rx.cap(2).toInt();

        if (shopid && cartid) {
            Transfer trans;
            ProgressDialog d(&trans, FrameWork::inst());
            ImportBLCart import(shopid, cartid, &d);

            if (d.exec() == QDialog::Accepted) {
                auto *doc = new Document();

                doc->m_currencycode = import.currencyCode();
                doc->setBrickLinkItems(import.items()); // ImportBLCart owns the items
                doc->setTitle(tr("Cart in Shop %1").arg(shopid));
                return doc;
            }
        }
        else
            QApplication::beep();
    }
    return nullptr;
}

Document *Document::fileImportBrickLinkXML()
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";
    filters << tr("All Files") + "(*.*)";

    QString s = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), Config::inst()->documentDir(), filters.join(";;"));

    if (!s.isEmpty()) {
        Document *doc = fileLoadFrom(s, "xml", true);

        if (doc)
            doc->setTitle(tr("Import of %1").arg(QFileInfo(s).fileName()));
        return doc;
    }
    else
        return nullptr;
}

Document *Document::fileLoadFrom(const QString &name, const char *type, bool import_only)
{
    BrickLink::ItemListXMLHint hint;

    if (qstrcmp(type, "bsx") == 0)
        hint = BrickLink::XMLHint_BrickStore;
    else if (qstrcmp(type, "xml") == 0)
        hint = BrickLink::XMLHint_MassUpload;
    else
        return nullptr;


    QFile f(name);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(FrameWork::inst(), tr("Could not open file %1 for reading.").arg(CMB_BOLD(name)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    BrickLink::Core::ParseItemListXMLResult result;

    QString emsg;
    int eline = 0, ecol = 0;
    QDomDocument domdoc;

    if (domdoc.setContent(&f, &emsg, &eline, &ecol)) {
        QDomElement root = domdoc.documentElement();
        QDomElement item_elem;

        if (hint == BrickLink::XMLHint_BrickStore) {
            for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
                if (!n.isElement())
                    continue;

                if (n.nodeName() == QLatin1String("Inventory"))
                    item_elem = n.toElement();
            }
        }
        else {
            item_elem = root;
        }

        result = BrickLink::core()->parseItemListXML(item_elem, hint); // we own the items now
    }
    else {
        MessageBox::warning(FrameWork::inst(), tr("Could not parse the XML data in file %1:<br /><i>Line %2, column %3: %4</i>").arg(CMB_BOLD(name)).arg(eline).arg(ecol).arg(emsg));
        QApplication::restoreOverrideCursor();
        return nullptr;
    }

    QApplication::restoreOverrideCursor();

    Document *doc = nullptr;

    if (result.items) {
        uint fixedCount = 0;

        if (result.invalidItemCount) {
            fixedCount = uint(BrickLink::core()->applyChangeLogToItems(*result.items));
            result.invalidItemCount -= fixedCount;

            if (result.invalidItemCount) {
                if (MessageBox::information(FrameWork::inst(),
                                            tr("This file contains %n unknown item(s).<br /><br />Do you still want to open this file?",
                                               nullptr, result.invalidItemCount),
                                            QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                    result.invalidItemCount = 0;
                }
            }
        }

        if (!result.invalidItemCount) {
            doc = new Document();
            doc->m_currencycode = result.currencyCode; // we own the items
            doc->setBrickLinkItems(*result.items);

            doc->setFileName(import_only ? QString() : name);
            if (!import_only)
                Config::inst()->addToRecentFiles(name);

            if (fixedCount) {
                QString fixedMsg = tr("While loading, the item and color ids of %n item(s) have been adjusted automatically according to the current BrickLink catalog change log.",
                                      nullptr, fixedCount);

                if (!import_only) {
                    if (MessageBox::question(FrameWork::inst(),
                                             fixedMsg + "<br><br>" + tr("Do you want to save these changes now?"),
                                             MessageBox::Yes | MessageBox::No) == MessageBox::Yes) {
                        doc->fileSaveTo(name, type, true, *result.items);
                    }
                } else {
                    MessageBox::information(FrameWork::inst(), fixedMsg);

                }
            }
        }

        qDeleteAll(*result.items);
        delete result.items;
    } else {
        MessageBox::warning(FrameWork::inst(), tr("Could not parse the XML data in file %1.").arg(CMB_BOLD(name)));
    }

    return doc;
}

Document *Document::fileImportLDrawModel()
{
    QStringList filters;
    filters << tr("LDraw Models") + " (*.dat;*.ldr;*.mpd)";
    filters << tr("All Files") + "(*.*)";

    QString s = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), Config::inst()->documentDir(), filters.join(";;"));

    if (s.isEmpty())
        return nullptr;

    QFile f(s);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(FrameWork::inst(), tr("Could not open file %1 for reading.").arg(CMB_BOLD(s)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    uint invalid_items = 0;
    BrickLink::InvItemList items; // we own the items

    bool b = BrickLink::core()->parseLDrawModel(f, items, &invalid_items);
    Document *doc = nullptr;

    QApplication::restoreOverrideCursor();

    if (b && !items.isEmpty()) {
        if (invalid_items) {
            if (MessageBox::information(FrameWork::inst(),
                                        tr("This file contains %n unknown item(s).<br /><br />Do you still want to open this file?",
                                           nullptr, invalid_items),
                                        QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                invalid_items = 0;
            }
        }

        if (!invalid_items) {
            doc = new Document();
            doc->setBrickLinkItems(items); // we own the items
            doc->setTitle(tr("Import of %1").arg(QFileInfo(s).fileName()));
        }
    } else {
        MessageBox::warning(FrameWork::inst(), tr("Could not parse the LDraw model in file %1.").arg(CMB_BOLD(s)));
    }

    qDeleteAll(items);
    return doc;
}



void Document::setBrickLinkItems(const BrickLink::InvItemList &bllist, uint multiply)
{
    ItemList items;
    QVector<int> positions;

    for (const BrickLink::InvItem *blitem : bllist) {
        Item *item = new Item(*blitem);

        if (item->isIncomplete()) {
            //IncompleteItemDialog dlg(item, FrameWork::inst());

            //if (waitcursor)
            //    QApplication::restoreOverrideCursor();

            bool drop_this = false; //(dlg.exec() != QDialog::Accepted);

            //if (waitcursor)
            //    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

            if (drop_this)
                continue;
        }
        item->setQuantity(item->quantity() * int(multiply));
        items.append(item);
    }
    insertItemsDirect(items, positions);

    // reset difference WITHOUT a command

    for (Item *pos : qAsConst(m_items)) {
        if ((pos->origQuantity() != pos->quantity()) ||
            (!qFuzzyCompare(pos->origPrice(), pos->price()))) {
            pos->setOrigQuantity(pos->quantity());
            pos->setOrigPrice(pos->price());
        }
    }
}

QString Document::fileName() const
{
    return m_filename;
}

void Document::setFileName(const QString &str)
{
    m_filename = str;

    QFileInfo fi(str);

    if (fi.exists())
        setTitle(QDir::toNativeSeparators(fi.absoluteFilePath()));

    emit fileNameChanged(m_filename);
}

QString Document::title() const
{
    return m_title;
}

void Document::setTitle(const QString &str)
{
    m_title = str;
    emit titleChanged(m_title);
}

bool Document::isModified() const
{
    return !m_undo->isClean();
}

void Document::fileSave()
{
    if (fileName().isEmpty())
        fileSaveAs();
    else if (isModified())
        fileSaveTo(fileName(), "bsx", false, items());
}


void Document::fileSaveAs()
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";

    QString fn = fileName();

    if (fn.isEmpty()) {
        QDir d(Config::inst()->documentDir());

        if (d.exists())
            fn = d.filePath(m_title);
    }
    if (fn.right(4) == ".xml")
        fn.truncate(fn.length() - 4);

    fn = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Save File as"), fn, filters.join(";;"));

    if (!fn.isNull()) {
        if (fn.right(4) != ".bsx")
            fn += ".bsx";

        if (QFile::exists(fn) &&
            MessageBox::question(FrameWork::inst(), tr("A file named %1 already exists.Are you sure you want to overwrite it?").arg(CMB_BOLD(fn)), MessageBox::Yes, MessageBox::No) != MessageBox::Yes)
            return;

        fileSaveTo(fn, "bsx", false, items());
    }
}


bool Document::fileSaveTo(const QString &s, const char *type, bool export_only, const ItemList &itemlist)
{
    BrickLink::ItemListXMLHint hint;

    if (qstrcmp(type, "bsx") == 0)
        hint = BrickLink::XMLHint_BrickStore;
    else if (qstrcmp(type, "xml") == 0)
        hint = BrickLink::XMLHint_MassUpload;
    else
        return false;


    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        QDomDocument doc((hint == BrickLink::XMLHint_BrickStore) ? QString("BrickStoreXML") : QString());
        doc.appendChild(doc.createProcessingInstruction("xml", R"(version="1.0" encoding="UTF-8")"));

        QDomElement item_elem = BrickLink::core()->createItemListXML(doc, hint, itemlist, m_currencycode);

        if (hint == BrickLink::XMLHint_BrickStore) {
            QDomElement root = doc.createElement("BrickStoreXML");

            root.appendChild(item_elem);
            doc.appendChild(root);
        }
        else {
            doc.appendChild(item_elem);
        }

        QByteArray output = doc.toByteArray();
        bool ok = (f.write(output.data(), output.size()) == qint64(output.size()));

        QApplication::restoreOverrideCursor();

        if (ok) {
            if (!export_only) {
                deleteAutosave();
                m_undo->setClean();
                setFileName(s);

                Config::inst()->addToRecentFiles(s);
            }
            return true;
        }
        else
            MessageBox::warning(FrameWork::inst(), tr("Failed to save data in file %1.").arg(CMB_BOLD(s)));
    }
    else
        MessageBox::warning(FrameWork::inst(), tr("Failed to open file %1 for writing.").arg(CMB_BOLD(s)));

    return false;
}

void Document::fileExportBrickLinkInvReqClipboard(const ItemList &itemlist)
{
    QDomDocument doc(QString {});
    doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_Inventory, itemlist, m_currencycode));

    QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

    if (Config::inst()->value("/General/Export/OpenBrowser", true).toBool())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_InventoryRequest));
}

void Document::fileExportBrickLinkWantedListClipboard(const ItemList &itemlist)
{
    QString wantedlist;

    if (MessageBox::getString(FrameWork::inst(), tr("Enter the ID number of Wanted List (leave blank for the default Wanted List)"), wantedlist)) {
        QMap <QString, QString> extra;
        if (!wantedlist.isEmpty())
            extra.insert("WANTEDLISTID", wantedlist);

        QDomDocument doc(QString {});
        doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_WantedList, itemlist, m_currencycode, extra.isEmpty() ? nullptr : &extra));

        QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

        if (Config::inst()->value("/General/Export/OpenBrowser", true).toBool())
            QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_WantedListUpload));
    }
}

void Document::fileExportBrickLinkXMLClipboard(const ItemList &itemlist)
{
    QDomDocument doc(QString {});
    doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_MassUpload, itemlist, m_currencycode));

    QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

    if (Config::inst()->value("/General/Export/OpenBrowser", true).toBool())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_InventoryUpload));
}

void Document::fileExportBrickLinkUpdateClipboard(const ItemList &itemlist)
{
    for (const Item *item : itemlist) {
        if (!item->lotId()) {
            if (MessageBox::warning(FrameWork::inst(), tr("This list contains items without a BrickLink Lot-ID.<br /><br />Do you really want to export this list?"), MessageBox::Yes, MessageBox::No) != MessageBox::Yes)
                return;
            else
                break;
        }
    }

    QDomDocument doc(QString {});
    doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_MassUpdate, itemlist, m_currencycode));

    QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

    if (Config::inst()->value(QLatin1String("/General/Export/OpenBrowser"), true).toBool())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_InventoryUpdate));
}

void Document::fileExportBrickLinkXML(const ItemList &itemlist)
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";

    QString s = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Export File"), Config::inst()->documentDir(), filters.join(";;"));

    if (!s.isNull()) {
        if (s.right(4) != QLatin1String(".xml"))
            s += QLatin1String(".xml");

        if (QFile::exists(s) &&
            MessageBox::question(FrameWork::inst(), tr("A file named %1 already exists.Are you sure you want to overwrite it?").arg(CMB_BOLD(s)), MessageBox::Yes, MessageBox::No) != MessageBox::Yes)
            return;

        fileSaveTo(s, "xml", true, itemlist);
    }
}

void Document::clean2Modified(bool b)
{
    emit modificationChanged(!b);
}

quint64 Document::errorMask() const
{
    return m_error_mask;
}

void Document::setErrorMask(quint64 em)
{
    m_error_mask = em;
    emit statisticsChanged();
    emit itemsChanged(items(), false);
}

const BrickLink::Order *Document::order() const
{
    return m_order;
}

void Document::resetDifferences(const ItemList &items)
{
    beginMacro(tr("Reset differences"));

    for (Item *pos : items) {
        if ((pos->origQuantity() != pos->quantity()) ||
                (!qFuzzyCompare(pos->origPrice(), pos->price()))) {
            Item item = *pos;

            item.setOrigQuantity(item.quantity());
            item.setOrigPrice(item.price());
            changeItem(pos, item);
        }
    }
    endMacro();
}








////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
// Itemviews API
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

QModelIndex Document::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, m_items.at(row));
    return {};
}

Document::Item *Document::item(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<Item *>(idx.internalPointer()) : nullptr;
}

QModelIndex Document::index(const Item *ci, int column) const
{
    Item *i = const_cast<Item *>(ci);

    return i ? createIndex(m_items.indexOf(i), column, i) : QModelIndex();
}


int Document::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : items().size();
}

int Document::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : FieldCount;
}

Qt::ItemFlags Document::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemFlags();

    Qt::ItemFlags ifs = QAbstractItemModel::flags(index);

    switch (index.column()) {
    case Total       :
    case ItemType    :
    case Category    :
    case YearReleased:
    case LotId       : break;
    case Retain      : ifs |= Qt::ItemIsUserCheckable; Q_FALLTHROUGH();
    default          : ifs |= Qt::ItemIsEditable; break;
    }
    return ifs;
}

bool Document::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        Item *itemp = items().at(index.row());
        Item item = *itemp;
        auto f = static_cast<Field>(index.column());

        switch (f) {
        case Document::PartNo      : {
            char itid = item.itemType() ? item.itemType()->id() : 'P';
            const BrickLink::Item *newitem = BrickLink::core()->item(itid, value.toString().toLatin1().constData());
            if (newitem)
                item.setItem(newitem);
            break;
        }
        case Comments    : item.setComments(value.toString()); break;
        case Remarks     : item.setRemarks(value.toString()); break;
        case Reserved    : item.setReserved(value.toString()); break;
        case Sale        : item.setSale(value.toInt()); break;
        case Bulk        : item.setBulkQuantity(value.toInt()); break;
        case TierQ1      : item.setTierQuantity(0, value.toInt()); break;
        case TierQ2      : item.setTierQuantity(1, value.toInt()); break;
        case TierQ3      : item.setTierQuantity(2, value.toInt()); break;
        case TierP1      : item.setTierPrice(0, Currency::fromString(value.toString())); break;
        case TierP2      : item.setTierPrice(1, Currency::fromString(value.toString())); break;
        case TierP3      : item.setTierPrice(2, Currency::fromString(value.toString())); break;
        case Weight      : item.setWeight(Utility::stringToWeight(value.toString(), Config::inst()->measurementSystem())); break;
        case Quantity    : item.setQuantity(value.toInt()); break;
        case QuantityDiff: item.setQuantity(itemp->origQuantity() + value.toInt()); break;
        case Price       : item.setPrice(Currency::fromString(value.toString())); break;
        case PriceDiff   : item.setPrice(itemp->origPrice() + Currency::fromString(value.toString())); break;
        default          : break;
        }
        if (!(item == *itemp)) {
            changeItem(index.row(), item);
            emit dataChanged(index, index);
            return true;
        }
    }
    return false;
}


QVariant Document::data(const QModelIndex &index, int role) const
{
    //if (role == Qt::DecorationRole)
    //qDebug() << "data() for row " << index.row() << "(picture: " << items().at(index.row())->m_picture << ", item: "<< items().at(index.row())->item()->name();


    if (index.isValid()) {
        Item *it = items().at(index.row());
        auto f = static_cast<Field>(index.column());

        switch (role) {
        case Qt::DisplayRole      : return dataForDisplayRole(it, f);
        case Qt::DecorationRole   : return dataForDecorationRole(it, f);
        case Qt::ToolTipRole      : return dataForToolTipRole(it, f);
        case Qt::TextAlignmentRole: return dataForTextAlignmentRole(it, f);
        case Qt::EditRole         : return dataForEditRole(it, f);
        case Qt::CheckStateRole   : return dataForCheckStateRole(it, f);
        }
    }
    return QVariant();
}

QVariant Document::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        auto f = static_cast<Field>(section);

        switch (role) {
        case Qt::DisplayRole      : return headerDataForDisplayRole(f);
        case Qt::TextAlignmentRole: return headerDataForTextAlignmentRole(f);
        case Qt::UserRole         : return headerDataForDefaultWidthRole(f);
        }
    }
    return QVariant();
}

QVariant Document::dataForEditRole(Item *it, Field f) const
{
    switch (f) {
    case PartNo      : return it->itemId();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case Reserved    : return it->reserved();
    case Sale        : return it->sale();
    case Bulk        : return it->bulkQuantity();
    case TierQ1      : return it->tierQuantity(0);
    case TierQ2      : return it->tierQuantity(1);
    case TierQ3      : return it->tierQuantity(2);
    case TierP1      : return Currency::toString(!qFuzzyIsNull(it->tierPrice(0)) ? it->tierPrice(0) : it->price(),      currencyCode());
    case TierP2      : return Currency::toString(!qFuzzyIsNull(it->tierPrice(1)) ? it->tierPrice(1) : it->tierPrice(0), currencyCode());
    case TierP3      : return Currency::toString(!qFuzzyIsNull(it->tierPrice(2)) ? it->tierPrice(2) : it->tierPrice(1), currencyCode());
    case Weight      : return Utility::weightToString(it->weight(), Config::inst()->measurementSystem(), false);
    case Quantity    : return it->quantity();
    case QuantityDiff: return it->quantity() - it->origQuantity();
    case Price       : return Currency::toString(it->price(), currencyCode());
    case PriceDiff   : return Currency::toString(it->price() - it->origPrice(), currencyCode());
    default          : return QString();
    }
}

QString Document::dataForDisplayRole(Item *it, Field f) const
{
    QString dash = QLatin1String("-");

    switch (f) {
    case LotId       : return (it->lotId() == 0 ? dash : QString::number(it->lotId()));
    case PartNo      : return it->itemId();
    case Description : return it->itemName();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case Quantity    : return QString::number(it->quantity());
    case Bulk        : return (it->bulkQuantity() == 1 ? dash : QString::number(it->bulkQuantity()));
    case Price       : return Currency::toString(it->price(), currencyCode());
    case Total       : return Currency::toString(it->total(), currencyCode());
    case Sale        : return (it->sale() == 0 ? dash : QString::number(it->sale()) + QLatin1Char('%'));
    case Condition   : return (it->condition() == BrickLink::Condition::New ? tr("N", "New") : tr("U", "Used"));
    case Color       : return it->colorName();
    case Category    : return it->category() ? it->category()->name() : dash;
    case ItemType    : return it->itemType() ? it->itemType()->name() : dash;
    case TierQ1      : return (it->tierQuantity(0) == 0 ? dash : QString::number(it->tierQuantity(0)));
    case TierQ2      : return (it->tierQuantity(1) == 0 ? dash : QString::number(it->tierQuantity(1)));
    case TierQ3      : return (it->tierQuantity(2) == 0 ? dash : QString::number(it->tierQuantity(2)));
    case TierP1      : return Currency::toString(it->tierPrice(0), currencyCode());
    case TierP2      : return Currency::toString(it->tierPrice(1), currencyCode());
    case TierP3      : return Currency::toString(it->tierPrice(2), currencyCode());
    case Reserved    : return it->reserved();
    case Weight      : return qFuzzyIsNull(it->weight()) ? dash : Utility::weightToString(it->weight(), Config::inst()->measurementSystem(), true, true);
    case YearReleased: return (it->itemYearReleased() == 0) ? dash : QString::number(it->itemYearReleased());

    case PriceOrig   : return Currency::toString(it->origPrice(), currencyCode());
    case PriceDiff   : return Currency::toString(it->price() - it->origPrice(), currencyCode());
    case QuantityOrig: return QString::number(it->origQuantity());
    case QuantityDiff: return QString::number(it->quantity() - it->origQuantity());
    default          : return QString();
    }
}

QVariant Document::dataForDecorationRole(Item *it, Field f) const
{
    switch (f) {
    case Picture: return it->image();
    default     : return QPixmap();
    }
}

Qt::CheckState Document::dataForCheckStateRole(Item *it, Field f) const
{
    switch (f) {
    case Retain   : return it->retain() ? Qt::Checked : Qt::Unchecked;
    default       : return Qt::Unchecked;
    }
}

int Document::dataForTextAlignmentRole(Item *, Field f) const
{
    switch (f) {
    case Retain      :
    case Stockroom   :
    case Picture     :
    case Condition   : return Qt::AlignVCenter | Qt::AlignHCenter;
    case PriceOrig   :
    case PriceDiff   :
    case Price       :
    case Total       :
    case Sale        :
    case TierP1      :
    case TierP2      :
    case TierP3      :
    case Weight      : return Qt::AlignRight | Qt::AlignVCenter;
    default          : return Qt::AlignLeft | Qt::AlignVCenter;
    }
}

QString Document::dataForToolTipRole(Item *it, Field f) const
{
    switch (f) {
    case Status: {
        QString str;
        switch (it->status()) {
        case BrickLink::Status::Exclude: str = tr("Exclude"); break;
        case BrickLink::Status::Extra  : str = tr("Extra"); break;
        case BrickLink::Status::Include: str = tr("Include"); break;
        default                : break;
        }
        if (it->counterPart())
            str += QLatin1String("\n(") + tr("Counter part") + QLatin1Char(')');
        else if (it->alternateId())
            str += QLatin1String("\n(") + tr("Alternate match id: %1").arg(it->alternateId()) + QLatin1Char(')');
        return str;
    }
    case Picture: {
        return dataForDisplayRole(it, PartNo) + QLatin1Char(' ') + dataForDisplayRole(it, Description);
    }
    case Condition: {
        switch (it->condition()) {
        case BrickLink::Condition::New : return tr("New");
        case BrickLink::Condition::Used: return tr("Used");
        default                        : break;
        }
        break;
    }
    case Category: {
        if (!it->item())
            break;

        const auto allcats = it->item()->allCategories();

        if (allcats.size() == 1) {
            return allcats[0]->name();
        }
        else {
            QString str = QLatin1String("<b>") + allcats[0]->name() + QLatin1String("</b>");
            for (int i = 1; i < allcats.size(); ++i)
                str = str + QLatin1String("<br />") + allcats[i]->name();
            return str;
        }
    }
    default: {
        return dataForDisplayRole(it, f);
    }
    }
    return QString();
}


QString Document::headerDataForDisplayRole(Field f)
{
    switch (f) {
    case Status      : return tr("Status");
    case Picture     : return tr("Image");
    case PartNo      : return tr("Part #");
    case Description : return tr("Description");
    case Comments    : return tr("Comments");
    case Remarks     : return tr("Remarks");
    case QuantityOrig: return tr("Qty.Orig");
    case QuantityDiff: return tr("Qty.Diff");
    case Quantity    : return tr("Qty.");
    case Bulk        : return tr("Bulk");
    case PriceOrig   : return tr("Pr.Orig");
    case PriceDiff   : return tr("Pr.Diff");
    case Price       : return tr("Price");
    case Total       : return tr("Total");
    case Sale        : return tr("Sale");
    case Condition   : return tr("Cond.");
    case Color       : return tr("Color");
    case Category    : return tr("Category");
    case ItemType    : return tr("Item Type");
    case TierQ1      : return tr("Tier Q1");
    case TierP1      : return tr("Tier P1");
    case TierQ2      : return tr("Tier Q2");
    case TierP2      : return tr("Tier P2");
    case TierQ3      : return tr("Tier Q3");
    case TierP3      : return tr("Tier P3");
    case LotId       : return tr("Lot Id");
    case Retain      : return tr("Retain");
    case Stockroom   : return tr("Stockroom");
    case Reserved    : return tr("Reserved");
    case Weight      : return tr("Weight");
    case YearReleased: return tr("Year");
    default          : return QString();
    }
}

int Document::headerDataForTextAlignmentRole(Field f) const
{
    return dataForTextAlignmentRole(nullptr, f);
}

int Document::headerDataForDefaultWidthRole(Field f) const
{
    int width = 0;
    QSize picsize = BrickLink::core()->standardPictureSize();

    switch (f) {
    case Status      : width = 6; break;
    case Picture     : width = -picsize.width(); break;
    case PartNo      : width = 10; break;
    case Description : width = 28; break;
    case Comments    : width = 8; break;
    case Remarks     : width = 8; break;
    case QuantityOrig: width = 5; break;
    case QuantityDiff: width = 5; break;
    case Quantity    : width = 5; break;
    case Bulk        : width = 5; break;
    case PriceOrig   : width = 8; break;
    case PriceDiff   : width = 8; break;
    case Price       : width = 8; break;
    case Total       : width = 8; break;
    case Sale        : width = 5; break;
    case Condition   : width = 5; break;
    case Color       : width = 15; break;
    case Category    : width = 12; break;
    case ItemType    : width = 12; break;
    case TierQ1      : width = 5; break;
    case TierP1      : width = 8; break;
    case TierQ2      : width = 5; break;
    case TierP2      : width = 8; break;
    case TierQ3      : width = 5; break;
    case TierP3      : width = 8; break;
    case LotId       : width = 8; break;
    case Retain      : width = 8; break;
    case Stockroom   : width = 8; break;
    case Reserved    : width = 8; break;
    case Weight      : width = 10; break;
    case YearReleased: width = 5; break;
    default          : break;
    }
    return width;
}


void Document::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item())
        return;

    int row = 0;
    for (const Item *it : qAsConst(m_items)) {
        if ((pic->item() == it->item()) && (pic->color() == it->color())) {
            QModelIndex idx = index(row, Picture);
            emit dataChanged(idx, idx);
        }
        row++;
    }
}


QString Document::subConditionLabel(BrickLink::SubCondition sc) const
{
    switch (sc) {
    case BrickLink::SubCondition::None      : return tr("-", "no subcondition");
    case BrickLink::SubCondition::Sealed    : return tr("Sealed");
    case BrickLink::SubCondition::Complete  : return tr("Complete");
    case BrickLink::SubCondition::Incomplete: return tr("Incomplete");
    default                                 : return QString();
    }
}




DocumentProxyModel::DocumentProxyModel(Document *model, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_lastSortColumn[0] = m_lastSortColumn[1] = -1;

    setDynamicSortFilter(true);
    setSourceModel(model);

    m_parser = new Filter::Parser();

    languageChange();
}

DocumentProxyModel::~DocumentProxyModel()
{
    delete m_parser;
}

void DocumentProxyModel::setFilterExpression(const QString &str)
{
    bool had_filter = !m_filter.isEmpty();

    m_filter_expression = str;
    m_filter = m_parser->parse(str);

    if (had_filter || !m_filter.isEmpty())
        invalidateFilter();
}

QString DocumentProxyModel::filterExpression() const
{
    return m_filter_expression;
}

QString DocumentProxyModel::filterToolTip() const
{
    return m_parser->toolTip();
}

bool DocumentProxyModel::filterAcceptsColumn(int, const QModelIndex &) const
{
    return true;
}

bool DocumentProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid() || source_row < 0 || source_row >= sourceModel()->rowCount())
        return false;
    else if (m_filter.isEmpty())
        return true;

    bool result = false;
    Filter::Combination nextcomb = Filter::Or;

    for (const Filter &f : m_filter) {
        int firstcol = f.field();
        int lastcol = firstcol;
        if (firstcol < 0) {
            firstcol = 0;
            lastcol = sourceModel()->columnCount() - 1;
        }

        bool localresult = false;
        for (int c = firstcol; c <= lastcol && !localresult; ++c) {
            QVariant v = sourceModel()->data(sourceModel()->index(source_row, c), Qt::EditRole);
            if (v.isNull())
                v = sourceModel()->data(sourceModel()->index(source_row, c), Qt::DisplayRole);
            if (!v.isNull())
                localresult = f.matches(v);
        }
        if (nextcomb == Filter::And)
            result = result && localresult;
        else
            result = result || localresult;

        nextcomb = f.combination();
    }
    return result;
}

bool DocumentProxyModel::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QSortFilterProxyModel::event(e);
}

void DocumentProxyModel::languageChange()
{
    auto model = sourceModel();

    m_parser->setStandardCombinationTokens(Filter::And | Filter::Or);
    m_parser->setStandardComparisonTokens(Filter::Matches | Filter::DoesNotMatch |
                                          Filter::Is | Filter::IsNot |
                                          Filter::Less | Filter::LessEqual |
                                          Filter::Greater | Filter::GreaterEqual |
                                          Filter::StartsWith | Filter::DoesNotStartWith |
                                          Filter::EndsWith | Filter::DoesNotEndWith);

    QMultiMap<int, QString> fields;
    QString str;
    for (int i = 0; i < model->columnCount(QModelIndex()); ++i) {
        str = model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!str.isEmpty())
            fields.insert(i, str);
    }
    fields.insert(-1, tr("Any"));

    m_parser->setFieldTokens(fields);
}

void DocumentProxyModel::sort(int column, Qt::SortOrder order)
{
    m_lastSortColumn[1] = m_lastSortColumn[0];
    m_lastSortColumn[0] = sortColumn();

    QSortFilterProxyModel::sort(column, order);
}

bool DocumentProxyModel::lessThan(const QModelIndex &idx1, const QModelIndex &idx2) const
{
    const Document::Item *i1 = static_cast<Document *>(sourceModel())->item(idx1);
    const Document::Item *i2 = static_cast<Document *>(sourceModel())->item(idx2);

    int result = compare(i1, i2, sortColumn());
    if (!result && m_lastSortColumn[0] > -1)
        result = compare(i1, i2, m_lastSortColumn[0]);
    if (!result && m_lastSortColumn[1] > -1)
        result = compare(i1, i2, m_lastSortColumn[1]);
    return result < 0;
}

static inline int boolCompare(bool b1, bool b2)
{
    return (b1 ? 1 : 0) - (b2 ? 1 : 0);
}

static inline int uintCompare(uint u1, uint u2)
{
    return (u1 == u2) ? 0 : ((u1 < u2) ? -1 : 1);
}

static inline int doubleCompare(double d1, double d2)
{
    return qFuzzyCompare(d1, d2) ? 0 : ((d1 < d2) ? -1 : 1);
}

int DocumentProxyModel::compare(const Document::Item *i1, const Document::Item *i2, int sortColumn)
{
    switch (sortColumn) {
    case Document::Status      : {
        if (i1->counterPart() != i2->counterPart())
            return boolCompare(i1->counterPart(), i2->counterPart());
        else if (i1->alternateId() != i2->alternateId())
            return uintCompare(i1->alternateId(), i2->alternateId());
        else if (i1->alternate() != i2->alternate())
            return boolCompare(i1->alternate(), i2->alternate());
        else
            return int(i1->status()) - int(i2->status());
    }
    case Document::Picture     :
    case Document::PartNo      : return Utility::naturalCompare(i1->itemId(),
                                                                i2->itemId());
    case Document::Description : return Utility::naturalCompare(i1->itemName(),
                                                                i2->itemName());

    case Document::Color       : return i1->colorName().localeAwareCompare(i2->colorName());
    case Document::Category    : return i1->categoryName().localeAwareCompare(i2->categoryName());
    case Document::ItemType    : return i1->itemTypeName().localeAwareCompare(i2->itemTypeName());

    case Document::Comments    : return i1->comments().localeAwareCompare(i2->comments());
    case Document::Remarks     : return i1->remarks().localeAwareCompare(i2->remarks());

    case Document::LotId       : return uintCompare(i1->lotId(), i2->lotId());
    case Document::Quantity    : return i1->quantity() - i2->quantity();
    case Document::Bulk        : return i1->bulkQuantity() - i2->bulkQuantity();
    case Document::Price       : return doubleCompare(i1->price(), i2->price());
    case Document::Total       : return doubleCompare(i1->total(), i2->total());
    case Document::Sale        : return i1->sale() - i2->sale();
    case Document::Condition   : {
        if (i1->condition() == i2->condition())
            return int(i1->subCondition()) - int(i2->subCondition());
        else
            return int(i1->condition()) - int(i2->condition());
    }
    case Document::TierQ1      : return i1->tierQuantity(0) - i2->tierQuantity(0);
    case Document::TierQ2      : return i1->tierQuantity(1) - i2->tierQuantity(1);
    case Document::TierQ3      : return i1->tierQuantity(2) - i2->tierQuantity(2);
    case Document::TierP1      : return doubleCompare(i1->tierPrice(0), i2->tierPrice(0));
    case Document::TierP2      : return doubleCompare(i1->tierPrice(1), i2->tierPrice(1));
    case Document::TierP3      : return doubleCompare(i1->tierPrice(2), i2->tierPrice(2));
    case Document::Retain      : return boolCompare(i1->retain(), i2->retain());
    case Document::Stockroom   : return int(i1->stockroom()) - int(i2->stockroom());
    case Document::Reserved    : return i1->reserved().compare(i2->reserved());
    case Document::Weight      : return doubleCompare(i1->weight(), i2->weight());
    case Document::YearReleased: return i1->itemYearReleased() - i2->itemYearReleased();
    case Document::PriceOrig   : return doubleCompare(i1->origPrice(), i2->origPrice());
    case Document::PriceDiff   : return doubleCompare((i1->price() - i1->origPrice()), (i2->price() - i2->origPrice()));
    case Document::QuantityOrig: return i1->origQuantity() - i2->origQuantity();
    case Document::QuantityDiff: return (i1->quantity() - i1->origQuantity()) - (i2->quantity() - i2->origQuantity());
    }
    return false;
}

class SortItemListCompare {
public:
    SortItemListCompare(const DocumentProxyModel *view, int sortcol, Qt::SortOrder sortorder)
        : m_doc(static_cast<Document *>(view->sourceModel())), m_view(view),
          m_sortcol(sortcol), m_sortasc(sortorder == Qt::AscendingOrder)
    { }

    bool operator()(const Document::Item *i1, const Document::Item *i2)
    {
        bool b = m_view->lessThan(m_doc->index(i1, m_sortcol), m_doc->index(i2, m_sortcol));
        return m_sortasc ? b : !b;
    }
private:
    const Document *m_doc;
    const DocumentProxyModel *m_view;
    int m_sortcol;
    bool m_sortasc;
};

Document::ItemList DocumentProxyModel::sortItemList(const Document::ItemList &list) const
{
    //qWarning("sort in: %d", list.count());
    Document::ItemList result(list);
    std::sort(result.begin(), result.end(), SortItemListCompare(this, sortColumn(), sortOrder()));
    //qWarning("sort out: %d", result.count());
    return result;
}

#include "moc_document.cpp"
