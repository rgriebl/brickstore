// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <cfloat>
#include <cmath>

#include <QDockWidget>
#include <QMainWindow>
#include <QEvent>
#include <QDesktopServices>
#include <QStatusTipEvent>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QItemSelectionModel>
#include <QMenu>
#include <QDebug>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/documentlist.h"
#include "common/recentfiles.h"
#include "betteritemdelegate.h"
#include "mainwindow.h"

#include "taskwidgets.h"

using namespace std::chrono_literals;


TaskPriceGuideWidget::TaskPriceGuideWidget(QWidget *parent)
    : PriceGuideWidget(parent), m_document(nullptr), m_dock(nullptr)
{
    setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        const Lot *selected = nullptr;
        if (m_document) {
            const auto selection = m_document->selectedLots();
            if (selection.size() == 1)
                selected = selection.constFirst();
        }
        setPriceGuide(selected ? BrickLink::core()->priceGuideCache()->priceGuide(selected->item(),
                                                                                  selected->color(), true)
                               : nullptr);
    });

    connect(MainWindow::inst(), &MainWindow::documentActivated,
            this, &TaskPriceGuideWidget::documentUpdate);
    connect(this, &PriceGuideWidget::priceDoubleClicked,
            this, &TaskPriceGuideWidget::setPrice);
    fixParentDockWindow();
}

void TaskPriceGuideWidget::documentUpdate(Document *document)
{
    if (m_document) {
        disconnect(m_document.data(), &Document::selectedLotsChanged,
                   this, &TaskPriceGuideWidget::selectionUpdate);
        disconnect(m_document->model(), &DocumentModel::currencyCodeChanged,
                   this, &TaskPriceGuideWidget::currencyUpdate);
    }
    m_document = document;
    if (m_document) {
        connect(m_document.data(), &Document::selectedLotsChanged,
                this, &TaskPriceGuideWidget::selectionUpdate);
        connect(m_document->model(), &DocumentModel::currencyCodeChanged,
                this, &TaskPriceGuideWidget::currencyUpdate);
    }

    setCurrencyCode(m_document ? m_document->model()->currencyCode() : Config::inst()->defaultCurrencyCode());
    selectionUpdate();
}

void TaskPriceGuideWidget::currencyUpdate(const QString &ccode)
{
    setCurrencyCode(ccode);
}

void TaskPriceGuideWidget::selectionUpdate()
{
    m_delayTimer.start();
}

void TaskPriceGuideWidget::setPrice(double p)
{
    if (m_document && (m_document->selectedLots().count() == 1)) {
        Lot *pos = m_document->selectedLots().front();
        Lot lot = *pos;

        auto doc = m_document->model();
        p *= Currency::inst()->rate(doc->currencyCode());
        lot.setPrice(p);

        doc->changeLot(pos, lot);
    }
}

bool TaskPriceGuideWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ParentChange)
        fixParentDockWindow();

    return PriceGuideWidget::event(e);
}

void TaskPriceGuideWidget::fixParentDockWindow()
{
    if (m_dock) {
        disconnect(m_dock, &QDockWidget::dockLocationChanged,
                   this, &TaskPriceGuideWidget::dockLocationChanged);
        disconnect(m_dock, &QDockWidget::topLevelChanged,
                   this, &TaskPriceGuideWidget::topLevelChanged);
    }

    m_dock = nullptr;

    for (QObject *p = parent(); p; p = p->parent()) {
        if (qobject_cast<QDockWidget *>(p)) {
            m_dock = static_cast<QDockWidget *>(p);
            break;
        }
    }

    if (m_dock) {
        connect(m_dock, &QDockWidget::dockLocationChanged,
                this, &TaskPriceGuideWidget::dockLocationChanged);
        connect(m_dock, &QDockWidget::topLevelChanged,
                this, &TaskPriceGuideWidget::topLevelChanged);
    }
}

void TaskPriceGuideWidget::topLevelChanged(bool b)
{
    if (b) {
        setLayout(PriceGuideWidget::Normal);
        //setFixedSize(minimumSize());
    }
}

void TaskPriceGuideWidget::dockLocationChanged(Qt::DockWidgetArea area)
{
    bool vertical = (area ==  Qt::LeftDockWidgetArea) || (area == Qt::RightDockWidgetArea);

    setLayout(vertical ? PriceGuideWidget::Vertical : PriceGuideWidget::Horizontal);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskInfoWidget::TaskInfoWidget(QWidget *parent)
    : QStackedWidget(parent), m_document(nullptr)
{
    setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_text = new QLabel(this);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_text->setIndent(8);
    m_text->setBackgroundRole(QPalette::Base);
    m_text->setAutoFillBackground(true);

    m_pic = new PictureWidget(this);

    addWidget(m_text);
    addWidget(m_pic);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout,
            this, &TaskInfoWidget::delayedSelectionUpdate);

    connect(MainWindow::inst(), &MainWindow::documentActivated,
            this, &TaskInfoWidget::documentUpdate);
    connect(Config::inst(), &Config::measurementSystemChanged,
            this, &TaskInfoWidget::refresh);

    m_pic->setPrefer3D(Config::inst()->value(u"MainWindow/TaskInfo/Prefer3D"_qs, true).toBool());
    setCurrentWidget(m_text);
}

TaskInfoWidget::~TaskInfoWidget()
{
    Config::inst()->setValue(u"MainWindow/TaskInfo/Prefer3D"_qs, m_pic->prefer3D());
}

void TaskInfoWidget::documentUpdate(Document *document)
{
    if (m_document) {
        disconnect(m_document.data(), &Document::selectedLotsChanged,
                   this, &TaskInfoWidget::selectionUpdate);
        disconnect(m_document->model(), &DocumentModel::statisticsChanged,
                   this, &TaskInfoWidget::statisticsUpdate);
        disconnect(m_document->model(), &DocumentModel::currencyCodeChanged,
                   this, &TaskInfoWidget::currencyUpdate);
    }
    m_document = document;
    if (m_document) {
        connect(m_document.data(), &Document::selectedLotsChanged,
                this, &TaskInfoWidget::selectionUpdate);
        connect(m_document->model(), &DocumentModel::statisticsChanged,
                this, &TaskInfoWidget::statisticsUpdate);
        connect(m_document->model(), &DocumentModel::currencyCodeChanged,
                this, &TaskInfoWidget::currencyUpdate);
    }

    selectionUpdate();
}

void TaskInfoWidget::currencyUpdate()
{
    selectionUpdate();
}

void TaskInfoWidget::statisticsUpdate()
{
    // only relevant, if we are showing the current document info
    if (m_document && (currentWidget() == m_text))
        selectionUpdate();
}

void TaskInfoWidget::selectionUpdate()
{
    m_delayTimer.start();
}

void TaskInfoWidget::delayedSelectionUpdate()
{
    if (!m_document) {
        m_pic->setItemAndColor(nullptr);
        m_text->clear();
        setCurrentWidget(m_text);
    } else {
        const auto selection = m_document->selectedLots();
        if (selection.count() == 1) {
            m_pic->setItemAndColor(selection.constFirst()->item(), selection.constFirst()->color());
            setCurrentWidget(m_pic);
        } else {
            auto stat = m_document->model()->statistics(selection.isEmpty() ? m_document->model()->lots()
                                                                            : selection,
                                                        false /* ignoreExcluded */);

            QString s = u"<h3>%1</h3>"_qs
                            .arg(selection.isEmpty() ? tr("Document statistics") : tr("Multiple lots selected"))
                        + stat.asHtmlTable();

            m_pic->setItemAndColor(nullptr);
            m_text->setText(s);
            setCurrentWidget(m_text);
        }
    }
}

void TaskInfoWidget::languageChange()
{
    refresh();
}

void TaskInfoWidget::refresh()
{
    selectionUpdate();
}

void TaskInfoWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QStackedWidget::changeEvent(e);
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskInventoryWidget::TaskInventoryWidget(QWidget *parent)
    : InventoryWidget(true /*showCanBuild*/, parent)
    , m_document(nullptr)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect(MainWindow::inst(), &MainWindow::documentActivated, this, &TaskInventoryWidget::documentUpdate);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        const LotList selection = m_document ? m_document->selectedLots() : LotList { };

        if (!m_document || selection.isEmpty())
            setItem(nullptr, nullptr);
        else if (selection.count() == 1)
            setItem(selection.constFirst()->item(), selection.constFirst()->color());
        else
            setItems(selection);
    });

    m_invGoToAction = new QAction(this);
    m_invGoToAction->setIcon(QIcon::fromTheme(u"edit-additems"_qs));
    connect(m_invGoToAction, &QAction::triggered, this, [this]() {
        const auto sel = selected();
        if (sel.m_item)
            MainWindow::inst()->showAddItemDialog(sel.m_item, sel.m_color);
    });
    addAction(m_invGoToAction);
    setActivateAction(m_invGoToAction);

    languageChange();

    restoreState(Config::inst()->value(u"MainWindow/TaskInventory/State"_qs).toByteArray());
}

TaskInventoryWidget::~TaskInventoryWidget()
{
    Config::inst()->setValue(u"MainWindow/TaskInventory/State"_qs, saveState());
}

void TaskInventoryWidget::documentUpdate(Document *document)
{
    if (m_document) {
        disconnect(m_document.data(), &Document::selectedLotsChanged,
                   this, &TaskInventoryWidget::selectionUpdate);
    }
    m_document = document;
    if (m_document) {
        connect(m_document.data(), &Document::selectedLotsChanged,
                this, &TaskInventoryWidget::selectionUpdate);
    }

    selectionUpdate();
}

void TaskInventoryWidget::selectionUpdate()
{
    m_delayTimer.start();
}

void TaskInventoryWidget::languageChange()
{
    m_invGoToAction->setText(tr("Add Item..."));
}

void TaskInventoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    InventoryWidget::changeEvent(e);
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


TaskOpenDocumentsWidget::TaskOpenDocumentsWidget(QWidget *parent)
    : QTreeView(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setTextElideMode(Qt::ElideMiddle);
    setWordWrap(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setModel(DocumentList::inst());
    int is = style()->pixelMetric(QStyle::PM_ListViewIconSize, nullptr, this);
    setIconSize({ int(is * 2), int(is * 2) });

    m_contextMenu = new QMenu(this);

    m_closeDocument = new QAction(this);
    m_closeDocument->setIcon(ActionManager::inst()->qAction("document_close")->icon());

    connect(m_closeDocument, &QAction::triggered, this, [this]() {
        auto idx = m_contextMenu->property("contextIndex").toModelIndex();
        if (idx.isValid()) {
            if (auto *doc = idx.data(Qt::UserRole).value<Document *>())
                doc->requestClose();
        }
    });

    m_contextMenu->addAction(m_closeDocument);

    connect(MainWindow::inst(), &MainWindow::documentActivated,
            this, [this](Document *doc) {
        int row = int(DocumentList::inst()->documents().indexOf(doc));
        selectionModel()->select(DocumentList::inst()->index(row, 0),
                                 QItemSelectionModel::ClearAndSelect);
    });

    connect(this, &QTreeView::clicked, this, [](const QModelIndex &idx) {
        if (idx.isValid()) {
            if (auto *doc = idx.data(Qt::UserRole).value<Document *>())
                emit doc->requestActivation();
        }
    });

    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        auto idx = indexAt(pos);
        if (idx.isValid()) {
            m_contextMenu->setProperty("contextIndex", idx);
            m_contextMenu->popup(viewport()->mapToGlobal(pos));
        }
    });

    languageChange();
}

void TaskOpenDocumentsWidget::languageChange()
{
    m_closeDocument->setText(ActionManager::inst()->qAction("document_close")->text());
}

void TaskOpenDocumentsWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QTreeView::changeEvent(e);
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


TaskRecentDocumentsWidget::TaskRecentDocumentsWidget(QWidget *parent)
    : QTreeView(parent)
{
    auto delegate = new BetterItemDelegate(BetterItemDelegate::Pinnable, this);
    delegate->setPinnedRole(RecentFiles::PinnedRole);
    setItemDelegate(delegate);
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setTextElideMode(Qt::ElideMiddle);
    setSelectionMode(QAbstractItemView::NoSelection);
    setModel(RecentFiles::inst());
    setProperty("singleClickActivation", true);

    connect(this, &QTreeView::activated, this, [](const QModelIndex &idx) {
        if (idx.isValid()) {
            QString fp = idx.data(RecentFiles::FilePathRole).toString();
            if (!fp.isEmpty())
                Document::load(fp);
        }
    });
}

#include "moc_taskwidgets.cpp"
