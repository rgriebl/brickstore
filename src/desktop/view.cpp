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
#include <QStringBuilder>
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

#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/order.h"
#include "bricklink/priceguide.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/uihelpers.h"
#include "utility/currency.h"
#include "utility/exception.h"
#include "utility/undo.h"
#include "utility/utility.h"
#include "changecurrencydialog.h"
#include "desktopuihelpers.h"
#include "documentdelegate.h"
#include "flowlayout.h"
#include "mainwindow.h"
#include "headerview.h"
#include "script.h"
#include "scriptmanager.h"
#include "view.h"
#include "view_p.h"

#include "printdialog.h"
#include "selectcolordialog.h"
#include "selectitemdialog.h"
#include "selectdocumentdialog.h"
#include "settopriceguidedialog.h"
#include "incdecpricesdialog.h"
#include "consolidateitemsdialog.h"



using namespace std::chrono_literals;




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
    for (int li = columnData.count(); li < DocumentModel::FieldCount; ++li)
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
{ }

void TableView::keyPressEvent(QKeyEvent *e)
{
    // ignore ctrl/alt+tab ... ViewPane needs to handle that
    if (DesktopUIHelpers::shouldSwitchViews(e)) {
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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


void View::setActive(bool active)
{
    Q_ASSERT(active == !m_actionConnectionContext);

    if (active) {
        m_actionConnectionContext = new QObject(this);

        for (auto &at : qAsConst(m_actionTable)) {
            if (QAction *a = ActionManager::inst()->qAction(at.first)) {
                QObject::connect(a, &QAction::triggered, m_actionConnectionContext, at.second);
            }
        }

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


View::View(Document *document, QWidget *parent)
    : QWidget(parent)
    , m_document(document)
    , m_sizeGrip(new QSizeGrip(this))
{
    Q_ASSERT(document && document->model());

    m_model = document->model();
    m_document->ref();

    connect(m_document, &Document::closeAllViews,
            this, [this]() { delete this; });

    m_latest_row = -1;
    m_latest_timer = new QTimer(this);
    m_latest_timer->setSingleShot(true);
    m_latest_timer->setInterval(100ms);

    m_actionTable = {
        { "edit_mergeitems", [this]() {
              if (!selectedLots().isEmpty())
                  consolidateLots(selectedLots());
              else
                  consolidateLots(m_model->sortedLots());
          } },
        { "edit_partoutitems", [this]() { partOutItems(); } },
        { "edit_copy_fields", [this]() {
              SelectCopyMergeDialog dlg(model(),
                                        tr("Select the document that should serve as a source to fill in the corresponding fields in the current document"),
                                        tr("Choose how fields are getting copied or merged."));

              if (dlg.exec() == QDialog::Accepted ) {
                  LotList lots = dlg.lots();
                  if (!lots.empty())
                      m_document->copyFields(lots, dlg.defaultMergeMode(), dlg.fieldMergeModes());
                  qDeleteAll(lots);
              }
          } },
        { "edit_subtractitems", [this]() {
              SelectDocumentDialog dlg(model(), tr("Which items should be subtracted from the current document:"));

              if (dlg.exec() == QDialog::Accepted) {
                  LotList lots = dlg.lots();
                  if (!lots.empty())
                      m_document->subtractItems(lots);
                  qDeleteAll(lots);
              }
          } },
        { "edit_paste", [this]() -> QCoro::Task<> {
              LotList lots = DocumentLotsMimeData::lots(QApplication::clipboard()->mimeData());

              if (!lots.empty()) {
                  if (!selectedLots().isEmpty()) {
                       if (co_await UIHelpers::question(tr("Overwrite the currently selected items?"),
                                                        UIHelpers::Yes | UIHelpers::No, UIHelpers::Yes
                                               ) == UIHelpers::Yes) {
                          m_model->removeLots(selectedLots());
                      }
                  }
                  addLots(std::move(lots), AddLotMode::ConsolidateInteractive);
              }
          } },
        { "edit_paste_silent", [this]() {
              LotList lots = DocumentLotsMimeData::lots(QApplication::clipboard()->mimeData());
              if (!lots.empty())
                  addLots(std::move(lots), AddLotMode::AddAsNew);
          } },
        { "edit_price_to_priceguide", [this]() {
              Q_ASSERT(!selectedLots().isEmpty());
              //Q_ASSERT(m_setToPG.isNull());

              SetToPriceGuideDialog dlg(this);

              if (dlg.exec() == QDialog::Accepted)
                  m_document->setPriceToGuide(dlg.time(), dlg.price(), dlg.forceUpdate());
          } },
        { "edit_price_inc_dec", [this]() {
              bool showTiers = !m_header->isSectionHidden(DocumentModel::TierQ1);
              IncDecPricesDialog dlg(tr("Increase or decrease the prices of the selected items by"),
                                     showTiers, m_model->currencyCode(), this);
              if (dlg.exec() == QDialog::Accepted)
                  m_document->priceAdjust(dlg.isFixed(), dlg.value(), dlg.applyToTiers());
          } },
        { "edit_cost_inc_dec", [this]() {
              IncDecPricesDialog dlg(tr("Increase or decrease the costs of the selected items by"),
                                     false, m_model->currencyCode(), this);
              if (dlg.exec() == QDialog::Accepted)
                  m_document->costAdjust(dlg.isFixed(), dlg.value());
          } },
        { "edit_color", [this]() { m_table->editCurrentItem(DocumentModel::Color); } },
        { "edit_item", [this]() { m_table->editCurrentItem(DocumentModel::Description); } },

        { "document_print", [this]() { print(false); } },
        { "document_print_pdf", [this]() { print(true); } },
    };

    m_table = new TableView(this);
    m_header = new HeaderView(Qt::Horizontal, m_table);
    m_header->setSectionsClickable(true);
    m_header->setSectionsMovable(true);
    m_header->setHighlightSections(false);
    m_table->setHorizontalHeader(m_header);
    m_header->setSortIndicatorShown(false);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setAutoFillBackground(false);
    m_table->setTabKeyNavigation(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
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

    connect(m_header, &HeaderView::sortColumnsChanged,
            this, [this](const QVector<QPair<int, Qt::SortOrder>> &sortColumns) {
        if (model()->sortColumns() != sortColumns) {
            model()->sort(sortColumns);
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

    // This shouldn't be needed, but we are abusing layoutChanged a bit for adding and removing
    // items. The docs are a bit undecided if you should really do that, but it really helps
    // performance wise. Just the selection is not updated, when the items in it are deleted.
    connect(m_model, &DocumentModel::layoutChanged,
            m_document, &Document::updateSelection);

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
    m_blockCancel->setIcon(QIcon::fromTheme("process-stop"_l1));
    m_blockCancel->setAutoRaise(true);
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

    connect(m_document, &Document::blockingOperationActiveChanged,
            m_table, &QWidget::setDisabled);

    updateMinimumSectionSize();

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

    updateCaption();

    setFocusProxy(m_table);

    languageChange();
}

View::~View()
{
    m_document->deref();
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
    else if (e->type() == QEvent::FontChange)
        updateMinimumSectionSize();
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

    cap += "[*]"_l1;

    setWindowTitle(cap);
    setWindowModified(m_model->isModified());
}

void View::updateMinimumSectionSize()
{
    int s = qMax(16, fontMetrics().height() * 2);
    m_header->setMinimumSectionSize(s);
    m_table->setIconSize({ s, s });
}

void View::ensureLatestVisible()
{
    if (m_latest_row >= 0) {
        int xOffset = m_table->horizontalScrollBar()->value();
        m_table->scrollTo(m_model->index(m_latest_row, m_header->logicalIndexAt(-xOffset)));
        m_latest_row = -1;
    }
}

void View::addLots(LotList &&lots, AddLotMode addLotMode)
{
    // we own the items now

    if (lots.empty())
        return;

    bool startedMacro = false;

    bool wasEmpty = (model()->lotCount() == 0);
    Lot *lastAdded = nullptr;
    int addCount = 0;
    int consolidateCount = 0;
    Consolidate conMode = Consolidate::IntoExisting;
    bool repeatForRemaining = false;
    bool costQtyAvg = true;

    for (int i = 0; i < lots.size(); ++i) {
        Lot *lot = lots.at(i);
        bool justAdd = true;

        if (addLotMode != AddLotMode::AddAsNew) {
            Lot *mergeLot = nullptr;

            const auto documentLots = model()->sortedLots();
            for (int j = documentLots.count() - 1; j >= 0; --j) {
                Lot *otherLot = documentLots.at(j);
                if ((!lot->isIncomplete() && !otherLot->isIncomplete())
                        && (lot->item() == otherLot->item())
                        && (lot->color() == otherLot->color())
                        && (lot->condition() == otherLot->condition())
                        && ((lot->status() == BrickLink::Status::Exclude) ==
                            (otherLot->status() == BrickLink::Status::Exclude))) {
                    mergeLot = otherLot;
                    break;
                }
            }

            if (mergeLot) {
                int mergeIndex = -1;

                if ((addLotMode == AddLotMode::ConsolidateInteractive) && !repeatForRemaining) {
                    LotList list { mergeLot, lot };

                    ConsolidateItemsDialog dlg(this, list,
                                               conMode == Consolidate::IntoExisting ? 0 : 1,
                                               conMode, i + 1, lots.size(), this);
                    bool yesClicked = (dlg.exec() == QDialog::Accepted);
                    repeatForRemaining = dlg.repeatForAll();
                    costQtyAvg = dlg.costQuantityAverage();


                    if (yesClicked) {
                        mergeIndex = dlg.consolidateToIndex();

                        if (repeatForRemaining) {
                            conMode = dlg.consolidateRemaining();
                            mergeIndex = (conMode == Consolidate::IntoExisting) ? 0 : 1;
                        }
                    } else {
                        if (repeatForRemaining)
                            addLotMode = AddLotMode::AddAsNew;
                    }
                } else {
                    mergeIndex = (conMode == Consolidate::IntoExisting) ? 0 : 1;
                }

                if (mergeIndex >= 0) {
                    justAdd = false;

                    if (!startedMacro) {
                        m_model->beginMacro();
                        startedMacro = true;
                    }

                    if (mergeIndex == 0) {
                        // merge new into existing
                        Lot changedLot = *mergeLot;
                        changedLot.mergeFrom(*lot, costQtyAvg);
                        m_model->changeLot(mergeLot, changedLot);
                        delete lot; // we own it, but we don't need it anymore
                    } else {
                        // merge existing into new, add new, remove existing
                        lot->mergeFrom(*mergeLot, costQtyAvg);
                        lot->setDateAdded(QDateTime::currentDateTimeUtc());
                        m_model->appendLot(std::move(lot)); // pass on ownership
                        m_model->removeLot(mergeLot);
                    }

                    ++consolidateCount;
                }
            }
        }

        if (justAdd) {
            if (!startedMacro) {
                m_model->beginMacro();
                startedMacro = true;
            }

            lot->setDateAdded(QDateTime::currentDateTimeUtc());
            lastAdded = lot;
            m_model->appendLot(std::move(lot));  // pass on ownership to the doc
            ++addCount;
        }
    }

    if (startedMacro)
        m_model->endMacro(tr("Added %1, consolidated %2 items").arg(addCount).arg(consolidateCount));

    if (wasEmpty)
        m_table->selectRow(0);

    if (lastAdded) {
        m_latest_row = m_model->index(lastAdded).row();
        m_latest_timer->start();
    }

    lots.clear();
}


void View::consolidateLots(const LotList &lots)
{
    if (lots.count() < 2)
        return;

    QVector<LotList> mergeList;
    LotList sourceLots = lots;

    for (int i = 0; i < sourceLots.count(); ++i) {
        Lot *lot = sourceLots.at(i);
        LotList mergeLots;

        for (int j = i + 1; j < sourceLots.count(); ++j) {
            Lot *otherLot = sourceLots.at(j);
            if ((!lot->isIncomplete() && !otherLot->isIncomplete())
                    && (lot->item() == otherLot->item())
                    && (lot->color() == otherLot->color())
                    && (lot->condition() == otherLot->condition())
                    && ((lot->status() == BrickLink::Status::Exclude) ==
                        (otherLot->status() == BrickLink::Status::Exclude))) {
                mergeLots << sourceLots.takeAt(j--);
            }
        }
        if (mergeLots.isEmpty())
            continue;

        mergeLots.prepend(sourceLots.at(i));
        mergeList << mergeLots;
    }

    if (mergeList.isEmpty())
        return;

    bool startedMacro = false;

    auto conMode = Consolidate::IntoLowestIndex;
    bool repeatForRemaining = false;
    bool costQtyAvg = true;
    int consolidateCount = 0;

    for (int mi = 0; mi < mergeList.count(); ++mi) {
        const LotList &mergeLots = mergeList.at(mi);
        int mergeIndex = -1;

        if (!repeatForRemaining) {
            ConsolidateItemsDialog dlg(this, mergeLots, consolidateLotsHelper(mergeLots, conMode),
                                       conMode, mi + 1, mergeList.count(), this);
            bool yesClicked = (dlg.exec() == QDialog::Accepted);
            repeatForRemaining = dlg.repeatForAll();
            costQtyAvg = dlg.costQuantityAverage();

            if (yesClicked) {
                mergeIndex = dlg.consolidateToIndex();

                if (repeatForRemaining) {
                    conMode = dlg.consolidateRemaining();
                    mergeIndex = consolidateLotsHelper(mergeLots, conMode);
                }
            } else {
                if (repeatForRemaining)
                    break;
                else
                    continue;
            }
        } else {
            mergeIndex = consolidateLotsHelper(mergeLots, conMode);
        }

        if (!startedMacro) {
            m_model->beginMacro();
            startedMacro = true;
        }

        Lot newitem = *mergeLots.at(mergeIndex);
        for (int i = 0; i < mergeLots.count(); ++i) {
            if (i != mergeIndex) {
                newitem.mergeFrom(*mergeLots.at(i), costQtyAvg);
                m_model->removeLot(mergeLots.at(i));
            }
        }
        m_model->changeLot(mergeLots.at(mergeIndex), newitem);

        ++consolidateCount;
    }
    if (startedMacro)
        m_model->endMacro(tr("Consolidated %n item(s)", nullptr, consolidateCount));
}

int View::consolidateLotsHelper(const LotList &lots, Consolidate conMode) const
{
    switch (conMode) {
    case Consolidate::IntoTopSorted:
        return 0;
    case Consolidate::IntoBottomSorted:
        return lots.count() - 1;
    case Consolidate::IntoLowestIndex: {
        const auto di = model()->lots();
        auto it = std::min_element(lots.cbegin(), lots.cend(), [di](const auto &a, const auto &b) {
            return di.indexOf(a) < di.indexOf(b);
        });
        return int(std::distance(lots.cbegin(), it));
    }
    case Consolidate::IntoHighestIndex: {
        const auto di = model()->lots();
        auto it = std::max_element(lots.cbegin(), lots.cend(), [di](const auto &a, const auto &b) {
            return di.indexOf(a) < di.indexOf(b);
        });
        return int(std::distance(lots.cbegin(), it));
    }
    default:
        break;
    }
    return -1;
}

QCoro::Task<> View::partOutItems()
{
    if (selectedLots().count() >= 1) {
        auto pom = Config::inst()->partOutMode();
        bool inplace = (pom == Config::PartOutMode::InPlace);

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

        foreach(Lot *lot, selectedLots()) {
            if (inplace) {
                if (lot->item()->hasInventory() && lot->quantity()) {
                    const auto &parts = lot->item()->consistsOf();
                    if (!parts.isEmpty()) {
                        int multiply = lot->quantity();

                        LotList newLots;

                        for (const BrickLink::Item::ConsistsOf &part : parts) {
                            auto partItem = part.item();
                            auto partColor = part.color();
                            if (!partItem)
                                continue;
                            if (lot->colorId() && partItem->itemType()->hasColors()
                                    && partColor && (partColor->id() == 0)) {
                                partColor = lot->color();
                            }
                            auto *newLot = new Lot(partColor, partItem);
                            newLot->setQuantity(part.quantity() * multiply);
                            newLot->setCondition(lot->condition());
                            newLot->setAlternate(part.isAlternate());
                            newLot->setAlternateId(part.alternateId());
                            newLot->setCounterPart(part.isCounterPart());

                            newLots << newLot;
                        }
                        m_model->insertLotsAfter(lot, std::move(newLots));
                        m_model->removeLot(lot);
                        partcount++;
                    }
                }
            } else {
                Document::fromPartInventory(lot->item(), lot->color(), lot->quantity(),
                                            lot->condition());
            }
        }
        if (inplace)
            m_model->endMacro(tr("Parted out %n item(s)", nullptr, partcount));
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
        }
        for (const auto &actionName : qAsConst(actionNames)) {
            if (actionName == "-")
                m_contextMenu->addSeparator();
            else
                m_contextMenu->addAction(ActionManager::inst()->qAction(actionName.constData()));
        }
    }

    if (!m_contextMenu->isEmpty())
        m_contextMenu->addSeparator();

    const auto actions = MainWindow::inst()->contextMenuActions();
    for (auto action : qAsConst(actions)) {
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


void View::print(bool asPdf)
{
    if (m_model->filteredLots().isEmpty())
        return;

    PrintDialog pd(asPdf, this);
    connect(&pd, &PrintDialog::paintRequested,
            this, [=, this](QPrinter *previewPrt, const QList<uint> &pages, double scaleFactor,
            uint *maxPageCount, double *maxWidth) {
        printPages(previewPrt, (previewPrt->printRange() == QPrinter::Selection)
                   ? selectedLots() : model()->filteredLots(),
                   pages, scaleFactor, maxPageCount, maxWidth);
    });
    pd.exec();
}

void View::printScriptAction(PrintingScriptAction *printingAction)
{
    auto *pd = new PrintDialog(false /*asPdf*/, this);
    pd->setModal(true);
    pd->setAttribute(Qt::WA_DeleteOnClose);
    pd->setProperty("bsFailOnce", false);

    connect(pd, &PrintDialog::paintRequested,
            this, [=, this](QPrinter *previewPrt, const QList<uint> &pages, double scaleFactor,
            uint *maxPageCount, double *maxWidth) {
        try {
            Q_UNUSED(scaleFactor)
            Q_UNUSED(maxWidth)
            previewPrt->setFullPage(true);
            printingAction->executePrint(previewPrt, this,
                                         previewPrt->printRange() == QPrinter::Selection,
                                         pages, maxPageCount);
        } catch (const Exception &e) {
            if (!pd->property("bsFailOnce").toBool()) {
                pd->setProperty("bsFailOnce", true);

                QString msg = e.error();
                if (msg.isEmpty())
                    msg = tr("Printing failed.");
                else
                    msg.replace('\n'_l1, "<br>"_l1);

                QMetaObject::invokeMethod(this, [=]() {
                    UIHelpers::warning(msg);
                }, Qt::QueuedConnection);

                pd->deleteLater();
            }
        }
    });
    pd->open();
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

    prt->setDocName(document()->filePathOrTitle());

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

    int pagesDown = (lots.size() + rowsPerPage - 1) / rowsPerPage;

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
    double cwUsed = 0;
    for (const auto &cw : colWidths) {
        if (!qFuzzyIsNull(cwUsed) && ((cwUsed + cw.second) > pageRect.width())) {
            colWidthsPerPageAcross.append(curPageColWidths);
            curPageColWidths.clear();
            cwUsed = 0;
        }
        auto cwClamped = qMakePair(cw.first, qMin(cw.second, double(pageRect.width())));
        cwUsed += cwClamped.second;
        curPageColWidths.append(cwClamped);
    }
    if (!curPageColWidths.isEmpty())
        colWidthsPerPageAcross.append(curPageColWidths);

    int pagesAcross = colWidthsPerPageAcross.size();
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

            const auto &colWidths = colWidthsPerPageAcross.at(pa);
            double dx = pageRect.left();
            bool firstColumn = true;

            for (const auto &cw : colWidths) {
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
                for (int l = li; l < qMin(li + rowsPerPage, lots.size()); ++l) {
                    const Lot *lot = lots.at(l);
                    QModelIndex idx = model()->index(lot, cw.first);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    QStyleOptionViewItem options = m_table->viewOptions();
#else
                    QStyleOptionViewItem options;
                    m_table->initViewItemOption(&options);
#endif
                    options.rect = QRectF(dx, dy, w, rowHeight).toRect();
                    options.decorationSize *= (prtDpi / winDpi) * scaleFactor;
                    options.font = p.font();
                    options.fontMetrics = p.fontMetrics();
                    options.index = idx;
                    options.state &= ~QStyle::State_Selected;
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
