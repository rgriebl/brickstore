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
#include <QtCore/QDir>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QBitArray>
#include <QtGui/QClipboard>
#include <QtGui/QCursor>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>
#include <QAction>
#include <QDebug>

#include "bricklink/cart.h"
#include "bricklink/core.h"
#include "bricklink/order.h"
#include "bricklink/priceguide.h"
#include "bricklink/store.h"
#include "utility/currency.h"
#include "utility/exception.h"
#include "utility/utility.h"
#include "actionmanager.h"
#include "config.h"
#include "document.h"
#include "documentlist.h"
#include "recentfiles.h"
#include "uihelpers.h"

using namespace std::chrono_literals;

#define CONFIG_HEADER qint32(0x3b285931)


template <typename E>
static E nextEnumValue(E current, std::initializer_list<E> values)
{
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (current == values.begin()[i])
            return values.begin()[(i + 1) % values.size()];
    }
    return current;
}


ColumnCmd::ColumnCmd(Document *document, ColumnCmd::Type type, int logical, int oldValue, int newValue)
    : QUndoCommand()
    , m_document(document)
    , m_type(type)
    , m_logical(logical)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
    QString title = document->model()->headerData(logical, Qt::Horizontal).toString();

    if (type == Type::Move)
        setText(qApp->translate("ColumnCmd", "Moved column %1").arg(title));
    else if (type == Type::Hide)
        setText(qApp->translate("ColumnCmd", "Show/hide column %1").arg(title));
    else
        setText(qApp->translate("ColumnCmd", "Resized column %1").arg(title));
}

int ColumnCmd::id() const
{
    return CID_Column;
}

bool ColumnCmd::mergeWith(const QUndoCommand *other)
{
    auto otherColumn = static_cast<const ColumnCmd *>(other);
    if ((otherColumn->m_type == m_type) && (otherColumn->m_logical == m_logical)) {
        m_oldValue = otherColumn->m_oldValue; // actually the new size, but we're swapped already
        return true;
    }
    return false;
}

void ColumnCmd::redo()
{
    switch (m_type) {
    case Type::Move:
        m_document->moveColumnDirect(m_logical, m_newValue);
        break;
    case Type::Resize:
        m_document->resizeColumnDirect(m_logical, m_newValue);
        break;
    case Type::Hide:
        m_document->hideColumnDirect(m_logical, m_newValue);
        break;
    }
    std::swap(m_oldValue, m_newValue);
}

void ColumnCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnLayoutCmd::ColumnLayoutCmd(Document *document, const QVector<ColumnData> &columnData)
    : QUndoCommand(qApp->translate("ColumnLayoutCmd", "Changed column layout"))
    , m_document(document)
    , m_columnData(columnData)
{ }

int ColumnLayoutCmd::id() const
{
    return CID_ColumnLayout;
}

bool ColumnLayoutCmd::mergeWith(const QUndoCommand *)
{
    return false;
}

void ColumnLayoutCmd::redo()
{
    m_document->setColumnLayoutDirect(m_columnData);
}

void ColumnLayoutCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Document::Document(QObject *parent)
    : Document(new DocumentModel(), parent)
{ }

Document::Document(DocumentModel *model, QObject *parent)
    : Document(model, QByteArray { }, parent)
{ }

Document::Document(DocumentModel *model, BrickLink::Order *order, QObject *parent)
    : Document(model, QByteArray { }, parent)
{
    m_order = order;
    if (order)
        setTitle(tr("Order %1 (%2)").arg(order->id(), order->otherParty()));
}

Document::Document(DocumentModel *model, const QByteArray &columnsState, bool restoredFromAutosave,
                   QObject *parent)
    : Document(model, columnsState, parent)
{
    m_restoredFromAutosave = restoredFromAutosave;
}

Document::Document(DocumentModel *model, const QByteArray &columnsState, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_selectionModel(new QItemSelectionModel(m_model, this))
    , m_uuid(QUuid::createUuid())
{
    setTitle(tr("Untitled"));

    // start with a 1:1 logical to visual mapping for the columns
    m_columnData.resize(m_model->columnCount());
    for (int li = 0; li < m_columnData.size(); ++li)
        m_columnData[li] = ColumnData { 20, li, false };

    // relay signals for the QML API
    connect(model, &DocumentModel::currencyCodeChanged,
            this, &Document::currencyCodeChanged);
    connect(model, &DocumentModel::lotCountChanged,
            this, &Document::lotCountChanged);

    connect(m_selectionModel, &QItemSelectionModel::selectionChanged,
            this, &Document::updateSelection);
    connect(this, &Document::selectedLotsChanged,
            ActionManager::inst(), &ActionManager::setSelection);

    connect(Config::inst(), &Config::showInputErrorsChanged,
            this, &Document::updateItemFlagsMask);
    connect(Config::inst(), &Config::showDifferenceIndicatorsChanged,
            this, &Document::updateItemFlagsMask);

    connect(m_model, &DocumentModel::dataChanged,
            this, &Document::documentDataChanged);

    connect(BrickLink::core(), &BrickLink::Core::priceGuideUpdated,
            this, &Document::priceGuideUpdated);

    updateItemFlagsMask();

    connect(model->undoStack(), &QUndoStack::indexChanged,
               this, [this]() { m_autosaveClean = false; });
    connect(&m_autosaveTimer, &QTimer::timeout,
            this, &Document::autosave);
    m_autosaveTimer.start(1min);


    try {
        auto [columnData, sortColumns] = parseColumnsState(columnsState);
        { } // { } to work around QtCreator being confused by the [] return tuple
        setColumnLayoutDirect(columnData);
    } catch (const Exception &) {
        auto layout = Config::inst()->columnLayout(columnLayoutCommandId(ColumnLayoutCommand::UserDefault));

        try {
            auto [columnData, sortColumns] = parseColumnsState(layout);
            { } // { } to work around QtCreator being confused by the [] return tuple
            setColumnLayoutDirect(columnData);
        } catch (const Exception &) {
            resizeColumnsToDefault();
        }
    }

    m_actionTable = {
        { "edit_cut", [this]() { cut(); } },
        { "edit_copy", [this]() { copy(); } },
        { "edit_duplicate", [this]() { duplicate(); } },
        { "edit_delete", [this]() { remove(); } },
        { "edit_select_all", [this]() { selectAll(); } },
        { "edit_select_none", [this]() { selectNone(); } },
        { "edit_filter_from_selection", [this]() { setFilterFromSelection(); } },
        { "edit_status_include", [this]() { setStatus(BrickLink::Status::Include); } },
        { "edit_status_exclude", [this]() { setStatus(BrickLink::Status::Exclude); } },
        { "edit_status_extra", [this]() { setStatus(BrickLink::Status::Extra); } },
        { "edit_status_toggle", [this]() { toggleStatus(); } },
        { "edit_cond_new", [this]() { setCondition(BrickLink::Condition::New); } },
        { "edit_cond_used", [this]() { setCondition(BrickLink::Condition::Used); } },
        { "edit_subcond_none", [this]() { setSubCondition(BrickLink::SubCondition::None); } },
        { "edit_subcond_sealed", [this]() { setSubCondition(BrickLink::SubCondition::Sealed); } },
        { "edit_subcond_complete", [this]() { setSubCondition(BrickLink::SubCondition::Complete); } },
        { "edit_subcond_incomplete", [this]() { setSubCondition(BrickLink::SubCondition::Incomplete); } },
        { "edit_retain_yes", [this]() { setRetain(true); } },
        { "edit_retain_no", [this]() { setRetain(false); } },
        { "edit_retain_toggle", [this]() { toggleRetain(); } },
        { "edit_stockroom_no", [this]() { setStockroom(BrickLink::Stockroom::None); } },
        { "edit_stockroom_a", [this]() { setStockroom(BrickLink::Stockroom::A); } },
        { "edit_stockroom_b", [this]() { setStockroom(BrickLink::Stockroom::B); } },
        { "edit_stockroom_c", [this]() { setStockroom(BrickLink::Stockroom::C); } },
        { "edit_price_set", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto d = co_await UIHelpers::getDouble(tr("Enter the new price for all selected items:"),
                                                         m_model->currencyCode(),
                                                         selectedLots().front()->price(),
                                                         0, DocumentModel::maxPrice, 3)) {
                  setPrice(*d);
              }
          } },
        { "edit_price_round", [this]() { roundPrice(); } },
        { "edit_cost_set", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto d = co_await UIHelpers::getDouble(tr("Enter the new cost for all selected items:"),
                                                         m_model->currencyCode(),
                                                         selectedLots().front()->cost(),
                                                         0, DocumentModel::maxPrice, 3)) {
                  setCost(*d);
              }
          } },
        { "edit_cost_round", [this]() { roundCost(); } },
        { "edit_cost_spread", [this]() -> QCoro::Task<> {
              Q_ASSERT(selectedLots().size() >= 2);
              if (auto d = co_await UIHelpers::getDouble(tr("Enter the cost amount to spread over all the selected items:"),
                                                         m_model->currencyCode(), 0,
                                                         0, DocumentModel::maxPrice, 3)) {
                  spreadCost(*d);
              }
          } },
        { "edit_qty_divide", [this]() -> QCoro::Task<> {
              if (auto i = co_await UIHelpers::getInteger(tr("Divide the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisible without reminder, before this operation is performed.)"),
                                                          QString(), 1, 1, 1000)) {
                  divideQuantity(*i);
              }
          } },
        { "edit_qty_multiply", [this]() -> QCoro::Task<> {
              if (auto i = co_await UIHelpers::getInteger(tr("Multiply the quantities of all selected items with this factor."),
                                                          tr("x"), 1, -1000, 1000)) {
                  multiplyQuantity(*i);
              }
          } },
        { "edit_bulk", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto i = co_await UIHelpers::getInteger(tr("Set bulk quantity for the selected items:"),
                                                          QString(),
                                                          selectedLots().front()->bulkQuantity(),
                                                          1, 99999)) {
                  setBulkQuantity(*i);
              }
          } },
        { "edit_sale", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto i = co_await UIHelpers::getInteger(tr("Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed."),
                                                          tr("%"), selectedLots().front()->sale(),
                                                          -1000, 99)) {
                  setSale(*i);
              }
          } },
        { "edit_qty_set", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto i = co_await UIHelpers::getInteger(tr("Enter the new quantities for all selected items:"),
                                                          QString(), selectedLots().front()->quantity(),
                                                          -DocumentModel::maxQuantity, DocumentModel::maxQuantity)) {
                  setQuantity(*i);
              }
          } },
        { "edit_remark_set", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto s = co_await UIHelpers::getString(tr("Enter the new remark for all selected items:"),
                                                         selectedLots().front()->remarks())) {
                  setRemarks(*s);
              }
          } },
        { "edit_remark_clear", [this]() { setRemarks({ }); } },
        { "edit_remark_add", [this]() -> QCoro::Task<> {
              if (auto s = co_await UIHelpers::getString(tr("Enter the text, that should be added to the remarks of all selected items:"))) {
                  addRemarks(*s);
              }
          } },
        { "edit_remark_rem", [this]() -> QCoro::Task<> {
              if (auto s = co_await UIHelpers::getString(tr("Enter the text, that should be removed from the remarks of all selected items:"))) {
                  removeRemarks(*s);
              }
          } },
        { "edit_comment_set", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto s = co_await UIHelpers::getString(tr("Enter the new comment for all selected items:"),
                                                         selectedLots().front()->comments())) {
                  setComments(*s);
              }
          } },
        { "edit_comment_clear", [this]() { setComments({ }); } },
        { "edit_comment_add", [this]() -> QCoro::Task<> {
              if (auto s = co_await UIHelpers::getString(tr("Enter the text, that should be added to the comments of all selected items:"))) {
                  addComments(*s);
              }
          } },
        { "edit_comment_rem", [this]() -> QCoro::Task<> {
              if (auto s = co_await UIHelpers::getString(tr("Enter the text, that should be removed from the comments of all selected items:"))) {
                  removeComments(*s);
              }
          } },
        { "edit_reserved", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto s = co_await UIHelpers::getString(tr("Reserve all selected items for this specific buyer (BrickLink username):"),
                                                         selectedLots().front()->reserved())) {
                  setReserved(*s);
              }
          } },
        { "edit_marker_text", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto s = co_await UIHelpers::getString(tr("Enter the new marker text for all selected items:"),
                                                         selectedLots().front()->markerText())) {
                  setMarkerText(*s);
              }
          } },
        { "edit_marker_color", [this]() -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              if (auto c = co_await UIHelpers::getColor(selectedLots().front()->markerColor())) {
                  setMarkerColor(*c);
              }
          } },
        { "edit_marker_clear", [this]() { clearMarker(); } },

        { "view_reset_diff_mode", [this]() { resetDifferenceMode(); } },
        { "view_goto_next_diff", [this]() { gotoNextErrorOrDifference(true); } },
        { "view_goto_next_input_error", [this]() { gotoNextErrorOrDifference(false); } },
        { "view_column_layout_save", [this]() { saveCurrentColumnLayout(); } },

        { "document_save", [this]() { save(false); } },
        { "document_save_as", [this]() { save(true); } },
        { "document_export_bl_xml", [this]() { exportBrickLinkXMLToFile(); } },
        { "document_export_bl_xml_clip", [this]() { exportBrickLinkXMLToClipboard(); } },
        { "document_export_bl_update_clip", [this]() { exportBrickLinkUpdateXMLToClipboard(); } },
        { "document_export_bl_invreq_clip", [this]() { exportBrickLinkInventoryRequestToClipboard(); } },
        { "document_export_bl_wantedlist_clip", [this]() { exportBrickLinkWantedListToClipboard(); } },

        { "bricklink_catalog", [this]() {
              Q_ASSERT(!selectedLots().isEmpty());
              const auto *lot = selectedLots().constFirst();
              BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, lot->item(), lot->color());
          } },
        { "bricklink_priceguide", [this]() {
              Q_ASSERT(!selectedLots().isEmpty());
              const auto *lot = selectedLots().constFirst();
              BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, lot->item(), lot->color());
          } },
        { "bricklink_lotsforsale", [this]() {
              Q_ASSERT(!selectedLots().isEmpty());
              const auto *lot = selectedLots().constFirst();
              BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, lot->item(), lot->color());
          } },
        { "bricklink_myinventory", [this]() {
              Q_ASSERT(!selectedLots().isEmpty());
              const auto *lot = selectedLots().constFirst();
              uint lotid = lot->lotId();
              if (lotid)
              BrickLink::core()->openUrl(BrickLink::URL_StoreItemDetail, &lotid);
              else
              BrickLink::core()->openUrl(BrickLink::URL_StoreItemSearch, lot->item(), lot->color());
          } },
    };

    QMetaObject::invokeMethod(this, [this]() { DocumentList::inst()->add(this); }, Qt::QueuedConnection);
}


Document::~Document()
{
    DocumentList::inst()->remove(this);

    delete m_order;

    m_autosaveTimer.stop();
    deleteAutosave();

    delete m_model;
    //qWarning() << "~" << this;
}

void Document::ref()
{
    m_ref.ref();
}

void Document::deref()
{
    if (!m_ref.deref())
        deleteLater();
}

int Document::refCount() const
{
    return m_ref;
}

void Document::setActive(bool active)
{
    if (active) {
        m_actionConnectionContext = new QObject(this);

        for (auto &at : qAsConst(m_actionTable)) {
            if (QAction *a = ActionManager::inst()->qAction(at.first)) {
                QObject::connect(a, &QAction::triggered, m_actionConnectionContext, at.second);
            }
        }

        if (!m_hasBeenActive) {
            if (!isRestoredFromAutosave()) {
                QStringList messages;
                if (m_model->invalidLotCount()) {
                    messages << tr("This file contains %n unknown item(s).",
                                   nullptr, m_model->invalidLotCount());
                }
                if (m_model->fixedLotCount()) {
                    messages << tr("%n oudated item or color reference(s) in this file have been updated according to the BrickLink catalog.",
                                   nullptr, m_model->fixedLotCount());
                }
                if (m_model->legacyCurrencyCode() && (Config::inst()->defaultCurrencyCode() != "USD"_l1)) {
                    messages << tr("You have loaded an old style document that does not have any currency information attached. You can convert this document to include this information by using the currency code selector in the top right corner.");
                }

                if (!messages.isEmpty()) {
                    const QString msg = u"<b>" % fileNameOrTitle() % u"</b><br><ul><li>"
                            % messages.join("</li><li>"_l1) % u"</li></ul>";

                    static auto notifyUser = [](QString s) -> QCoro::Task<> {
                        co_await UIHelpers::information(s);
                    };
                    QMetaObject::invokeMethod(this, [=]() { notifyUser(msg); }, Qt::QueuedConnection);
                }
            }
            m_hasBeenActive = true;
        }
    } else {
        delete m_actionConnectionContext;
        m_actionConnectionContext = nullptr;
    }
    m_model->undoStack()->setActive(active);
}

QString Document::fileName() const
{
    return m_filename;
}

void Document::setFileName(const QString &str)
{
    if (str != m_filename) {
        m_filename = str;
        emit fileNameChanged(str);

        setTitle({ });
    }
}

QString Document::fileNameOrTitle() const
{
    QFileInfo fi(m_filename);
    if (fi.exists())
        return QDir::toNativeSeparators(fi.absoluteFilePath());

    if (!m_title.isEmpty())
        return m_title;

    return m_filename;
}

QString Document::title() const
{
    return m_title;
}

void Document::setTitle(const QString &str)
{
    if (str != m_title) {
        m_title = str;
        emit titleChanged(m_title);
    }
}

QModelIndex Document::currentIndex() const
{
    return m_selectionModel->currentIndex();
}

void Document::documentDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
        auto lot = m_model->lot(topLeft.siblingAtRow(r));
        if (m_selectedLots.contains(lot)) {
            emit selectedLotsChanged(m_selectedLots);
            break;
        }
    }
}

void Document::moveColumnDirect(int logical, int newVisual)
{
    Q_ASSERT(logical >= 0 && logical < m_columnData.size());
    Q_ASSERT(newVisual >= 0 && newVisual < m_columnData.size());

    int oldVisual = m_columnData.at(logical).m_visualIndex;

    if (newVisual == oldVisual)
        return;

    QVector<int> v2l;
    v2l.resize(m_columnData.size());
    for (int li = 0; li < m_columnData.size(); ++li)
        v2l[m_columnData.at(li).m_visualIndex] = li;

    if (newVisual > oldVisual) {
        // append
        std::rotate(v2l.begin() + oldVisual, v2l.begin() + oldVisual + 1, v2l.begin() + newVisual + 1);
    } else {
        // prepend
        std::rotate(v2l.begin() + newVisual, v2l.begin() + oldVisual, v2l.begin() + oldVisual + 1);
    }

    for (int vi = 0; vi < m_columnData.size(); ++vi)
        m_columnData[v2l.at(vi)].m_visualIndex = vi;

    emit columnPositionChanged(logical, newVisual);
}

void Document::resizeColumnDirect(int logical, int newSize)
{
    Q_ASSERT(logical >= 0 && logical < m_columnData.size());

    if (m_columnData.value(logical).m_size != newSize) {
        m_columnData[logical].m_size = newSize;
        emit columnSizeChanged(logical, newSize);
    }
}

void Document::hideColumnDirect(int logical, bool newHidden)
{
    Q_ASSERT(logical >= 0 && logical < m_columnData.size());

    if (m_columnData.value(logical).m_hidden != newHidden) {
        m_columnData[logical].m_hidden = newHidden;
        emit columnHiddenChanged(logical, newHidden);
    }
}

void Document::setColumnLayoutDirect(QVector<ColumnData> &columnData)
{
    auto current = m_columnData;

//    QBitArray check(columnData.size(), false);
//    for (int i = 0; i < columnData.size(); ++i)
//        check.toggleBit(columnData.at(i).m_visualIndex);
//    if (check.count(true) != columnData.size()) {
//        qWarning() << "Invalid column data!";
//        return;
//    }

    // we need to move the columns into their (visual) place from left to right
    for (int vi = 0; vi < DocumentModel::FieldCount; ++vi) {
        int li;
        for (li = 0; li < columnData.count(); ++li) {
            if (columnData.value(li).m_visualIndex == vi)
                break;
        }
        if ((li < 0) || (li >= columnData.count())) {
            continue;
        }

        const auto &newData = columnData.value(li);

//        qWarning() << "Col:" << DocumentModel::Field(li)
//                   << "move to" << newData.m_visualIndex
//                   << " - resize to" << newData.m_size
//                   << " - hide?" << newData.m_hidden;

        moveColumnDirect(li, newData.m_visualIndex);
        resizeColumnDirect(li, newData.m_size);
        hideColumnDirect(li, newData.m_hidden);
    }
    // hide columns that got added after this was saved
    for (int li = columnData.count(); li < DocumentModel::FieldCount; ++li) {
        moveColumnDirect(li, li);
        resizeColumnDirect(li, current.value(li).m_size);
        hideColumnDirect(li, true);
    }

    emit columnLayoutChanged();
}

QVector<ColumnData> Document::columnLayout() const
{
    return m_columnData;
}


void Document::updateItemFlagsMask()
{
    quint64 em = 0;
    quint64 dm = 0;

    if (Config::inst()->showInputErrors())
        em = (1ULL << DocumentModel::FieldCount) - 1;
    if (Config::inst()->showDifferenceIndicators())
        dm = (1ULL << DocumentModel::FieldCount) - 1;

    m_model->setLotFlagsMask({ em, dm });
}

QString Document::actionText() const
{
    if (auto a = qobject_cast<QAction *>(sender()))
        return a->text();
    return { };
}

void Document::resetDifferenceMode()
{
    m_model->resetDifferenceMode(selectedLots());
}

void Document::cut()
{
    if (!m_selectedLots.isEmpty()) {
        QGuiApplication::clipboard()->setMimeData(new DocumentLotsMimeData(m_selectedLots));
        m_model->removeLots(m_selectedLots);
    }
}

void Document::copy()
{
    if (!m_selectedLots.isEmpty())
        QGuiApplication::clipboard()->setMimeData(new DocumentLotsMimeData(m_selectedLots));
}

void Document::duplicate()
{
    if (selectedLots().isEmpty())
        return;

    QItemSelection newSelection;
    const QModelIndex oldCurrentIdx = m_selectionModel->currentIndex();
    QModelIndex newCurrentIdx;

    applyTo(selectedLots(), [&](const auto &from, auto &to) {
        auto *lot = new Lot(from);
        m_model->insertLotsAfter(&from, { lot });
        QModelIndex idx = m_model->index(lot);
        newSelection.merge(QItemSelection(idx, idx), QItemSelectionModel::Select);
        if (m_model->lot(oldCurrentIdx) == &from)
            newCurrentIdx = m_model->index(idx.row(), oldCurrentIdx.column());

        // this isn't necessary and we should just return false, but the counter would be wrong then
        to = from;
        return true;
    });

    m_selectionModel->select(newSelection,
                             QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (newCurrentIdx.isValid())
        m_selectionModel->setCurrentIndex(newCurrentIdx, QItemSelectionModel::Current);
}

void Document::remove()
{
    if (!m_selectedLots.isEmpty())
        m_model->removeLots(m_selectedLots);
}

void Document::selectAll()
{
    QItemSelection all(m_model->index(0, 0),
                       m_model->index(m_model->rowCount() - 1, m_model->columnCount() - 1));

    m_selectionModel->select(all, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void Document::selectNone()
{
    m_selectionModel->clearSelection();
}

void Document::setFilterFromSelection()
{
    if (selectedLots().count() != 1)
        return;

    auto idx = m_selectionModel->currentIndex();
    if (idx.isValid() && idx.column() >= 0) {
        QVariant v = idx.data(DocumentModel::FilterRole);
        QString s;
        static QLocale loc;

        switch (v.userType()) {
        case QMetaType::Double : s = loc.toString(v.toDouble(), 'f', 3); break;
        case QMetaType::Int    : s = loc.toString(v.toInt()); break;
        default:
        case QMetaType::QString: s = v.toString(); break;
        }
        if (idx.column() == DocumentModel::Weight)
            s = Utility::weightToString(v.toDouble(), Config::inst()->measurementSystem());

        Filter f;
        f.setField(idx.column());
        f.setComparison(Filter::Comparison::Is);
        f.setExpression(s);

        m_model->setFilter({ f });
    }
}

void Document::toggleStatus()
{
    applyTo(m_selectedLots, [](const auto &from, auto &to) {
        (to = from).setStatus(nextEnumValue(from.status(), { BrickLink::Status::Include,
                                                             BrickLink::Status::Exclude }));
        return true;
    });
}

void Document::setStatus(BrickLink::Status status)
{
    applyTo(selectedLots(), [status](const auto &from, auto &to) {
        (to = from).setStatus(status); return true;
    });
}
void Document::setCondition(BrickLink::Condition condition)
{
    applyTo(selectedLots(), [condition](const auto &from, auto &to) {
        (to = from).setCondition(condition);
        return true;
    });
}

void Document::setSubCondition(BrickLink::SubCondition subCondition)
{
    applyTo(selectedLots(), [subCondition](const auto &from, auto &to) {
        (to = from).setSubCondition(subCondition); return true;
    });
}

void Document::toggleRetain()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setRetain(!from.retain()); return true;
    });
}

void Document::setRetain(bool retain)
{
    applyTo(selectedLots(), [retain](const auto &from, auto &to) {
        (to = from).setRetain(retain); return true;
    });
}

void Document::setStockroom(BrickLink::Stockroom stockroom)
{
    m_model->applyTo(selectedLots(), [stockroom](const auto &from, auto &to) {
        (to = from).setStockroom(stockroom); return true;
    });
}

void Document::setPrice(double price)
{
    applyTo(selectedLots(), [price](const auto &from, auto &to) {
        (to = from).setPrice(price); return true;
    });
}

void Document::setPriceToGuide(BrickLink::Time time, BrickLink::Price price, bool forceUpdate)
{
    Q_ASSERT(m_setToPG.isNull());
    const auto sel = selectedLots();

    Q_ASSERT(!isBlockingOperationActive());
    startBlockingOperation(tr("Loading price guide data from disk"));

    m_model->beginMacro();

    m_setToPG.reset(new SetToPriceGuideData);
    m_setToPG->changes.reserve(sel.size());
    m_setToPG->totalCount = sel.count();
    m_setToPG->doneCount = 0;
    m_setToPG->time = time;
    m_setToPG->price = price;
    m_setToPG->currencyRate = Currency::inst()->rate(m_model->currencyCode());

    for (Lot *item : sel) {
        BrickLink::PriceGuide *pg = BrickLink::core()->priceGuide(item->item(), item->color());

        if (pg && (forceUpdate || !pg->isValid())
                && (pg->updateStatus() != BrickLink::UpdateStatus::Updating)) {
            pg->update();
        }

        if (pg && ((pg->updateStatus() == BrickLink::UpdateStatus::Loading)
                   || (pg->updateStatus() == BrickLink::UpdateStatus::Updating))) {
            m_setToPG->priceGuides.insert(pg, item);
            pg->addRef();

        } else if (pg && pg->isValid()) {
            double price = pg->price(m_setToPG->time, item->condition(), m_setToPG->price)
                    * m_setToPG->currencyRate;

            if (!qFuzzyCompare(price, item->price())) {
                Lot newItem = *item;
                newItem.setPrice(price);
                m_setToPG->changes.emplace_back(item, newItem);
            }
            ++m_setToPG->doneCount;
            emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);

        } else {
            Lot newItem = *item;
            newItem.setPrice(0);
            m_setToPG->changes.emplace_back(item, newItem);

            ++m_setToPG->failCount;
            ++m_setToPG->doneCount;
            emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);
        }
    }

    setBlockingOperationTitle(tr("Downloading price guide data from BrickLink"));
    setBlockingOperationCancelCallback(std::bind(&Document::cancelPriceGuideUpdates, this));

    if (m_setToPG->priceGuides.isEmpty())
        priceGuideUpdated(nullptr);
}

void Document::priceGuideUpdated(BrickLink::PriceGuide *pg)
{
    if (m_setToPG && pg) {
        const auto lots = m_setToPG->priceGuides.values(pg);

        if (lots.isEmpty())
            return; // not a PG requested by us
        if (pg->updateStatus() == BrickLink::UpdateStatus::Updating)
            return; // loaded now, but still needs an online update

        for (auto lot : lots) {
            if (!m_setToPG->canceled) {
                double price = pg->isValid() ? (pg->price(m_setToPG->time, lot->condition(),
                                                          m_setToPG->price) * m_setToPG->currencyRate)
                                             : 0;

                if (!qFuzzyCompare(price, lot->price())) {
                    Lot newLot = *lot;
                    newLot.setPrice(price);
                    m_setToPG->changes.emplace_back(lot, newLot);
                }
            }
            pg->release();
        }

        m_setToPG->doneCount += lots.size();
        emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);

        if (!pg->isValid() || (pg->updateStatus() == BrickLink::UpdateStatus::UpdateFailed))
            m_setToPG->failCount += lots.size();
        m_setToPG->priceGuides.remove(pg);
    }

    if (m_setToPG && m_setToPG->priceGuides.isEmpty()
            && (m_setToPG->doneCount == m_setToPG->totalCount)) {
        int failCount = m_setToPG->failCount;
        int successCount = m_setToPG->totalCount - failCount;
        m_model->changeLots(m_setToPG->changes);
        m_setToPG.reset();

        m_model->endMacro(tr("Set price to guide on %n item(s)", nullptr, successCount));

        endBlockingOperation();

        QString s = tr("Prices of the selected items have been updated to Price Guide values.");
        if (failCount) {
            s = s % u"<br><br>" % tr("%1 have been skipped, because of missing Price Guide records or network errors.")
                    .arg(CMB_BOLD(QString::number(failCount)));
        }

        emit requestActivation();
        UIHelpers::information(s);
    }
}

void Document::cancelPriceGuideUpdates()
{
    if (m_setToPG) {
        m_setToPG->canceled = true;
        const auto pgs = m_setToPG->priceGuides.uniqueKeys();
        for (BrickLink::PriceGuide *pg : pgs) {
            if (pg->updateStatus() == BrickLink::UpdateStatus::Updating)
                pg->cancelUpdate();
        }
    }
}


void Document::roundPrice()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        double price = int(from.price() * 100 + .5) / 100.;
        if (!qFuzzyCompare(price, from.price())) {
            (to = from).setPrice(price);
            return true;
        }
        return false;
    });
}

void Document::priceAdjust(bool isFixed, double value, bool applyToTiers)
{
    if (qFuzzyIsNull(value))
        return;

    double factor = isFixed ? 0 : (1.+ value / 100.);

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        double price = from.price();

        if (isFixed)
            price += value;
        else
            price *= factor;

        if (!qFuzzyCompare(price, from.price())) {
            (to = from).setPrice(price);

            if (applyToTiers) {
                for (int i = 0; i < 3; i++) {
                    price = from.tierPrice(i);

                    if (isFixed)
                        price += value;
                    else
                        price *= factor;

                    to.setTierPrice(i, price);
                }
            }
            return true;
        }
        return false;
    });
}

void Document::setCost(double cost)
{
    applyTo(m_selectedLots, [cost](const auto &from, auto &to) {
        (to = from).setCost(cost); return true;
    });
}

void Document::spreadCost(double spreadAmount)
{
    double priceTotal = 0;

    for (const Lot *lot : m_selectedLots)
        priceTotal += (lot->price() * lot->quantity());
    if (qFuzzyIsNull(priceTotal))
        return;
    double f = spreadAmount / priceTotal;
    if (qFuzzyCompare(f, 1))
        return;

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        (to = from).setCost(from.price() * f); return true;
    });
}

void Document::roundCost()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        double cost = int(from.cost() * 100 + .5) / 100.;
        if (!qFuzzyCompare(cost, from.cost())) {
            (to = from).setCost(cost);
            return true;
        }
        return false;
    });
}

void Document::costAdjust(bool isFixed, double value)
{
    if (qFuzzyIsNull(value))
        return;

    double factor = isFixed ? 0 : (1.+ value / 100.);

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        double cost = from.cost();

        if (isFixed)
            cost += value;
        else
            cost *= factor;

        if (!qFuzzyCompare(cost, from.cost())) {
            (to = from).setCost(cost);
            return true;
        }
        return false;
    });
}



void Document::divideQuantity(int divisor)
{
    if (divisor <= 1)
        return;

    int lotsNotDivisible = 0;

    for (const Lot *item : m_selectedLots) {
        if (qAbs(item->quantity()) % divisor)
            ++lotsNotDivisible;
    }

    if (lotsNotDivisible) {
        UIHelpers::information(tr("The quantities of %n lot(s) are not divisible without remainder by %1.<br /><br />Nothing has been modified.",
                                  nullptr, lotsNotDivisible).arg(divisor));
        return;
    }

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        (to = from).setQuantity(from.quantity() / divisor); return true;
    });
}

void Document::multiplyQuantity(int factor)
{
    if ((factor == 0) || (factor == 1))
        return;

    int lotsTooLarge = 0;
    int maxQty = DocumentModel::maxQuantity / qAbs(factor);

    for (const Lot *item : m_selectedLots) {
        if (qAbs(item->quantity()) > maxQty)
            lotsTooLarge++;
    }

    if (lotsTooLarge) {
        UIHelpers::information(tr("The quantities of %n lot(s) would exceed the maximum allowed item quantity (%2) when multiplied by %1.<br><br>Nothing has been modified.",
                                  nullptr, lotsTooLarge).arg(factor).arg(DocumentModel::maxQuantity));
        return;
    }

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        (to = from).setQuantity(from.quantity() * factor); return true;
    });
}



void Document::setSale(int sale)
{
    applyTo(selectedLots(), [sale](const auto &from, auto &to) {
        (to = from).setSale(sale); return true;
    });
}

void Document::setBulkQuantity(int qty)
{
    applyTo(selectedLots(), [qty](const auto &from, auto &to) {
        (to = from).setBulkQuantity(qty); return true;
    });
}

void Document::setQuantity(int quantity)
{
    applyTo(selectedLots(), [quantity](const auto &from, auto &to) {
        (to = from).setQuantity(quantity); return true;
    });
}

void Document::setRemarks(const QString &remarks)
{
    applyTo(selectedLots(), [remarks](const auto &from, auto &to) {
        (to = from).setRemarks(remarks); return true;
    });
}

void Document::addRemarks(const QString &addRemarks)
{
    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        to = from;
        Lot tmp = from;
        tmp.setRemarks(addRemarks);

        return DocumentModel::mergeLotFields(tmp, to, DocumentModel::MergeMode::Ignore,
                                             {{ DocumentModel::Remarks, DocumentModel::MergeMode::MergeText }});
    });
}

void Document::removeRemarks(const QString &remRemarks)
{
    QRegularExpression regexp(u"\\b" % QRegularExpression::escape(remRemarks) % u"\\b");

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        QString remarks = from.remarks();
        if (!remarks.isEmpty())
            remarks = remarks.remove(regexp).simplified();
        if (from.remarks() != remarks) {
            (to = from).setRemarks(remarks);
            return true;
        }
        return false;
    });
}

void Document::setComments(const QString &comments)
{
    applyTo(selectedLots(), [comments](const auto &from, auto &to) {
        (to = from).setComments(comments); return true;
    });
}

void Document::addComments(const QString &addComments)
{
    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        to = from;
        Lot tmp = from;
        tmp.setComments(addComments);

        return DocumentModel::mergeLotFields(tmp, to, DocumentModel::MergeMode::Ignore,
                                             {{ DocumentModel::Comments, DocumentModel::MergeMode::MergeText }});
    });
}

void Document::removeComments(const QString &remComments)
{
    QRegularExpression regexp(u"\\b" % QRegularExpression::escape(remComments) % u"\\b");

    applyTo(selectedLots(), [=](const auto &from, auto &to) {
        QString comments = from.comments();
        if (!comments.isEmpty())
            comments = comments.remove(regexp).simplified();
        if (from.comments() != comments) {
            (to = from).setComments(comments);
            return true;
        }
        return false;
    });
}

void Document::setReserved(const QString &reserved)
{
    applyTo(selectedLots(), [reserved](const auto &from, auto &to) {
        (to = from).setReserved(reserved); return true;
    });
}

void Document::setMarkerText(const QString &text)
{
    applyTo(selectedLots(), [text](const auto &from, auto &to) {
        (to = from).setMarkerText(text); return true;
    });
}

void Document::setMarkerColor(const QColor &color)
{
    applyTo(selectedLots(), [color](const auto &from, auto &to) {
        (to = from).setMarkerColor(color); return true;
    });
}

void Document::clearMarker()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setMarkerText({ }); to.setMarkerColor({ }); return true;
    });
}

QCoro::Task<> Document::exportBrickLinkXMLToFile()
{
    LotList lots = co_await exportCheck(ExportToFile);
    if (lots.isEmpty())
        co_return;

    QString fn;
    if (auto f = co_await UIHelpers::getSaveFileName(fn, DocumentIO::nameFiltersForBrickLinkXML(false),
                                                     tr("Export File"))) {
        fn = *f;
    }
    if (fn.isEmpty())
        co_return;

#if !defined(Q_OS_ANDROID)
    if (fn.right(4) != ".xml"_l1)
        fn = fn % ".xml"_l1;
#endif

    const QByteArray xml = BrickLink::IO::toBrickLinkXML(lots).toUtf8();

    QSaveFile f(fn);
    f.setDirectWriteFallback(true);
    try {
        if (!f.open(QIODevice::WriteOnly))
            throw Exception(tr("Failed to open file %1 for writing."));
        if (f.write(xml.data(), xml.size()) != qint64(xml.size()))
            throw Exception(tr("Failed to save data to file %1."));
        if (!f.commit())
            throw Exception(tr("Failed to save data to file %1."));

    } catch (const Exception &e) {
        UIHelpers::warning(e.error().arg(f.fileName()) % u"<br><br>" % f.errorString());
    }
}

QCoro::Task<> Document::exportBrickLinkXMLToClipboard()
{
    LotList lots = co_await exportCheck(ExportToClipboard);

    if (!lots.isEmpty()) {
        QString xml = BrickLink::IO::toBrickLinkXML(lots);

        QGuiApplication::clipboard()->setText(xml, QClipboard::Clipboard);
        if (Config::inst()->openBrowserOnExport())
            BrickLink::core()->openUrl(BrickLink::URL_InventoryUpload);
    }
}

QCoro::Task<> Document::exportBrickLinkUpdateXMLToClipboard()
{
    LotList lots; co_await exportCheck(ExportToClipboard | ExportUpdate);

    if (!lots.isEmpty()) {
        auto warnings = model()->hasDifferenceUpdateWarnings(lots);
        if (!warnings.isEmpty()) {
            QString s = u"<ul><li>" % warnings.join("</li><li>"_l1) % u"</li></ul>";
            s = tr("There are problems: %1Do you really want to export this list?").arg(s);

            if (co_await UIHelpers::question(s) != UIHelpers::Yes)
                co_return;
        }

        auto xml = BrickLink::IO::toBrickLinkUpdateXML(lots, [this](const Lot *lot) {
            return model()->differenceBaseLot(lot);
        });

        QGuiApplication::clipboard()->setText(xml, QClipboard::Clipboard);
        if (Config::inst()->openBrowserOnExport())
            BrickLink::core()->openUrl(BrickLink::URL_InventoryUpdate);
    }
}

QCoro::Task<> Document::exportBrickLinkInventoryRequestToClipboard()
{
    LotList lots = co_await exportCheck(ExportToClipboard);

    if (!lots.isEmpty()) {
        auto xml = BrickLink::IO::toInventoryRequest(lots);

        QGuiApplication::clipboard()->setText(xml, QClipboard::Clipboard);
        if (Config::inst()->openBrowserOnExport())
            BrickLink::core()->openUrl(BrickLink::URL_InventoryRequest);
    }
}

QCoro::Task<> Document::exportBrickLinkWantedListToClipboard()
{
    LotList lots = co_await exportCheck(ExportToClipboard);

    if (!lots.isEmpty()) {
        if (auto wantedList = co_await UIHelpers::getString(tr("Enter the ID number of Wanted List (leave blank for the default Wanted List)"))) {
            auto xml = BrickLink::IO::toWantedListXML(lots, *wantedList);

            QGuiApplication::clipboard()->setText(xml, QClipboard::Clipboard);
            if (Config::inst()->openBrowserOnExport())
                BrickLink::core()->openUrl(BrickLink::URL_WantedListUpload);
        }
    }
}

void Document::moveColumn(int logical, int oldVisual, int newVisual)
{
    model()->undoStack()->push(new ColumnCmd(this, ColumnCmd::Type::Move,
                                             logical, oldVisual, newVisual));
}

void Document::resizeColumn(int logical, int oldSize, int newSize)
{
    model()->undoStack()->push(new ColumnCmd(this, ColumnCmd::Type::Resize,
                                             logical, oldSize, newSize));
}

void Document::hideColumn(int logical, bool oldHidden, int newHidden)
{
    model()->undoStack()->push(new ColumnCmd(this, ColumnCmd::Type::Hide,
                                             logical, oldHidden, newHidden));
}

QCoro::Task<Document *> Document::load(const QString &fileName)
{
    QString fn = fileName;
    if (fn.isEmpty()) {
        if (auto f = co_await UIHelpers::getOpenFileName(DocumentIO::nameFiltersForBrickStoreXML(true),
                                                         tr("Open File"))) {
            fn = *f;
        }
    }
    if (fn.isEmpty())
        co_return nullptr;

    if (auto *existingDocument = DocumentList::inst()->documentForFile(fn)) {
        emit existingDocument->requestActivation();
        co_return existingDocument;
    }

    try {
        auto doc = loadFromFile(fn);
        QMetaObject::invokeMethod(doc, &Document::requestActivation, Qt::QueuedConnection);
        co_return doc;
    } catch (const Exception &e) {
        UIHelpers::warning(e.error());
        co_return nullptr;
    }
}

Document *Document::loadFromFile(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(f.errorString());

    try {
        auto doc = DocumentIO::parseBsxInventory(&f);
        doc->setFileName(fileName);
        RecentFiles::inst()->add(fileName);
        return doc;
    } catch (const Exception &e) {
        throw Exception(tr("Failed to load document %1: %2").arg(fileName).arg(e.error()));
    }
}


QCoro::Task<bool> Document::save(bool saveAs)
{
    QString fn;
    const auto filters = DocumentIO::nameFiltersForBrickStoreXML(false);

    if (saveAs || fileName().isEmpty()) {
        fn = fileName();
        if (fn.right(4) == ".xml"_l1)
            fn.truncate(fn.length() - 4);

        if (auto f = co_await UIHelpers::getSaveFileName(fn, filters, tr("Save File as"), title()))
            fn = *f;
        else
            fn.clear();
    } else if (model()->isModified()) {
        fn = fileName();
    }

    if (!fn.isEmpty()) {
#if !defined(Q_OS_ANDROID)
        QString suffix = filters.at(0).right(5).left(4);

        if (fn.right(4) != suffix)
            fn = fn % suffix;
#endif
        try {
            QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            saveToFile(fn);
            QGuiApplication::restoreOverrideCursor();
            co_return true;

        } catch (const Exception &e) {
            UIHelpers::warning(e.error());
        }
    }
    co_return false;
}


void Document::saveToFile(const QString &fileName)
{
    QSaveFile f(fileName);
    f.setDirectWriteFallback(true);
    if (!f.open(QIODevice::WriteOnly))
        throw Exception(&f, tr("Failed to save document"));

    if (!DocumentIO::createBsxInventory(&f, this) || !f.commit())
        throw Exception(&f, tr("Failed to save document"));

    model()->unsetModified();
    setFileName(fileName);

    RecentFiles::inst()->add(fileName);
    deleteAutosave();
}

Document *Document::fromStore(BrickLink::Store *store)
{
    Q_ASSERT(store);
    Q_ASSERT(store && store->isValid());

    BrickLink::IO::ParseResult pr;
    const auto lots = store->lots();
    for (const auto *lot : lots)
        pr.addLot(new Lot(*lot));
    pr.setCurrencyCode(store->currencyCode());

    auto *document = new Document(new DocumentModel(std::move(pr)));
    document->setTitle(tr("Store %1").arg(QLocale().toString(store->lastUpdated(), QLocale::ShortFormat)));
    return document;
}

Document *Document::fromOrder(BrickLink::Order *order)
{
    Q_ASSERT(order);

    BrickLink::IO::ParseResult pr;
    const auto lots = order->lots();
    for (const auto *lot : lots)
        pr.addLot(new Lot(*lot));
    pr.setCurrencyCode(order->currencyCode());

    return new Document(new DocumentModel(std::move(pr)), order); // Document owns the items now
}

Document *Document::fromCart(BrickLink::Cart *cart)
{
    Q_ASSERT(cart);

    BrickLink::IO::ParseResult pr;
    const auto lots = cart->lots();
    for (const auto *lot : lots)
        pr.addLot(new Lot(*lot));
    pr.setCurrencyCode(cart->currencyCode());

    auto *document = new Document(new DocumentModel(std::move(pr))); // Document owns the items now
    document->setTitle(tr("Cart in store %1").arg(cart->storeName()));
    return document;
}

Document *Document::fromPartInventory(const BrickLink::Item *item,
                                          const BrickLink::Color *color, int multiply,
                                          BrickLink::Condition condition, BrickLink::Status extraParts,
                                          bool includeInstructions)
{
    Q_ASSERT(item);
    Q_ASSERT(item && item->hasInventory());
    Q_ASSERT(item && !item->consistsOf().isEmpty());
    Q_ASSERT(multiply != 0);

    const auto &parts = item->consistsOf();
    BrickLink::IO::ParseResult pr;

    for (const BrickLink::Item::ConsistsOf &part : parts) {
        const BrickLink::Item *partItem = part.item();
        const BrickLink::Color *partColor = part.color();
        if (!partItem)
            continue;
        if (color && color->id() && partItem->itemType()->hasColors()
                && partColor && (partColor->id() == 0)) {
            partColor = color;
        }

        Lot *lot = new Lot(partColor, partItem);
        lot->setQuantity(part.quantity() * multiply);
        lot->setCondition(condition);
        if (part.isExtra())
            lot->setStatus(extraParts);
        lot->setAlternate(part.isAlternate());
        lot->setAlternateId(part.alternateId());
        lot->setCounterPart(part.isCounterPart());

        pr.addLot(std::move(lot));
    }
    if (includeInstructions) {
        if (const auto *instructions = BrickLink::core()->item('I', item->id())) {
            auto *lot = new Lot(BrickLink::core()->color(0), instructions);
            lot->setQuantity(multiply);
            lot->setCondition(condition);

            pr.addLot(std::move(lot));
        }
    }

    auto *document = new Document(new DocumentModel(std::move(pr))); // Document own the items now
    document->setTitle(tr("Inventory for %1").arg(QLatin1String(item->id())));
    return document;
}

BrickLink::Order *Document::order() const
{
    return m_order;
}

QCoro::Task<BrickLink::LotList> Document::exportCheck(int exportCheckMode) const
{
    const LotList lots = selectedLots().isEmpty() ? m_model->lots() : selectedLots();

    if ((lots.size() > 1000) && (exportCheckMode & ExportToClipboard)) {
        if (co_await UIHelpers::question(tr("You have selected more than 1,000 lots, but BrickLink's servers are unable to cope with this many lots at the same time.<br>You should better export multiple, smaller batches.<br><br>Do you want to export this list anyway?"))
                != UIHelpers::Yes) {
            co_return { };
        }
    }

    if (m_model->statistics(lots, true /* ignoreExcluded */, exportCheckMode & ExportUpdate).errors()) {
        if (co_await UIHelpers::question(tr("This list contains lots with errors.<br><br>Do you want to export this list anyway?"))
                != UIHelpers::Yes) {
            co_return { };
        }
    }
    co_return m_model->sortLotList(lots);
}


void Document::updateSelection()
{
    if (!m_delayedSelectionUpdate) {
        m_delayedSelectionUpdate = new QTimer(this);
        m_delayedSelectionUpdate->setSingleShot(true);
        m_delayedSelectionUpdate->setInterval(0);

        connect(m_delayedSelectionUpdate, &QTimer::timeout, this, [this]() {
            m_selectedLots.clear();

            auto sel = m_selectionModel->selectedRows();
            std::sort(sel.begin(), sel.end(), [](const auto &idx1, const auto &idx2) {
                return idx1.row() < idx2.row(); });

            for (const QModelIndex &idx : qAsConst(sel))
                m_selectedLots.append(m_model->lot(idx));

            emit selectedLotsChanged(m_selectedLots);

            if (m_selectionModel->currentIndex().isValid())
                emit ensureVisible(m_selectionModel->currentIndex());
        });
    }
    m_delayedSelectionUpdate->start();
}

void Document::applyTo(const LotList &lots, std::function<bool(const Lot &, Lot &)> callback)
{
    QString actionText;
    if (auto a = qobject_cast<QAction *>(sender()))
        actionText = a->text();
    model()->applyTo(lots, callback, actionText);
}


void Document::copyFields(const LotList &srcLots, DocumentModel::MergeMode defaultMergeMode,
                          const QHash<DocumentModel::Field, DocumentModel::MergeMode> &fieldMergeModes)
{
    if (srcLots.isEmpty())
        return;

    int copyCount = 0;
    std::vector<std::pair<Lot *, Lot>> changes;
    changes.reserve(m_model->lots().size()); // just a guestimate

    model()->beginMacro();

    for (Lot *dstLot : m_model->lots()) {
        for (const auto &srcLot : srcLots) {
            if (!DocumentModel::canLotsBeMerged(*dstLot, *srcLot))
                continue;

            Lot newLot = *dstLot;
            if (DocumentModel::mergeLotFields(*srcLot, newLot, defaultMergeMode, fieldMergeModes)) {
                changes.emplace_back(dstLot, newLot);
                ++copyCount;
                break;
            }
        }
    }
    if (!changes.empty())
        m_model->changeLots(changes);
    m_model->endMacro(tr("Copied or merged %n item(s)", nullptr, copyCount));
}

void Document::subtractItems(const LotList &subLots)
{
    if (subLots.isEmpty())
        return;
    const LotList &lots = model()->lots();

    std::vector<std::pair<Lot *, Lot>> changes;
    changes.reserve(subLots.size() * 2); // just a guestimate
    LotList newLots;

    model()->beginMacro();

    for (const Lot *subLot : subLots) {
        int qty = subLot->quantity();
        if (!subLot->item() || !subLot->color() || !qty)
            continue;

        bool hadMatch = false;

        for (Lot *lot : lots) {
            if (!DocumentModel::canLotsBeMerged(*lot, *subLot))
                continue;

            Lot newItem = *lot;
            auto change = std::find_if(changes.begin(), changes.end(), [=](const auto &c) {
                return c.first == lot;
            });
            Lot &newItemRef = (change == changes.end()) ? newItem : change->second;
            int qtyInItem = newItemRef.quantity();

            if (qtyInItem >= qty) {
                newItemRef.setQuantity(qtyInItem - qty);
                qty = 0;
            } else {
                newItemRef.setQuantity(0);
                qty -= qtyInItem;
            }

            // make sure that this is the last entry in changes, so we can reference it
            // easily below, if a qty is left
            if (&newItemRef == &newItem) {
                changes.emplace_back(lot, newItem);
            } else {
                auto last = std::prev(changes.end());
                if (last != change)
                    std::swap(*change, *last);
            }
            hadMatch = true;

            if (qty == 0)
                break;
        }
        if (qty) {   // still a qty left
            if (hadMatch) {
                Lot &lastChange = changes.back().second;
                lastChange.setQuantity(lastChange.quantity() - qty);
            } else {
                auto newLot = new Lot();
                newLot->setItem(subLot->item());
                newLot->setColor(subLot->color());
                newLot->setCondition(subLot->condition());
                newLot->setSubCondition(subLot->subCondition());
                newLot->setQuantity(-qty);

                newLots << newLot;
            }
        }
    }

    if (!changes.empty())
        model()->changeLots(changes);
    if (!newLots.empty())
        model()->appendLots(std::move(newLots));
    model()->endMacro(tr("Subtracted %n item(s)", nullptr, int(subLots.size())));
}

void Document::gotoNextErrorOrDifference(bool difference)
{
    auto startIdx = m_selectionModel->currentIndex();
    auto skipIdx = startIdx; // skip the field we're on right now...
    if (!startIdx.isValid() || !m_selectionModel->isSelected(startIdx)) {
        startIdx = m_model->index(0, 0);
        skipIdx = { }; // ... but no skipping when we're not anywhere at all
    }

    bool wrapped = false;
    int startCol = startIdx.column();

    for (int row = startIdx.row(); row < m_model->rowCount(); ) {
        if (wrapped && (row > startIdx.row()))
            return;

        auto flags = m_model->lotFlags(m_model->lot(m_model->index(row, 0)));
        quint64 mask = difference ? flags.second : flags.first;
        if (mask) {
            for (int col = startCol; col < m_model->columnCount(); ++col) {
                if (wrapped && (row == startIdx.row()) && (col > startIdx.column()))
                    return;

                if (mask & (1ULL << col)) {
                    auto gotoIdx = m_model->index(row, col);
                    if (skipIdx.isValid() && (gotoIdx == skipIdx)) {
                        skipIdx = { };
                    } else {
                        m_selectionModel->setCurrentIndex(gotoIdx, QItemSelectionModel::Clear
                                                           | QItemSelectionModel::Select
                                                           | QItemSelectionModel::Current
                                                           | QItemSelectionModel::Rows);
                        emit ensureVisible(gotoIdx, true /* center item */);
                        return;
                    }
                }
            }
        } else if (wrapped && (row == startIdx.row())) {
            return;
        }
        startCol = 0;

        ++row;
        if (row >= m_model->rowCount()) { // wrap around
            row = 0;
            wrapped = true;
        }
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


static const struct {
    Document::ColumnLayoutCommand cmd;
    const char *name;
    const char *id;
} columnLayoutCommandList[] = {
{ Document::ColumnLayoutCommand::BrickStoreDefault,
    QT_TRANSLATE_NOOP("Document", "BrickStore default"), "@@brickstore-default" },
{ Document::ColumnLayoutCommand::BrickStoreSimpleDefault,
    QT_TRANSLATE_NOOP("Document", "BrickStore buyer/collector default"), "@@brickstore-simple-default" },
{ Document::ColumnLayoutCommand::AutoResize,
    QT_TRANSLATE_NOOP("Document", "Auto-resize once"), "@@auto-resize" },
{ Document::ColumnLayoutCommand::UserDefault,
    QT_TRANSLATE_NOOP("Document", "User default"), "user-default" },
};

std::vector<Document::ColumnLayoutCommand> Document::columnLayoutCommands()
{
    return { ColumnLayoutCommand::BrickStoreDefault, ColumnLayoutCommand::BrickStoreSimpleDefault,
                ColumnLayoutCommand::AutoResize, ColumnLayoutCommand::UserDefault };
}

QString Document::columnLayoutCommandName(ColumnLayoutCommand clc)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (cmd.cmd == clc)
            return tr(cmd.name);
    }
    return { };
}

QString Document::columnLayoutCommandId(Document::ColumnLayoutCommand clc)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (cmd.cmd == clc)
            return QString::fromLatin1(cmd.id);
    }
    return { };
}

Document::ColumnLayoutCommand Document::columnLayoutCommandFromId(const QString &id)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (id == QLatin1String(cmd.id))
            return cmd.cmd;
    }
    return ColumnLayoutCommand::User;
}

void Document::resizeColumnsToDefault(bool simpleMode)
{
    auto cd = defaultColumnLayout(simpleMode);
    model()->undoStack()->push(new ColumnLayoutCmd(this, cd));
}

QVector<ColumnData> Document::defaultColumnLayout(bool simpleMode)
{
    static const QVector<int> columnOrder {
        DocumentModel::Index,
        DocumentModel::Status,
        DocumentModel::DateAdded,
        DocumentModel::Picture,
        DocumentModel::PartNo,
        DocumentModel::Description,
        DocumentModel::Condition,
        DocumentModel::Color,
        DocumentModel::QuantityOrig,
        DocumentModel::QuantityDiff,
        DocumentModel::Quantity,
        DocumentModel::PriceOrig,
        DocumentModel::PriceDiff,
        DocumentModel::Price,
        DocumentModel::Total,
        DocumentModel::Cost,
        DocumentModel::Bulk,
        DocumentModel::Sale,
        DocumentModel::DateLastSold,
        DocumentModel::Marker,
        DocumentModel::Comments,
        DocumentModel::Remarks,
        DocumentModel::Category,
        DocumentModel::ItemType,
        DocumentModel::TierQ1,
        DocumentModel::TierP1,
        DocumentModel::TierQ2,
        DocumentModel::TierP2,
        DocumentModel::TierQ3,
        DocumentModel::TierP3,
        DocumentModel::LotId,
        DocumentModel::Retain,
        DocumentModel::Stockroom,
        DocumentModel::Reserved,
        DocumentModel::Weight,
        DocumentModel::YearReleased,
    };

    QVector<ColumnData> defaultData;
    defaultData.resize(model()->columnCount());

    int em = QFontMetrics(QGuiApplication::font()).averageCharWidth();
    for (int i = 0; i < model()->columnCount(); i++) {
        int vi = columnOrder.indexOf(i);

        int mw = model()->headerData(i, Qt::Horizontal, DocumentModel::HeaderDefaultWidthRole).toInt();
        int width = (mw < 0 ? -mw : mw * em) + 8;

        defaultData[i] = ColumnData { width, vi, false };
    }

    static const int hiddenColumns[] = {
        DocumentModel::DateAdded,
        DocumentModel::DateLastSold,
        DocumentModel::PriceOrig,
        DocumentModel::PriceDiff,
        DocumentModel::QuantityOrig,
        DocumentModel::QuantityDiff,
    };

    for (auto i : hiddenColumns)
        defaultData[i].m_hidden = true;

    if (simpleMode) {
        static const int hiddenColumnsInSimpleMode[] = {
            DocumentModel::Bulk,
            DocumentModel::Sale,
            DocumentModel::TierQ1,
            DocumentModel::TierQ2,
            DocumentModel::TierQ3,
            DocumentModel::TierP1,
            DocumentModel::TierP2,
            DocumentModel::TierP3,
            DocumentModel::Reserved,
            DocumentModel::Stockroom,
            DocumentModel::Retain,
            DocumentModel::LotId,
            DocumentModel::Comments,
        };

        for (auto i : hiddenColumnsInSimpleMode)
            defaultData[i].m_hidden = true;
    }
    return defaultData;
}

QCoro::Task<> Document::saveCurrentColumnLayout()
{
    if (auto name = co_await UIHelpers::getString(tr("Enter an unique name for this column layout. Leave empty to change the user default layout."))) {
        QString layoutId;
        if (name->isEmpty()) {
            layoutId = columnLayoutCommandId(ColumnLayoutCommand::UserDefault);
        } else {
            const auto allIds = Config::inst()->columnLayoutIds();
            for (const auto &id : allIds) {
                if (Config::inst()->columnLayoutName(id) == *name) {
                    layoutId = id;
                    break;
                }
            }
        }

        QString newId = Config::inst()->setColumnLayout(layoutId, saveColumnsState());
        if (layoutId.isEmpty() && !newId.isEmpty())
            Config::inst()->renameColumnLayout(newId, *name);
    }
}

void Document::setColumnLayoutFromId(const QString &layoutId)
{
    auto clc = columnLayoutCommandFromId(layoutId);
    QString undoName = columnLayoutCommandName(clc);
    QByteArray userLayout;
    if (clc == ColumnLayoutCommand::User) {
        userLayout = Config::inst()->columnLayout(layoutId);
        if (userLayout.isEmpty())
            return;
        undoName = Config::inst()->columnLayoutName(layoutId);
    }

    model()->undoStack()->beginMacro(tr("Set column layout:") % u' ' % undoName);

    switch (clc) {
    case ColumnLayoutCommand::BrickStoreDefault:
    case ColumnLayoutCommand::BrickStoreSimpleDefault:
        resizeColumnsToDefault(clc == ColumnLayoutCommand::BrickStoreSimpleDefault);
        model()->sort(0, Qt::AscendingOrder);
        break;
    case ColumnLayoutCommand::AutoResize:
        emit resizeColumnsToContents();
        break;
    case ColumnLayoutCommand::UserDefault:
        userLayout = Config::inst()->columnLayout(layoutId);
        if (userLayout.isEmpty()) { // use brickstore default
            resizeColumnsToDefault(false);
            model()->sort(0, Qt::AscendingOrder);
            break;
        }
        Q_FALLTHROUGH();
    case ColumnLayoutCommand::User: {
        try {
            auto [columnData, sortColumns] = parseColumnsState(userLayout);
            { } // { } to work around QtCreator being confused by the [] return tuple
            model()->undoStack()->push(new ColumnLayoutCmd(this, columnData));
            model()->sort(sortColumns);
        } catch (const Exception &) {
        }
        break;
    }
    }

    model()->undoStack()->endMacro();
}

QByteArray Document::saveColumnsState() const
{
    QByteArray cl;
    QDataStream ds(&cl, QIODevice::WriteOnly);

    auto sortColumns = model()->sortColumns();

    ds << CONFIG_HEADER << qint32(3) /*version*/
       << qint32(m_columnData.size())
       << qint32(sortColumns.size());

    for (int i = 0; i < sortColumns.size(); ++i) {
       ds << qint32(sortColumns.at(i).first)
          << (sortColumns.at(i).second == Qt::AscendingOrder);
    }

    for (const auto &cd : m_columnData) {
        ds << qint32(cd.m_size)
           << qint32(cd.m_visualIndex)
           << cd.m_hidden;
    }
    return cl;
}

std::tuple<QVector<ColumnData>, QVector<QPair<int, Qt::SortOrder>>> Document::parseColumnsState(const QByteArray &cl)
{
    if (cl.isEmpty())
        throw Exception("");

    QDataStream ds(cl);
    qint32 magic = 0, version = 0, count = -1;

    ds >> magic >> version >> count;

    if ((ds.status() != QDataStream::Ok)
            || (magic != CONFIG_HEADER)
            || (version < 2) || (version > 3)) {
        throw Exception("");
    }

    QVector<QPair<int, Qt::SortOrder>> sortColumns;

    qint32 sortColumnCount = 1;
    if (version == 3)
        ds >> sortColumnCount;
    for (int i = 0; i < sortColumnCount; ++i) {
        qint32 sortIndicator = -1;
        bool sortAscending = false;
        ds >> sortIndicator >> sortAscending;

        if ((sortIndicator >= 0) && (sortIndicator < count)) {
            sortColumns.append(qMakePair(sortIndicator, sortAscending
                                         ? Qt::AscendingOrder : Qt::DescendingOrder));
        }
    }

    QVector<ColumnData> columnData;

    for (int i = 0; i < count; ++i) {
        qint32 size, position;
        bool isHidden;
        ds >> size >> position >> isHidden;

        columnData.insert(i, ColumnData { size, position, isHidden });
    }

    // sanity checks...
    if (ds.status() != QDataStream::Ok)
        throw Exception("");

    QBitArray duplicateVisual(count, false);

    for (int i = 0; i < count; ++i) {
        const auto &cd = columnData.value(i);
        if ((cd.m_size <= 0)
                || (cd.m_size > 5000)
                || (cd.m_visualIndex < 0)
                || (cd.m_visualIndex >= count)
                || duplicateVisual.toggleBit(cd.m_visualIndex))
            throw Exception("");
    }
    return { columnData, sortColumns };

}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


bool Document::isBlockingOperationActive() const
{
    return m_blocked;
}

QString Document::blockingOperationTitle() const
{
    return m_blockTitle;
}

bool Document::isBlockingOperationCancelable() const
{
    return bool(m_blockCancelCallback);
}

void Document::setBlockingOperationCancelCallback(std::function<void ()> cancelCallback)
{
    const bool wasCancelable = isBlockingOperationCancelable();
    m_blockCancelCallback = cancelCallback;
    if (wasCancelable != isBlockingOperationCancelable())
        emit blockingOperationCancelableChanged(!wasCancelable);
}

void Document::setBlockingOperationTitle(const QString &title)
{
    if (title != m_blockTitle) {
        m_blockTitle = title;
        emit blockingOperationTitleChanged(title);
    }
}

void Document::startBlockingOperation(const QString &title, std::function<void ()> cancelCallback)
{
    if (!m_blocked) {
        m_blocked = true;
        setBlockingOperationCancelCallback(cancelCallback);
        setBlockingOperationTitle(title);

        emit blockingOperationActiveChanged(m_blocked);
    }
}

void Document::endBlockingOperation()
{
    if (m_blocked) {
        m_blocked = false;
        setBlockingOperationCancelCallback({ });
        setBlockingOperationTitle({ });

        emit blockingOperationActiveChanged(m_blocked);
    }
}

void Document::cancelBlockingOperation()
{
    if (m_blocked) {
        if (m_blockCancelCallback)
            m_blockCancelCallback();
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


static const char *autosaveMagic = "||BRICKSTORE AUTOSAVE MAGIC||";
static const char *autosaveTemplate = "brickstore_%1.autosave";

bool Document::isRestoredFromAutosave() const
{
    return m_restoredFromAutosave;
}

void Document::deleteAutosave()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString filename = QString::fromLatin1(autosaveTemplate).arg(m_uuid.toString());
    temp.remove(filename);
}

class AutosaveJob : public QRunnable
{
public:
    explicit AutosaveJob(Document *document, const QByteArray &contents)
        : QRunnable()
        , m_document(document)
        , m_uuid(document->m_uuid)
        , m_contents(contents)
    { }

    void run() override;
private:
    QPointer<Document> m_document;
    const QUuid m_uuid;
    const QByteArray m_contents;
};

void AutosaveJob::run()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString fileName = QString::fromLatin1(autosaveTemplate).arg(m_uuid.toString());
    QString newFileName = fileName % u".new";

    { // reading is cheaper than writing, so check first
        QFile f(temp.filePath(fileName));
        if (f.open(QIODevice::ReadOnly)) {
            if (f.readAll() == m_contents)
                return;
        }
    }

    QSaveFile f(temp.filePath(fileName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_contents);
        if (f.commit()) {
            QPointer<Document> document = m_document;
            QMetaObject::invokeMethod(qApp, [=]() {
                if (document)
                    document->m_autosaveClean = true;
            });
        } else {
            qWarning() << "Autosave rename from" << newFileName << "to" << fileName << "failed";
        }
    }
}


void Document::autosave() const
{
    if (m_uuid.isNull() || !model()->isModified() || model()->lots().isEmpty() || m_autosaveClean)
        return;

    const auto lots = m_model->lots();
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray(autosaveMagic)
       << qint32(5) // version
       << title()
       << fileName()
       << m_model->currencyCode()
       << saveColumnsState()
       << model()->saveSortFilterState()
       << qint32(lots.count());

    for (auto lot : lots) {
        lot->save(ds);
        auto base = m_model->differenceBaseLot(lot);
        ds << bool(base);
        if (base)
            base->save(ds);
    }
    ds << QByteArray(autosaveMagic);

    QThreadPool::globalInstance()->start(new AutosaveJob(const_cast<Document *>(this), ba));
}

int Document::restorableAutosaves()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return temp.entryList({ QString::fromLatin1(autosaveTemplate).arg("*"_l1) }).count();
}

int Document::processAutosaves(AutosaveAction action)
{
    int restoredCount = 0;

    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const auto ondisk = temp.entryList({ QString::fromLatin1(autosaveTemplate).arg("*"_l1) });

    for (const QString &filename : ondisk) {
        QFile f(temp.filePath(filename));
        if ((action == AutosaveAction::Restore) && f.open(QIODevice::ReadOnly)) {
            QByteArray magic;
            qint32 version;
            QString savedTitle;
            QString savedFileName;
            QString savedCurrencyCode;
            QByteArray columnState;
            QByteArray savedSortFilterState;
            QHash<const Lot *, Lot> differenceBase;
            qint32 count = 0;

            QDataStream ds(&f);
            ds >> magic >> version;
            if ((magic != QByteArray(autosaveMagic)) || (version != 5))
                continue;
            ds >> savedTitle >> savedFileName >> savedCurrencyCode >> columnState
                    >> savedSortFilterState >> count;

            BrickLink::IO::ParseResult pr;
            pr.setCurrencyCode(savedCurrencyCode);

            if (count > 0) {
                for (int i = 0; i < count; ++i) {
                    if (auto lot = Lot::restore(ds)) {
                        bool hasBase = false;
                        ds >> hasBase;
                        if (hasBase) {
                            if (auto base = Lot::restore(ds)) {
                                pr.addToDifferenceModeBase(lot, *base);
                                delete base;
                            } else {
                                hasBase = false;
                            }
                        }
                        if (!hasBase)
                            pr.addToDifferenceModeBase(lot, *lot);
                        pr.addLot(std::move(lot));
                    }
                }
                ds >> magic;

                if (magic == QByteArray(autosaveMagic)) {
                    QString restoredTag = tr("RESTORED", "Tag for document restored from autosave");

                    // Document owns the items now
                    DocumentIO::BsxContents bsx;
                    auto model = new DocumentModel(std::move(pr), true /*mark as modified*/);
                    model->restoreSortFilterState(savedSortFilterState);
                    Document *doc = new Document(model, columnState, true /* is autosave restore*/);

                    if (!savedFileName.isEmpty()) {
                        QFileInfo fi(savedFileName);
                        QString newFileName = fi.dir().filePath(restoredTag % u" " % fi.fileName());
                        doc->saveToFile(newFileName);
                    } else {
                        doc->setTitle(restoredTag % u" " % savedTitle);
                    }
                    QMetaObject::invokeMethod(doc, &Document::requestActivation, Qt::QueuedConnection);

                    ++restoredCount;
                }
            }
            f.close();
        }
        f.remove();
    }
    return restoredCount;
}


#include "moc_document.cpp"
