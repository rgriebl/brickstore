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
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QPalette>
#include <QVariant>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QShortcut>
#include <QKeyEvent>
#include <QTimer>
#include <QMessageBox>

#include "bricklink/cart.h"
#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "utility/currency.h"
#include "utility/humanreadabletimedelta.h"
#include "utility/utility.h"
#include "utility/xmlhelpers.h"
#include "betteritemdelegate.h"
#include "historylineedit.h"
#include "importcartdialog.h"

using namespace std::chrono_literals;


ImportCartDialog::ImportCartDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_carts->header()->setStretchLastSection(false);
    auto proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortLocaleAware(true);
    proxyModel->setSortRole(BrickLink::Carts::CartSortRole);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);
    proxyModel->setSourceModel(BrickLink::core()->carts());
    w_carts->setModel(proxyModel);
    w_carts->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    w_carts->setItemDelegate(new BetterItemDelegate(BetterItemDelegate::AlwaysShowSelection
                                                    | BetterItemDelegate::MoreSpacing, w_carts));

    connect(w_filter, &HistoryLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { w_filter->setFocus(); });

    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, [this]() { importCarts(w_carts->selectionModel()->selectedRows()); });
    w_showOnBrickLink = new QPushButton();
    w_showOnBrickLink->setIcon(QIcon::fromTheme("bricklink"_l1));
    w_buttons->addButton(w_showOnBrickLink, QDialogButtonBox::ActionRole);
    connect(w_showOnBrickLink, &QAbstractButton::clicked,
            this, &ImportCartDialog::showCartsOnBrickLink);

    connect(w_update, &QToolButton::clicked,
            this, &ImportCartDialog::updateCarts);
    connect(w_carts->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportCartDialog::checkSelected);
    connect(w_carts, &QTreeView::activated,
            this, [this]() { w_import->animateClick(); });
    connect(BrickLink::core(), &BrickLink::Core::authenticatedTransferOverallProgress,
            this, [this](int done, int total) {
        w_progress->setVisible(done != total);
        w_progress->setMaximum(total);
        w_progress->setValue(done);
    });
    connect(BrickLink::core()->carts(), &BrickLink::Carts::updateFinished,
            this, [this](bool success, const QString &message) {
        Q_UNUSED(success)

        w_update->setEnabled(true);
        w_carts->setEnabled(true);

        m_updateMessage = message;
        updateStatusLabel();

        if (BrickLink::core()->carts()->rowCount()) {
            auto tl = w_carts->model()->index(0, 0);
            w_carts->selectionModel()->select(tl, QItemSelectionModel::SelectCurrent
                                              | QItemSelectionModel::Rows);
            w_carts->scrollTo(tl);
        }
        w_carts->header()->resizeSections(QHeaderView::ResizeToContents);
        w_carts->setFocus();

        checkSelected();
    });
    connect(BrickLink::core()->carts(), &BrickLink::Carts::fetchLotsFinished,
            this, [this](BrickLink::Cart *cart, bool success, const QString &message) {
        int idx = m_cartsToOpen.indexOf(cart ? cart->sellerId() : -1);
        if (idx >= 0)
            m_cartsToOpen.removeAt(idx);
        else
            return;

        if (!success) {
            QMessageBox::warning(this, tr("Import Cart"), message);
        } else {
            BrickLink::IO::ParseResult pr;
            const auto lots = cart->lots();
            for (const auto *lot : lots)
                pr.addLot(new Lot(*lot));
            pr.setCurrencyCode(cart->currencyCode());

            auto *document = new Document(new DocumentModel(std::move(pr))); // Document owns the items now
            document->setTitle(tr("Cart in store %1").arg(cart->storeName()));

            if (!message.isEmpty())
                QMessageBox::information(this, tr("Import Cart"), message);
        }
    });

    languageChange();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportCartDialog::updateStatusLabel);
    t->start();

    QMetaObject::invokeMethod(this, &ImportCartDialog::updateCarts, Qt::QueuedConnection);

    QByteArray ba = Config::inst()->value("/MainWindow/ImportCartDialog/Geometry"_l1)
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
    ba = Config::inst()->value("/MainWindow/ImportCartDialog/Filter"_l1).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportCartDialog::~ImportCartDialog()
{
    Config::inst()->setValue("/MainWindow/ImportCartDialog/Geometry"_l1, saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportCartDialog/Filter"_l1, w_filter->saveState());
}

void ImportCartDialog::keyPressEvent(QKeyEvent *e)
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

void ImportCartDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ImportCartDialog::languageChange()
{
    retranslateUi(this);

    w_import->setText(tr("Import"));
    w_filter->setToolTip(ActionManager::toolTipLabel(tr("Filter the list for lines containing these words"),
                                                     QKeySequence::Find, w_filter->instructionToolTip()));
    w_showOnBrickLink->setText(tr("Show"));
    w_showOnBrickLink->setToolTip(tr("Show on BrickLink"));
    updateStatusLabel();
}

void ImportCartDialog::updateCarts()
{
    if (BrickLink::core()->carts()->updateStatus() == BrickLink::UpdateStatus::Updating)
        return;

    m_updateMessage.clear();
    w_update->setEnabled(false);
    w_import->setEnabled(false);
    w_carts->setEnabled(false);

    BrickLink::core()->carts()->startUpdate();
    updateStatusLabel();
}


void ImportCartDialog::importCarts(const QModelIndexList &rows)
{
    for (auto idx : rows) {        
        auto cart = idx.data(BrickLink::Carts::CartPointerRole).value<BrickLink::Cart *>();

        BrickLink::core()->carts()->startFetchLots(cart);
        m_cartsToOpen << cart->sellerId();
    }
}

void ImportCartDialog::showCartsOnBrickLink()
{
    const auto selection = w_carts->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto cart = idx.data(BrickLink::Carts::CartPointerRole).value<BrickLink::Cart *>();
        int sellerId = cart->sellerId();
        BrickLink::core()->openUrl(BrickLink::URL_ShoppingCart, &sellerId);
    }
}

void ImportCartDialog::checkSelected()
{
    bool b = w_carts->selectionModel()->hasSelection();
    w_import->setEnabled(b);
    w_showOnBrickLink->setEnabled(b);
}

void ImportCartDialog::updateStatusLabel()
{
    QString s;

    switch (BrickLink::core()->carts()->updateStatus()) {
    case BrickLink::UpdateStatus::Ok:
        s = tr("Last updated %1").arg(
                    HumanReadableTimeDelta::toString(QDateTime::currentDateTime(),
                                                     BrickLink::core()->carts()->lastUpdated()));
        break;

    case BrickLink::UpdateStatus::Updating:
        s = tr("Currently updating carts");
        break;

    case BrickLink::UpdateStatus::UpdateFailed:
        s = tr("Last update failed") % u": " % m_updateMessage;
        break;

    default:
        break;
    }
    w_lastUpdated->setText(s);
}

#include "moc_importcartdialog.cpp"
