// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPushButton>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QShortcut>
#include <QKeyEvent>
#include <QTimer>
#include <QMessageBox>
#include <QMenu>

#include "common/currency.h"
#include "common/humanreadabletimedelta.h"
#include "utility/utility.h"
#include "bricklink/core.h"
#include "bricklink/order.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentio.h"
#include "betteritemdelegate.h"
#include "historylineedit.h"
#include "importorderdialog.h"
#include "orderinformationdialog.h"

using namespace std::chrono_literals;


ImportOrderDialog::ImportOrderDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_update->setProperty("iconScaling", true);

    w_orders->header()->setStretchLastSection(false);
    auto proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortLocaleAware(true);
    proxyModel->setSortRole(BrickLink::Orders::OrderSortRole);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);
    proxyModel->setSourceModel(BrickLink::core()->orders());
    w_orders->setModel(proxyModel);
    w_orders->setContextMenuPolicy(Qt::CustomContextMenu);
    w_orders->header()->setSectionResizeMode(BrickLink::Orders::OtherParty, QHeaderView::Stretch);
    w_orders->setItemDelegate(new BetterItemDelegate(BetterItemDelegate::AlwaysShowSelection
                                                     | BetterItemDelegate::MoreSpacing, w_orders));

    connect(w_filter, &HistoryLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { w_filter->setFocus(); });

    w_importCombined = new QPushButton();
    w_importCombined->setDefault(false);
    w_buttons->addButton(w_importCombined, QDialogButtonBox::ActionRole);
    connect(w_importCombined, &QAbstractButton::clicked,
            this, [this]() { importOrders(w_orders->selectionModel()->selectedRows(), true); });
    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, [this]() { importOrders(w_orders->selectionModel()->selectedRows(), false); });

    connect(w_update, &QToolButton::clicked,
            this, &ImportOrderDialog::updateOrders);
    connect(w_orders->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportOrderDialog::checkSelected);
    connect(w_orders, &QTreeView::activated,
            this, [this]() { w_import->animateClick(); });
    connect(BrickLink::core(), &BrickLink::Core::authenticatedTransferOverallProgress,
            this, [this](int done, int total) {
        w_progress->setVisible(done != total);
        w_progress->setMaximum(total);
        w_progress->setValue(done);
    });
    connect(BrickLink::core()->orders(), &BrickLink::Orders::updateFinished,
            this, [this](bool success, const QString &message) {
        Q_UNUSED(success)

        w_update->setEnabled(true);
        w_orders->setEnabled(true);

        m_updateMessage = message;
        updateStatusLabel();

        if (BrickLink::core()->orders()->rowCount()) {
            auto tl = w_orders->model()->index(0, 0);
            w_orders->selectionModel()->select(tl, QItemSelectionModel::SelectCurrent
                                               | QItemSelectionModel::Rows);
            w_orders->scrollTo(tl);
        }
        w_orders->header()->resizeSections(QHeaderView::ResizeToContents);
        w_orders->setFocus();

        checkSelected();
    });

    m_contextMenu = new QMenu(this);

    m_orderInformation = new QAction(this);
    m_orderInformation->setIcon(QIcon::fromTheme(ActionManager::inst()->action("document_import_bl_order")->iconName()));
    connect(m_orderInformation, &QAction::triggered, this, [this]() {
        const auto selection = w_orders->selectionModel()->selectedRows();
        if (selection.size() == 1) {
            auto order = selection.constFirst().data(BrickLink::Orders::OrderPointerRole)
                    .value<BrickLink::Order *>();
            if (order) {
                auto *dlg = new OrderInformationDialog(order, this);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->setWindowModality(Qt::ApplicationModal);
                dlg->show();
            }
        }
    });
    m_contextMenu->addAction(m_orderInformation);

    m_showOnBrickLink = new QAction(this);
    m_showOnBrickLink->setIcon(QIcon::fromTheme(u"bricklink"_qs));
    connect(m_showOnBrickLink, &QAction::triggered, this, [this]() {
        const auto selection = w_orders->selectionModel()->selectedRows();
        for (const auto &idx : selection) {
            auto order = idx.data(BrickLink::Orders::OrderPointerRole).value<BrickLink::Order *>();
            Application::openUrl(BrickLink::Core::urlForOrderDetails(order->id()));
        }
    });
    m_contextMenu->addAction(m_showOnBrickLink);

    connect(w_orders, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        m_contextMenu->popup(w_orders->viewport()->mapToGlobal(pos));
    });

    languageChange();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportOrderDialog::updateStatusLabel);
    t->start();

    if (!BrickLink::core()->orders()->rowCount())
        QMetaObject::invokeMethod(this, &ImportOrderDialog::updateOrders, Qt::QueuedConnection);

    int daysBack = Config::inst()->value(u"MainWindow/ImportOrderDialog/DaysBack"_qs, -1).toInt();
    if (daysBack > 0)
        w_daysBack->setValue(daysBack);
    auto ba = Config::inst()->value(u"MainWindow/ImportOrderDialog/Filter"_qs).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);
    ba = Config::inst()->value(u"MainWindow/ImportOrderDialog/ListState"_qs).toByteArray();
    if (!ba.isEmpty())
        w_orders->header()->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportOrderDialog::~ImportOrderDialog()
{
    Config::inst()->setValue(u"MainWindow/ImportOrderDialog/DaysBack"_qs, w_daysBack->value());
    Config::inst()->setValue(u"MainWindow/ImportOrderDialog/Filter"_qs, w_filter->saveState());
    Config::inst()->setValue(u"MainWindow/ImportOrderDialog/ListState"_qs, w_orders->header()->saveState());
}

void ImportOrderDialog::keyPressEvent(QKeyEvent *e)
{
    // simulate QDialog behavior
    if (e->matches(QKeySequence::Cancel)) {
        close();
        return;
    } else if ((!e->modifiers() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))
               || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter))) {
        // we need the animateClick here instead of triggering directly: otherwise we
        // get interference from the QTreeView::activated signal, resulting in double triggers
        if (w_import->isVisible() && w_import->isEnabled())
            w_import->animateClick();
        return;
    }

    QWidget::keyPressEvent(e);
}

void ImportOrderDialog::showEvent(QShowEvent *e)
{
    QByteArray ba = Config::inst()->value(u"MainWindow/ImportOrderDialog/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    QDialog::showEvent(e);
    activateWindow();
}

void ImportOrderDialog::closeEvent(QCloseEvent *e)
{
    Config::inst()->setValue(u"MainWindow/ImportOrderDialog/Geometry"_qs, saveGeometry());
    QDialog::closeEvent(e);
}

void ImportOrderDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ImportOrderDialog::languageChange()
{
    retranslateUi(this);

    w_import->setText(tr("Import"));
    w_importCombined->setText(tr("Import combined"));
    w_filter->setToolTip(ActionManager::toolTipLabel(tr("Filter the list for lines containing these words"),
                                                     QKeySequence::Find, w_filter->instructionToolTip()));
    m_orderInformation->setText(tr("Show order information"));
    m_showOnBrickLink->setText(tr("Show on BrickLink"));
    updateStatusLabel();
}

QCoro::Task<> ImportOrderDialog::updateOrders()
{
    if (BrickLink::core()->orders()->updateStatus() == BrickLink::UpdateStatus::Updating)
        co_return;

    if (!co_await Application::inst()->checkBrickLinkLogin())
        co_return;

    m_updateMessage.clear();
    w_update->setEnabled(false);
    w_import->setEnabled(false);
    w_orders->setEnabled(false);

    QDate today = QDate::currentDate();
    QDate fromDate = today.addDays(-w_daysBack->value());

    BrickLink::core()->orders()->startUpdate(fromDate, today);
    updateStatusLabel();
}

void ImportOrderDialog::importOrders(const QModelIndexList &rows, bool combined)
{
    bool combineCCode = false;
    if (combined && m_selectedCurrencyCodes.size() > 1) {
        if (QMessageBox::question(this, tr("Import Order"),
                                  tr("You have selected multiple orders with differing currencies, which cannot be combined as-is.")
                                  + u"<br><br>"
                                  + tr("Do you want to continue and convert all prices to your default currency (%1)?")
                                  .arg(Config::inst()->defaultCurrencyCode())) == QMessageBox::No) {
            return;
        }
        combineCCode = true;
    }

    QString defaultCCode = Config::inst()->defaultCurrencyCode();

    BrickLink::IO::ParseResult combinedPr;
    int orderCount = 0;

    for (auto idx : rows) {
        auto order = idx.data(BrickLink::Orders::OrderPointerRole).value<BrickLink::Order *>();

        if (combined) {
            double crate = 0;

            if (combineCCode && (order->currencyCode() != defaultCCode))
                crate = Currency::inst()->crossRate(order->currencyCode(), defaultCCode);

            LotList orderLots = order->loadLots(); // we own the Lots now
            if (!orderLots.isEmpty()) {
                QColor col = QColor::fromHsl(360 * orderCount / rows.size(), 128, 128);
                for (auto orderLot : orderLots) {
                    QString marker = orderLot->markerText();

                    orderLot->setMarkerText(order->id() + u' ' + order->otherParty()
                                            + (marker.isEmpty() ? QString()
                                                                : QString(u' ' + tr("Batch") + u": " + marker)));
                    orderLot->setMarkerColor(col);

                    if (!qFuzzyIsNull(crate)) {
                        orderLot->setCost(orderLot->cost() * crate);
                        orderLot->setPrice(orderLot->price() * crate);
                        for (int i = 0; i < 3; ++i)
                            orderLot->setTierPrice(i, orderLot->tierPrice(i) * crate);
                    }
                    combinedPr.addLot(std::move(orderLot));
                }
                combinedPr.setCurrencyCode(combineCCode ? defaultCCode : order->currencyCode());
            }
            orderLots.clear();
        } else {
            DocumentIO::importBrickLinkOrder(order);
        }
        ++orderCount;
    }
    if (combined) {
        auto doc = new Document(new DocumentModel(std::move(combinedPr))); // Document owns the items now
        doc->setTitle(tr("Multiple Orders"));
        doc->setThumbnail(u"view-financial-list"_qs);
    }
}

void ImportOrderDialog::checkSelected()
{
    bool b = w_orders->selectionModel()->hasSelection();
    bool singleSelection = false;
    w_import->setEnabled(b);
    m_showOnBrickLink->setEnabled(b);
    m_selectedCurrencyCodes.clear();

    // combined import only makes sense for the same type

    if (b) {
        BrickLink::OrderType otype = BrickLink::OrderType::Any;

        const auto rows = w_orders->selectionModel()->selectedRows();
        if (rows.size() > 1) {
            for (auto idx : rows) {
                auto order = idx.data(BrickLink::Orders::OrderPointerRole).value<BrickLink::Order *>();
                m_selectedCurrencyCodes.insert(order->currencyCode());

                if (otype == BrickLink::OrderType::Any) {
                    otype = order->type();
                } else if (otype != order->type()) {
                    otype = BrickLink::OrderType::Any;
                    break;
                }
            }
            if (otype == BrickLink::OrderType::Any)
                b = false;
        } else {
            b = false;
        }
        singleSelection = (rows.size() == 1);
    }
    m_orderInformation->setEnabled(singleSelection);
    w_importCombined->setEnabled(b);
}

void ImportOrderDialog::updateStatusLabel()
{
    QString s;

    switch (BrickLink::core()->orders()->updateStatus()) {
    case BrickLink::UpdateStatus::Ok:
        s = tr("Last updated %1").arg(
                    HumanReadableTimeDelta::toString(QDateTime::currentDateTime(),
                                                     BrickLink::core()->orders()->lastUpdated()));
        break;

    case BrickLink::UpdateStatus::Updating:
        s = tr("Currently updating orders");
        break;

    case BrickLink::UpdateStatus::UpdateFailed:
        s = tr("Last update failed") + u": " + m_updateMessage;
        break;

    default:
        break;
    }
    w_lastUpdated->setText(s);
}

#include "moc_importorderdialog.cpp"
