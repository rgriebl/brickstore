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

#include "utility/currency.h"
#include "utility/humanreadabletimedelta.h"
#include "utility/utility.h"
#include "bricklink/core.h"
#include "bricklink/order.h"
#include "common/actionmanager.h"
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
    w_orders->header()->setSectionResizeMode(4, QHeaderView::Stretch);
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

    m_orderInformation = new QAction(this);
    m_orderInformation->setIcon(QIcon::fromTheme(ActionManager::inst()->action("document_import_bl_order")->iconName()));
    connect(m_orderInformation, &QAction::triggered, this, [this]() {
        const auto selection = w_orders->selectionModel()->selectedRows();
        if (selection.size() == 1) {
            auto order = selection.constFirst().data(BrickLink::Orders::OrderPointerRole).value<BrickLink::Order *>();
            OrderInformationDialog d(order, this);
            d.exec();
        }
    });
    addAction(m_orderInformation);
    m_showOnBrickLink = new QAction(this);
    m_showOnBrickLink->setIcon(QIcon::fromTheme("bricklink"_l1));
    connect(m_showOnBrickLink, &QAction::triggered, this, [this]() {
        const auto selection = w_orders->selectionModel()->selectedRows();
        for (auto idx : selection) {
            auto order = idx.data(BrickLink::Orders::OrderPointerRole).value<BrickLink::Order *>();
            QByteArray orderId = order->id().toLatin1();
            BrickLink::core()->openUrl(BrickLink::URL_OrderDetails, orderId.constData());
        }
    });
    addAction(m_showOnBrickLink);

    connect(w_orders, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        //if (!w_orders->selectionModel()->selection().isEmpty())
            QMenu::exec(actions(), w_orders->viewport()->mapToGlobal(pos));
    });

    languageChange();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportOrderDialog::updateStatusLabel);
    t->start();

    if (!BrickLink::core()->orders()->rowCount())
        QMetaObject::invokeMethod(this, &ImportOrderDialog::updateOrders, Qt::QueuedConnection);

    QByteArray ba = Config::inst()->value("/MainWindow/ImportOrderDialog/Geometry"_l1)
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
    int daysBack = Config::inst()->value("/MainWindow/ImportOrderDialog/DaysBack"_l1, -1).toInt();
    if (daysBack > 0)
        w_daysBack->setValue(daysBack);
    ba = Config::inst()->value("/MainWindow/ImportOrderDialog/Filter"_l1).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportOrderDialog::~ImportOrderDialog()
{
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Geometry"_l1, saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/DaysBack"_l1, w_daysBack->value());
    Config::inst()->setValue("/MainWindow/ImportOrderDialog/Filter"_l1, w_filter->saveState());
}

void ImportOrderDialog::keyPressEvent(QKeyEvent *e)
{
    // simulate QDialog behavior
    if (e->matches(QKeySequence::Cancel)) {
        reject();
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

void ImportOrderDialog::updateOrders()
{
    if (BrickLink::core()->orders()->updateStatus() == BrickLink::UpdateStatus::Updating)
        return;

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
                                  % "<br><br>"_l1
                                  % tr("Do you want to continue and convert all prices to your default currency (%1)?")
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

            const LotList orderLots = order->lots();
            if (!orderLots.isEmpty()) {
                QColor col = QColor::fromHsl(360 * orderCount / rows.size(), 128, 128);
                for (auto &orderLot : orderLots) {
                    Lot *combinedLot = new Lot(*orderLot);
                    QString marker = orderLot->markerText();

                    combinedLot->setMarkerText(order->id() % u' ' % order->otherParty() %
                                               (marker.isEmpty() ? QString()
                                                                 : QString(u' ' % tr("Batch") % u": " % marker)));
                    combinedLot->setMarkerColor(col);

                    if (crate) {
                        combinedLot->setCost(orderLot->cost() * crate);
                        combinedLot->setPrice(orderLot->price() * crate);
                        for (int i = 0; i < 3; ++i)
                            combinedLot->setTierPrice(i, orderLot->tierPrice(i) * crate);
                    }
                    combinedPr.addLot(std::move(combinedLot));
                }
                combinedPr.setCurrencyCode(combineCCode ? defaultCCode : order->currencyCode());
            }
        } else {
            DocumentIO::importBrickLinkOrder(order);
        }
        ++orderCount;
    }
    if (combined) {
        auto doc = new Document(new DocumentModel(std::move(combinedPr))); // Document owns the items now
        doc->setTitle(tr("Multiple Orders"));
        doc->setThumbnail("view-financial-list"_l1);
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
        s = tr("Last update failed") % u": " % m_updateMessage;
        break;

    default:
        break;
    }
    w_lastUpdated->setText(s);
}

#include "moc_importorderdialog.cpp"
