// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cmath>

#include <QStyle>
#include <QStyleFactory>
#include <QStyledItemDelegate>
#include <QProxyStyle>
#include <QLayout>
#include <QLabel>
#include <QApplication>
#include <QValidator>
#include <QClipboard>
#include <QCursor>
#include <QTableView>
#include <QScrollArea>
#include <QSizeGrip>
#include <QHeaderView>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QPrinter>
#include <QPrintDialog>
#include <QScrollBar>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QDebug>
#include <QColorDialog>
#include <QProgressBar>
#include <QStackedLayout>
#include <QButtonGroup>
#include <QCompleter>
#include <QStringListModel>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QGridLayout>
#include <QFrame>

#include <QCoro/QCoroSignal>

#include "bricklink/io.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/uihelpers.h"
#include "common/script.h"
#include "utility/exception.h"
#include "documentdelegate.h"
#include "mainwindow.h"
#include "headerview.h"
#include "view.h"
#include "view_p.h"

#include "printdialog.h"
#include "selectdocumentdialog.h"
#include "settopriceguidedialog.h"
#include "incdecpricesdialog.h"
#include "tierpricesdialog.h"
#include "importinventorydialog.h"


using namespace std::chrono_literals;
using namespace std::placeholders;


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnChangeWatcher::ColumnChangeWatcher(View *view, HeaderView *header)
    : QObject(view)
    , m_view(view)
    , m_header(header)
{
    // UI -> Document
    // redirect the signals, so that we can block them while we are applying new values
    connect(header, &HeaderView::sectionMoved,
            this, &ColumnChangeWatcher::internalColumnMoved);
    connect(this, &ColumnChangeWatcher::internalColumnMoved,
            this, [this](int logical, int oldVisual, int newVisual) {
        Q_ASSERT(!m_moving);
        m_moving = true;
        m_view->document()->moveColumn(logical, oldVisual, newVisual);
    });
    connect(header, &HeaderView::sectionResized,
            this, &ColumnChangeWatcher::internalColumnResized);
    connect(this, &ColumnChangeWatcher::internalColumnResized,
            this, [this](int logical, int oldSize, int newSize) {
        if (oldSize && newSize) {
            Q_ASSERT(!m_resizing);
            m_resizing = true;
            m_view->document()->resizeColumn(logical, oldSize, newSize);
        } else {
            Q_ASSERT(!m_hiding);
            m_hiding = true;
            m_view->document()->hideColumn(logical, oldSize == 0, newSize == 0);
        }
    });

    // Document -> UI
    connect(view->document(), &Document::columnPositionChanged,
            this, &ColumnChangeWatcher::moveColumn);
    connect(view->document(), &Document::columnSizeChanged,
            this, &ColumnChangeWatcher::resizeColumn);
    connect(view->document(), &Document::columnHiddenChanged,
            this, &ColumnChangeWatcher::hideColumn);

    // Set initial state in the HeaderView
    m_header->setSortColumns(view->model()->sortColumns());
    auto columnData = view->document()->columnLayout();

    for (int vi = 0; vi < DocumentModel::FieldCount; ++vi) {
        int li;
        for (li = 0; li < columnData.count(); ++li) {
            if (columnData.value(li).m_visualIndex == vi)
                break;
        }
        if (li >= columnData.count()) // ignore columns that we don't know about yet
            continue;

        const auto &data = columnData.value(li);

        moveColumn(li, data.m_visualIndex);
        resizeColumn(li, data.m_size);
        hideColumn(li, data.m_hidden);
    }
    // hide columns that got added after this was saved
    for (int li = int(columnData.count()); li < DocumentModel::FieldCount; ++li)
        hideColumn(li, true);

    QVector<int> v2l, l2v;
    v2l.resize(columnData.count());
    l2v.resize(columnData.count());

    for (int li = 0; li < columnData.size(); ++li) {
        l2v[li] = columnData[li].m_visualIndex;
        v2l[l2v[li]] = li;
    }
}

void ColumnChangeWatcher::moveColumn(int logical, int newVisual)
{
    if (m_moving) {
        m_moving = false;
        return;
    }
    disable();
    m_header->moveSection(m_header->visualIndex(logical), newVisual);
    enable();
}

void ColumnChangeWatcher::resizeColumn(int logical, int newSize)
{
    if (m_resizing) {
        m_resizing = false;
        return;
    }
    disable();
    m_header->resizeSection(logical, newSize);
    enable();
}

void ColumnChangeWatcher::hideColumn(int logical, bool newHidden)
{
    if (m_hiding) {
        m_hiding = false;
        return;
    }
    disable();
    m_header->setSectionHidden(logical, newHidden);
    enable();
}

void ColumnChangeWatcher::enable()
{
    blockSignals(false);
}

void ColumnChangeWatcher::disable()
{
    blockSignals(true);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


TableView::TableView(QWidget *parent)
    : QTableView(parent)
{
    updateMinimumSectionSize();
}

void TableView::editCurrentItem(int column)
{
    auto idx = currentIndex();
    if (!idx.isValid())
        return;
    idx = idx.siblingAtColumn(column);
    QKeyEvent kp(QEvent::KeyPress, Qt::Key_Execute, Qt::NoModifier);
    edit(idx, AllEditTriggers, &kp);
}

void TableView::updateMinimumSectionSize()

{
    int s = std::max(16, fontMetrics().height() * 2);
    horizontalHeader()->setMinimumSectionSize(s);
    setIconSize({ s, s });
}

void TableView::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange)
        updateMinimumSectionSize();
    QTableView::changeEvent(e);
}

void TableView::keyPressEvent(QKeyEvent *e)
{
    // QAbstractItemView thinks it is a good idea to handle 'copy', but we don't want that
    if (e == QKeySequence::Copy) {
        e->ignore();
        return;
    }

    QTableView::keyPressEvent(e);

#if !defined(Q_OS_MACOS)
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (state() != EditingState) {
            if (edit(currentIndex(), EditKeyPressed, e))
                e->accept();
        }
    }
#endif
}

QModelIndex TableView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    // QTableView maps Home/End to left/right, but we want top/bottom instead
    if (cursorAction == MoveHome || cursorAction == MoveEnd)
        modifiers ^= Qt::ControlModifier;

    return QTableView::moveCursor(cursorAction, modifiers);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


void View::setActive(bool active)
{
    Q_ASSERT(active == !m_actionConnectionContext);

    if (active) {
        m_actionConnectionContext = ActionManager::inst()->connectActionTable(m_actionTable);

        connect(m_document, &Document::ensureVisible,
                m_actionConnectionContext, [this](const QModelIndex &idx, bool centerItem) {
            m_table->scrollTo(idx, centerItem ? QAbstractItemView::PositionAtCenter
                                             : QAbstractItemView::EnsureVisible);
        });

    } else {
        delete m_actionConnectionContext;
        m_actionConnectionContext = nullptr;
    }
    m_table->setAutoScroll(active);
}

const QHeaderView *View::headerView() const
{
    return m_header;
}

void View::setLatestRow(int row)
{
    if (row >= 0) {
        m_latest_row = row;
        m_latest_timer->start();
    }
}


View::View(Document *document, QWidget *parent)
    : QWidget(parent)
    , m_document(document)
    , m_sizeGrip(new QSizeGrip(this))
{
    Q_ASSERT(document && document->model());

    //qWarning() << "+" << this;

    m_model = document->model();

    connect(m_document, &Document::closeAllViewsForDocument,
            this, [this]() { delete this; });

    m_latest_row = -1;
    m_latest_timer = new QTimer(this);
    m_latest_timer->setSingleShot(true);
    m_latest_timer->setInterval(100ms);

    m_actionTable = {
        { "edit_partoutitems", [this](bool) { partOutItems(); } },
        { "edit_additems_from_documents", [this](bool) -> QCoro::Task<> {
             SelectCopyMergeDialog dlg(model(),
                                       tr("Select the documents whose items will be appended to the current document"),
                                       QString(), true, this);
             dlg.setWindowModality(Qt::ApplicationModal);
             dlg.show();

             if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
                 m_model->addLots(dlg.lots(*m_model), DocumentModel::AddLotMode::AddAsNew);
             }
         } },
        { "edit_copy_fields", [this](bool) -> QCoro::Task<> {
              SelectCopyMergeDialog dlg(model(),
                                        tr("Select the document that should serve as a source to fill in the corresponding fields in the current document"),
                                        tr("Choose how fields are getting copied or merged."), false, this);
              dlg.setWindowModality(Qt::ApplicationModal);
              dlg.show();

              if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
                  LotList lots = dlg.lots(*m_model);
                  m_document->copyFields(lots, dlg.fieldMergeModes());
                  qDeleteAll(lots);
              }
          } },
        { "edit_subtractitems", [this](bool) -> QCoro::Task<> {
              SelectDocumentDialog dlg(model(), tr("Which items should be subtracted from the current document:"), this);
              dlg.setWindowModality(Qt::ApplicationModal);
              dlg.show();

              if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
                  LotList lots = dlg.lots();
                  if (!lots.empty())
                      m_document->subtractItems(lots);
                  qDeleteAll(lots);
              }
          } },
        { "edit_price_to_priceguide", [this](bool) -> QCoro::Task<> {
              Q_ASSERT(!selectedLots().isEmpty());
              //Q_ASSERT(m_setToPG.isNull());

              SetToPriceGuideDialog dlg(this);
              dlg.setWindowModality(Qt::ApplicationModal);
              dlg.show();

              if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
                  m_document->setPriceToGuide(dlg.time(), dlg.price(), dlg.forceUpdate(),
                                              dlg.noPriceGuideOption());
              }
          } },
        { "edit_price_inc_dec", [this](bool) -> QCoro::Task<> {
              bool showTiers = !m_header->isSectionHidden(DocumentModel::TierQ1);
              IncDecPricesDialog dlg(tr("Increase or decrease the prices of the selected items by"),
                                     showTiers, m_model->currencyCode(), this);
              dlg.setWindowModality(Qt::ApplicationModal);
              dlg.show();

              if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
                  m_document->priceAdjust(dlg.isFixed(), dlg.value(), dlg.applyToTiers());
          } },
        { "edit_tierprice_relative", [this](bool) -> QCoro::Task<> {
             Q_ASSERT(!selectedLots().isEmpty());

             TierPricesDialog dlg(this);
             dlg.setWindowModality(Qt::ApplicationModal);
             dlg.show();

             if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
                 m_document->setRelativeTierPrices(dlg.percentagesOff());
         } },
        { "edit_cost_inc_dec", [this](bool) -> QCoro::Task<> {
              IncDecPricesDialog dlg(tr("Increase or decrease the costs of the selected items by"),
                                     false, m_model->currencyCode(), this);
              dlg.setWindowModality(Qt::ApplicationModal);
              dlg.show();

              if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
                  m_document->costAdjust(dlg.isFixed(), dlg.value());
          } },
        { "edit_color", [this](bool) { m_table->editCurrentItem(DocumentModel::Color); } },
        { "edit_item", [this](bool) { m_table->editCurrentItem(DocumentModel::Description); } },

        { "document_print", [this](bool) { print(false); } },
        { "document_print_pdf", [this](bool) { print(true); } },
    };

    m_table = new TableView(this);
    m_header = new HeaderView(Qt::Horizontal, m_table);
    m_header->setConfigurable(true);
    m_header->setHighlightSections(false);
    m_header->setSortIndicatorShown(false);
    m_table->setHorizontalHeader(m_header);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setAutoFillBackground(false);
    m_table->setTabKeyNavigation(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    setFocusProxy(m_table);

    m_table->setModel(m_model);
    m_table->setSelectionModel(document->selectionModel());

    connect(m_header, &HeaderView::isSortedChanged,
            this, [this](bool b) {
        if (b && !model()->isSorted()) {
            model()->reSort();
            m_table->scrollTo(m_document->currentIndex());
        }
    });
    connect(m_model, &DocumentModel::isSortedChanged,
            m_header, [this](bool b) {
        if (b != m_header->isSorted())
            m_header->setSorted(b);
    });
    m_header->setSorted(m_model->isSorted());

    connect(m_header, &HeaderView::sortColumnsChanged,
            this, [this](const QVector<QPair<int, Qt::SortOrder>> &sortColumns) {
        if (model()->sortColumns() != sortColumns) {
            model()->multiSort(sortColumns);
            m_table->scrollTo(m_document->currentIndex());
        }
    });
    connect(m_model, &DocumentModel::sortColumnsChanged,
            m_header, [this](const QVector<QPair<int, Qt::SortOrder>> &sortColumns) {
        if (m_header->sortColumns() != sortColumns)
            m_header->setSortColumns(sortColumns);
    });
    connect(m_header, &HeaderView::visualColumnOrderChanged,
            this, &View::currentColumnOrderChanged);

    connect(m_document, &Document::resizeColumnsToContents,
            m_table, &QTableView::resizeColumnsToContents);

    auto *dd = new DocumentDelegate(m_table);
    m_table->setItemDelegate(dd);
    m_table->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(m_table));

    m_blockOverlay = new QFrame(this);
    m_blockOverlay->setAutoFillBackground(true);
    m_blockOverlay->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Raised));
    m_blockOverlay->hide();

    auto blockLayout = new QGridLayout(m_blockOverlay);
    blockLayout->setColumnStretch(0, 1);
    m_blockTitle = new QLabel();
    blockLayout->addWidget(m_blockTitle, 0, 0, 1, 2, Qt::AlignCenter);
    m_blockProgress = new QProgressBar();
    blockLayout->addWidget(m_blockProgress, 1, 0);
    m_blockCancel = new QToolButton();
    m_blockCancel->setIcon(QIcon::fromTheme(u"process-stop"_qs));
    m_blockCancel->setProperty("iconScaling", true);
    blockLayout->addWidget(m_blockCancel, 1, 1);

    connect(m_latest_timer, &QTimer::timeout,
            this, &View::ensureLatestVisible);
    connect(m_table, &QWidget::customContextMenuRequested,
            this, &View::contextMenu);

    connect(Config::inst(), &Config::measurementSystemChanged,
            m_table->viewport(), QOverload<>::of(&QWidget::update));

    connect(m_document, &Document::titleChanged,
            this, &View::updateCaption);
    connect(m_document, &Document::filePathChanged,
            this, &View::updateCaption);
    connect(m_model, &DocumentModel::modificationChanged,
            this, &View::updateCaption);

    m_ccw = new ColumnChangeWatcher(this, m_header);

    connect(m_document, &Document::blockingOperationActiveChanged,
            this, [this](bool blocked) {
        if (blocked) {
            m_blockTitle->setText(m_document->blockingOperationTitle());
            m_blockProgress->setRange(0, 0);
        } else {
            m_blockTitle->clear();
            m_blockProgress->reset();
        }
        repositionBlockOverlay();
        m_blockOverlay->setVisible(blocked);
        m_blockOverlay->updateGeometry();
        m_blockOverlay->repaint();
        if (blocked)
            m_table->clearFocus();
        else
            m_table->setFocus();
        m_table->setEnabled(!blocked);
    });
    connect(m_document, &Document::blockingOperationTitleChanged,
            this, [this](const QString &t) {
        m_blockTitle->setText(t);
        repositionBlockOverlay();
    });
    connect(m_document, &Document::blockingOperationCancelableChanged,
            this, [this](bool b) { m_blockCancel->setEnabled(b); });
    connect(m_document, &Document::blockingOperationProgress,
            this, [this](int done, int total) {
        if (m_blockProgress->maximum() != total)
            m_blockProgress->setRange(0, total);
        m_blockProgress->setValue(done);
    });
    connect(m_blockCancel, &QToolButton::clicked,
            this, [this]() { m_document->cancelBlockingOperation(); });

    auto rhp = Config::inst()->rowHeightPercent();
    if (rhp != 100)
        m_rowHeightFactor = double(rhp) / 100.;
    connect(Config::inst(), &Config::rowHeightPercentChanged,
            this, [this, dd](int) {
        m_table->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(m_table));
    });
    new EventFilter(m_table->viewport(), { QEvent::Wheel, QEvent::NativeGesture },
                    std::bind(&View::zoomFilter, this, _1, _2));

    updateCaption();

    languageChange();
}

View::~View()
{
    delete m_actionConnectionContext;
    m_actionConnectionContext = nullptr;
    //qWarning() << "~" << this;
}

const BrickLink::LotList &View::selectedLots() const
{
    return m_document->selectedLots();
}

void View::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

void View::languageChange()
{
    m_blockCancel->setToolTip(tr("Cancel the currently running blocking operation"));
    updateCaption();
}

void View::updateCaption()
{
    QString cap = m_document->filePathOrTitle();
    if (cap.isEmpty())
        cap = tr("Untitled");

    cap += u"[*]";

    setWindowTitle(cap);
    setWindowModified(m_model->isModified());
}

void View::ensureLatestVisible()
{
    if (m_latest_row >= 0) {
        int xOffset = m_table->horizontalScrollBar()->value();
        m_table->scrollTo(m_model->index(m_latest_row, m_header->logicalIndexAt(-xOffset)));
        m_latest_row = -1;
    }
}

QCoro::Task<> View::partOutItems()
{
    const auto selection = selectedLots();

    if (selection.count() >= 1) {
        auto pom = Config::inst()->partOutMode();
        bool inplace = (pom == Config::PartOutMode::InPlace);
        int oldCurrentColumn = m_document->currentIndex().column();

        if (pom == Config::PartOutMode::Ask) {
            switch (co_await UIHelpers::question(tr("Should the selected items be parted out into the current document, replacing the selected items?"),
                                                 UIHelpers::Yes | UIHelpers::No | UIHelpers::Cancel,
                                                 UIHelpers::Yes)) {
            case UIHelpers::Cancel:
                co_return;
            case UIHelpers::Yes:
                inplace = true;
                break;
            default:
                break;
            }
        }

        if (inplace)
            m_model->beginMacro();

        int partcount = 0;
        BrickLink::LotList inplaceLots;

        for (Lot *lot : selection) {
            if (!lot->item() || !lot->item()->hasInventory())
                continue;

            BrickLink::Status status = lot->status();
            int quantity = lot->quantity();
            auto condition = lot->condition();
            BrickLink::PartOutTraits partOutTraits = { };
            BrickLink::Status extraParts = BrickLink::Status::Extra;

            if (lot->item()->partOutTraits()) {
                ImportInventoryDialog dlg(lot->item(), quantity, condition, this);
                dlg.setWindowModality(Qt::ApplicationModal);
                dlg.show();
                if (co_await qCoro(&dlg, &QDialog::finished) != QDialog::Accepted)
                    continue;

                quantity = dlg.quantity();
                condition = dlg.condition();
                partOutTraits = dlg.partOutTraits();
                extraParts = dlg.extraParts();
            }

            if (inplace) {
                if (quantity) {
                    auto pr = BrickLink::IO::fromPartInventory(lot->item(), lot->color(), quantity,
                                                               condition, extraParts, partOutTraits,
                                                               status);
                    auto newLots = pr.takeLots();
                    if (!newLots.isEmpty()) {
                        inplaceLots.append(newLots);
                        m_model->insertLotsAfter(lot, std::move(newLots));
                        m_model->removeLot(lot);
                        partcount++;
                    }
                }
            } else {
                Document::fromPartInventory(lot->item(), lot->color(), quantity, condition,
                                            extraParts, partOutTraits, status);
            }
        }
        if (inplace) {
            m_model->endMacro(tr("Parted out %n item(s)", nullptr, partcount));

            if (!inplaceLots.isEmpty())
                m_document->selectLots(inplaceLots, inplaceLots.constFirst(), oldCurrentColumn);
        }
    }
    else
        QApplication::beep();
}


void View::contextMenu(const QPoint &pos)
{
    if (!m_contextMenu)
        m_contextMenu = new QMenu(this);
    m_contextMenu->clear();
    QAction *defaultAction = nullptr;

    auto idx = m_table->indexAt(pos);
    if (idx.isValid()) {
        QVector<QByteArray> actionNames;

        switch (idx.column()) {
        case DocumentModel::Status:
            actionNames = { "edit_status_include", "edit_status_exclude", "edit_status_extra", "-",
                            "edit_status_toggle" };
            break;
        case DocumentModel::Picture:
        case DocumentModel::PartNo:
        case DocumentModel::Description:
            actionNames = { "edit_item" };
            break;
        case DocumentModel::Condition:
            actionNames = { "edit_cond_new", "edit_cond_used", "-", "edit_subcond_none",
                            "edit_subcond_sealed", "edit_subcond_complete",
                            "edit_subcond_incomplete" };
            break;
        case DocumentModel::Color:
            actionNames = { "edit_color" };
            break;
        case DocumentModel::QuantityOrig:
        case DocumentModel::QuantityDiff:
        case DocumentModel::Quantity:
            actionNames = { "edit_qty_set", "edit_qty_multiply", "edit_qty_divide" };
            break;
        case DocumentModel::PriceOrig:
        case DocumentModel::PriceDiff:
        case DocumentModel::Price:
        case DocumentModel::Total:
            actionNames = { "edit_price_set", "edit_price_inc_dec", "edit_price_round",
                            "edit_price_to_priceguide" };
            break;
        case DocumentModel::Cost:
            actionNames = { "edit_cost_set", "edit_cost_inc_dec", "edit_cost_round",
                            "edit_cost_spread_price", "edit_cost_spread_weight" };
            break;
        case DocumentModel::Bulk:
            actionNames = { "edit_bulk" };
            break;
        case DocumentModel::Sale:
            actionNames = { "edit_sale" };
            break;
        case DocumentModel::Comments:
            actionNames = { "edit_comment_set", "edit_comment_add", "edit_comment_rem",
                            "edit_comment_clear" };
            break;
        case DocumentModel::Remarks:
            actionNames = { "edit_remark_set", "edit_remark_add", "edit_remark_rem",
                            "edit_remark_clear" };
            break;
        case DocumentModel::Retain:
            actionNames = { "edit_retain_yes", "edit_retain_no", "-", "edit_retain_toggle" };
            break;
        case DocumentModel::Stockroom:
            actionNames = { "edit_stockroom_no", "-", "edit_stockroom_a", "edit_stockroom_b",
                            "edit_stockroom_c" };
            break;
        case DocumentModel::Reserved:
            actionNames = { "edit_reserved" };
            break;
        case DocumentModel::Marker:
            actionNames = { "edit_marker_text", "edit_marker_color", "-", "edit_marker_clear" };
            break;
        case DocumentModel::LotId:
            actionNames = { "edit_lotid_copy", "edit_lotid_clear" };
            break;
        case DocumentModel::TierP1:
        case DocumentModel::TierP2:
        case DocumentModel::TierP3:
            actionNames = { "edit_tierprice_relative" };
            break;
        case DocumentModel::AlternateIds:
            actionNames = { "edit_altid_copy" };
            break;
        }
        for (const auto &actionName : std::as_const(actionNames)) {
            if (actionName == "-")
                m_contextMenu->addSeparator();
            else
                m_contextMenu->addAction(ActionManager::inst()->qAction(actionName.constData()));
        }
    }

    if (!m_contextMenu->isEmpty())
        m_contextMenu->addSeparator();

    const auto actions = MainWindow::inst()->contextMenuActions();
    for (auto action : std::as_const(actions)) {
        if (action)
            m_contextMenu->addAction(action);
        else
            m_contextMenu->addSeparator();
    }
    m_contextMenu->popup(m_table->viewport()->mapToGlobal(pos), defaultAction);
}

void View::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    m_table->setGeometry(rect());
    repositionBlockOverlay();

    // check if the view borders the windows' bottom right corner
    QPoint listBR = m_table->mapTo(m_table->window(), m_table->rect().bottomRight());
    if ((listBR - m_table->window()->rect().bottomRight()).manhattanLength() < 10)
        m_table->setCornerWidget(m_sizeGrip);
    else
        m_table->setCornerWidget(nullptr);
}

void View::repositionBlockOverlay()
{
    m_blockOverlay->resize(m_blockOverlay->sizeHint());
    auto p = rect().center() - QPoint(m_blockOverlay->width(), m_blockOverlay->height()) / 2;
    m_blockOverlay->move(p);
}

EventFilter::Result View::zoomFilter(QObject *o, QEvent *e)
{
    if ((e->type() == QEvent::Wheel) && (o == m_table->viewport())) {
        const auto *we = static_cast<QWheelEvent *>(e);
        if (we->modifiers() == Qt::ControlModifier) {
            double z = std::pow(1.001, we->angleDelta().y());
            setRowHeightFactor(m_rowHeightFactor * z);
            e->accept();
            return EventFilter::StopEventProcessing;
        }
    } else if ((e->type() == QEvent::NativeGesture) && (o == m_table->viewport())) {
        const auto *nge = static_cast<QNativeGestureEvent *>(e);
        if (nge->gestureType() == Qt::ZoomNativeGesture) {
            double z = 1 + nge->value();
            setRowHeightFactor(m_rowHeightFactor * z);
            e->accept();
            return EventFilter::StopEventProcessing;
        }
    }
    return EventFilter::ContinueEventProcessing;
}

double View::rowHeightFactor() const
{
    return m_rowHeightFactor;
}

void View::setRowHeightFactor(double factor)
{
    if (!Config::inst()->liveEditRowHeight())
        return;

    factor = std::clamp(factor, .5, 2.);

    if (!qFuzzyCompare(factor, m_rowHeightFactor)) {
        m_rowHeightFactor = factor;
        Config::inst()->setRowHeightPercent(int(factor * 100));
    }
}


void View::print(bool asPdf)
{
    if (m_model->filteredLots().isEmpty())
        return;

    auto *dlg = new PrintDialog(asPdf, this);
    connect(dlg, &PrintDialog::paintRequested,
            this, [=, this](QPrinter *previewPrt, const QList<uint> &pages, double scaleFactor,
            uint *maxPageCount, double *maxWidth) {
        printPages(previewPrt, (previewPrt->printRange() == QPrinter::Selection)
                   ? selectedLots() : model()->filteredLots(),
                   pages, scaleFactor, maxPageCount, maxWidth);
    });
    dlg->setWindowModality(Qt::ApplicationModal);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void View::printScriptAction(PrintingScriptAction *printingAction)
{
    auto *dlg = new PrintDialog(false /*asPdf*/, this);
    dlg->setWindowModality(Qt::ApplicationModal);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setProperty("bsFailOnce", false);

    connect(dlg, &PrintDialog::paintRequested,
            this, [=, this](QPrinter *previewPrt, const QList<uint> &pages, double scaleFactor,
            uint *maxPageCount, double *maxWidth) {
        try {
            Q_UNUSED(scaleFactor)
            Q_UNUSED(maxWidth)
            previewPrt->setFullPage(true);
            printingAction->executePrint(previewPrt, document(),
                                         previewPrt->printRange() == QPrinter::Selection,
                                         pages, maxPageCount);
        } catch (const Exception &e) {
            if (!dlg->property("bsFailOnce").toBool()) {
                dlg->setProperty("bsFailOnce", true);

                QString msg = e.errorString();
                if (msg.isEmpty())
                    msg = tr("Printing failed.");
                else
                    msg.replace(u"\n"_qs, u"<br>"_qs);

                QMetaObject::invokeMethod(this, [=]() {
                    UIHelpers::warning(msg);
                }, Qt::QueuedConnection);

                dlg->deleteLater();
            }
        }
    });
    dlg->show();
}

bool View::printPages(QPrinter *prt, const LotList &lots, const QList<uint> &pages,
                      double scaleFactor, uint *maxPageCount, double *maxWidth)
{
    if (maxPageCount)
        *maxPageCount = 0;
    if (maxWidth) // add a safety margin for rounding errors
        *maxWidth = 0.001;

    if (!prt)
        return false;

    prt->setDocName(document()->fileNameOrTitle()); // no absolute paths allowed

    QPainter p;
    if (!p.begin(prt))
        return false;

    double prtDpi = double(prt->logicalDpiX() + prt->logicalDpiY()) / 2;
    double winDpi = double(m_table->logicalDpiX() + m_table->logicalDpiX()) / 2 * m_table->devicePixelRatioF();

    QRectF pageRect = prt->pageLayout().paintRect(QPageLayout::Inch);
    pageRect = QRectF(QPointF(), pageRect.size() * prtDpi);

    double rowHeight = double(m_table->verticalHeader()->defaultSectionSize()) * prtDpi / winDpi * scaleFactor;
    int rowsPerPage = int((pageRect.height() - /* all headers and footers */ 2 * rowHeight) / rowHeight);

    if (rowsPerPage <= 0)
        return false;

    int pagesDown = (int(lots.size()) + rowsPerPage - 1) / rowsPerPage;

    QMap<int, QPair<DocumentModel::Field, double>> colWidths;
    for (int f = DocumentModel::Index; f < DocumentModel::FieldCount; ++f) {
        if (m_header->isSectionHidden(f))
            continue;
        int cw = m_header->sectionSize(f);
        if (cw <= 0)
            continue;
        double fcw = cw * prtDpi / winDpi;
        if (maxWidth)
            *maxWidth += fcw;
        colWidths.insert(m_header->visualIndex(f), qMakePair(DocumentModel::Field(f), fcw * scaleFactor));
    }

    QVector<QVector<QPair<DocumentModel::Field, double>>> colWidthsPerPageAcross;
    QVector<QPair<DocumentModel::Field, double>> curPageColWidths;
    curPageColWidths.reserve(colWidths.size());
    double cwUsed = 0;
    for (const auto &cw : colWidths) {
        if (!qFuzzyIsNull(cwUsed) && ((cwUsed + cw.second) > pageRect.width())) {
            colWidthsPerPageAcross.append(curPageColWidths);
            curPageColWidths.clear();
            cwUsed = 0;
        }
        auto cwClamped = qMakePair(cw.first, std::min(cw.second, double(pageRect.width())));
        cwUsed += cwClamped.second;
        curPageColWidths.append(cwClamped);
    }
    if (!curPageColWidths.isEmpty())
        colWidthsPerPageAcross.append(curPageColWidths);

    int pagesAcross = int(colWidthsPerPageAcross.size());
    int pageCount = 0;
    bool needNewPage = false;

    auto *dd = static_cast<DocumentDelegate *>(m_table->itemDelegate());

    QFont f = m_table->font();
    f.setPointSizeF(f.pointSizeF() * scaleFactor / m_table->devicePixelRatioF());
    p.setFont(f);

    QPen gridPen(Qt::darkGray, prtDpi / 96 * scaleFactor);
    QColor headerTextColor(Qt::white);
    double margin = 2 * prtDpi / 96 * scaleFactor;

    if (maxPageCount)
        *maxPageCount = uint(pagesDown * pagesAcross);

    for (int pd = 0; pd < pagesDown; ++pd) {
        for (int pa = 0; pa < pagesAcross; ++pa) {
            pageCount++;

            if (!pages.isEmpty() && !pages.contains(uint(pageCount)))
                continue;

            if (needNewPage)
                prt->newPage();
            else
                needNewPage = true;

            // print footer
            {
                QRectF footerRect(pageRect);
                footerRect.setTop(footerRect.bottom() - rowHeight);

                p.setPen(gridPen);
                QRectF boundingRect;
                p.drawText(footerRect, Qt::AlignRight | Qt::AlignBottom, tr("Page %1/%2")
                           .arg(pageCount).arg(pagesAcross * pagesDown), &boundingRect);
                footerRect.setWidth(footerRect.width() - boundingRect.width() - 5 * margin);
                p.drawText(footerRect, Qt::AlignLeft | Qt::AlignBottom, document()->filePathOrTitle());
            }

            const auto &colWidthsAcross = colWidthsPerPageAcross.at(pa);
            double dx = pageRect.left();
            bool firstColumn = true;

            for (const auto &cw : colWidthsAcross) {
                QString title = model()->headerData(cw.first, Qt::Horizontal).toString();
                Qt::Alignment align = Qt::Alignment(model()->headerData(cw.first, Qt::Horizontal,
                                                                        Qt::TextAlignmentRole).toInt());
                double w = cw.second;

                double dy = pageRect.top();

                // print header
                QRectF headerRect(dx, dy, w, rowHeight);
                p.setPen(gridPen);
                p.fillRect(headerRect, gridPen.color());
                p.drawRect(headerRect);

                p.setPen(headerTextColor);
                headerRect = headerRect.marginsRemoved({ margin, 0, margin, 0});
                p.drawText(headerRect, int(align), title);

                dy += rowHeight;

                int li = pd * rowsPerPage; // start at lot index
                for (int l = li; l < std::min(li + rowsPerPage, int(lots.size())); ++l) {
                    const Lot *lot = lots.at(l);
                    QModelIndex idx = model()->index(lot, cw.first);

                    QStyleOptionViewItem options;
                    m_table->initViewItemOption(&options);

                    options.rect = QRectF(dx, dy, w, rowHeight).toRect();
                    options.decorationSize *= (prtDpi / winDpi) * scaleFactor;
                    options.font = p.font();
                    options.fontMetrics = p.fontMetrics();
                    options.index = idx;
                    options.state.setFlag(QStyle::State_Selected, false);
                    options.palette = QPalette(Qt::lightGray);

                    dd->paint(&p, options, idx);

                    p.setPen(gridPen);
                    p.drawLine(options.rect.topRight(), options.rect.bottomRight());
                    p.drawLine(options.rect.bottomRight(), options.rect.bottomLeft());
                    if (firstColumn)
                        p.drawLine(options.rect.topLeft(), options.rect.bottomLeft());

                    dy += rowHeight;
                }
                dx += w;
                firstColumn = false;
            }
        }
    }
    return true;
}

#include "moc_view.cpp"
#include "moc_view_p.cpp"
