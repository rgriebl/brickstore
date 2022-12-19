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
#include <QMenu>
#include <QVariant>
#include <QAction>
#include <QActionEvent>
#include <QHeaderView>
#include <QApplication>
#include <QBitArray>
#include <QTreeView>
#include <QToolButton>
#include <QGridLayout>

#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/model.h"
#include "common/document.h"
#include "importinventorydialog.h"

#include "inventorywidget.h"


class InventoryWidgetPrivate {
public:
    QVector<BrickLink::InventoryModel::SimpleLot> m_lots;
    InventoryWidget::Mode m_mode = InventoryWidget::Mode::AppearsIn;
    bool m_modeSet = false;
    bool m_showCanBuild = false;
    QTreeView *m_view;
    BrickLink::ItemThumbsDelegate *m_viewDelegate;
    QToolButton *m_appearsIn;
    QToolButton *m_consistsOf;
    QToolButton *m_canBuild;
    QMenu *m_contextMenu;
    QAction *m_partOutAction;
    QAction *m_separatorAction;
    QAction *m_catalogAction;
    QAction *m_priceGuideAction;
    QAction *m_lotsForSaleAction;
};

InventoryWidget::InventoryWidget(QWidget *parent)
    : InventoryWidget(false, parent)
{ }

InventoryWidget::InventoryWidget(bool showCanBuild, QWidget *parent)
    : QFrame(parent)
    , d(new InventoryWidgetPrivate())
{
    d->m_showCanBuild = showCanBuild;

    d->m_contextMenu = new QMenu(this);

    d->m_view = new QTreeView();
    setFrameStyle(d->m_view->frameStyle());
    d->m_view->setFrameStyle(QFrame::NoFrame);
    d->m_view->setAlternatingRowColors(true);
    d->m_view->setAllColumnsShowFocus(true);
    d->m_view->setUniformRowHeights(true);
    d->m_view->setWordWrap(true);
    d->m_view->setRootIsDecorated(false);
    d->m_view->setSortingEnabled(true);
    d->m_view->sortByColumn(0, Qt::DescendingOrder);
    d->m_view->header()->setSortIndicatorShown(false);
    d->m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_viewDelegate = new BrickLink::ItemThumbsDelegate(1., d->m_view);
    d->m_view->setItemDelegate(d->m_viewDelegate);

    d->m_appearsIn = new QToolButton();
    d->m_appearsIn->setCheckable(true);
    d->m_appearsIn->setChecked(true);
    d->m_appearsIn->setAutoExclusive(true);
    d->m_appearsIn->setAutoRaise(true);
    d->m_appearsIn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(d->m_appearsIn, &QToolButton::clicked, this, [this]() { setMode(Mode::AppearsIn); });

    d->m_consistsOf = new QToolButton();
    d->m_consistsOf->setCheckable(true);
    d->m_consistsOf->setAutoExclusive(true);
    d->m_consistsOf->setAutoRaise(true);
    d->m_consistsOf->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(d->m_consistsOf, &QToolButton::clicked, this, [this]() { setMode(Mode::ConsistsOf); });

    d->m_canBuild = new QToolButton();
    d->m_canBuild->setCheckable(true);
    d->m_canBuild->setAutoExclusive(true);
    d->m_canBuild->setAutoRaise(true);
    d->m_canBuild->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(d->m_canBuild, &QToolButton::clicked, this, [this]() { setMode(Mode::CanBuild); });

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->setRowStretch(1, 10);
    grid->addWidget(d->m_appearsIn, 0, 0);
    grid->addWidget(d->m_consistsOf, 0, 1);
    if (d->m_showCanBuild)
        grid->addWidget(d->m_canBuild, 0, 2);
    else
        d->m_canBuild->hide();
    grid->addWidget(d->m_view, 1, 0, 1, d->m_showCanBuild ? 3 : 2);

    d->m_partOutAction = new QAction(this);
    d->m_partOutAction->setObjectName(u"appearsin_partoutitems"_qs);
    d->m_partOutAction->setIcon(QIcon::fromTheme(u"edit-partoutitems"_qs));
    connect(d->m_partOutAction, &QAction::triggered,
            this, &InventoryWidget::partOut);

    d->m_catalogAction = new QAction(this);
    d->m_catalogAction->setObjectName(u"appearsin_bl_catalog"_qs);
    d->m_catalogAction->setIcon(QIcon::fromTheme(u"bricklink-catalog"_qs));
    connect(d->m_catalogAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.m_item)
            BrickLink::core()->openUrl(BrickLink::Url::CatalogInfo, entry.m_item);
    });

    d->m_priceGuideAction = new QAction(this);
    d->m_priceGuideAction->setObjectName(u"appearsin_bl_priceguide"_qs);
    d->m_priceGuideAction->setIcon(QIcon::fromTheme(u"bricklink-priceguide"_qs));
    connect(d->m_priceGuideAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.m_item)
            BrickLink::core()->openUrl(BrickLink::Url::PriceGuideInfo, entry.m_item,
                                       entry.m_color ? entry.m_color : BrickLink::core()->color(0));
    });

    d->m_lotsForSaleAction = new QAction(this);
    d->m_lotsForSaleAction->setObjectName(u"appearsin_bl_lotsforsale"_qs);
    d->m_lotsForSaleAction->setIcon(QIcon::fromTheme(u"bricklink-lotsforsale"_qs));
    connect(d->m_lotsForSaleAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.m_item)
            BrickLink::core()->openUrl(BrickLink::Url::LotsForSale, entry.m_item,
                                       entry.m_color ? entry.m_color : BrickLink::core()->color(0));
    });

    d->m_contextMenu->addAction(d->m_partOutAction);
    d->m_separatorAction = d->m_contextMenu->addSeparator();
    d->m_contextMenu->addAction(d->m_catalogAction);
    d->m_contextMenu->addAction(d->m_priceGuideAction);
    d->m_contextMenu->addAction(d->m_lotsForSaleAction);

    connect(d->m_view, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        if (auto item = selected().m_item) {
            d->m_partOutAction->setEnabled(item->hasInventory());
            d->m_contextMenu->popup(d->m_view->viewport()->mapToGlobal(pos));
        }
    });

    connect(d->m_view, &QAbstractItemView::doubleClicked,
            this, &InventoryWidget::partOut);

    languageChange();
    updateModel({ });
    setMode(d->m_mode);
}

InventoryWidget::~InventoryWidget()
{ /* needed to use std::unique_ptr on d */ }

void InventoryWidget::languageChange()
{
    d->m_appearsIn->setText(tr("Appears in"));
    d->m_consistsOf->setText(tr("Consists of"));
    d->m_canBuild->setText(tr("Can build"));
    d->m_partOutAction->setText(tr("Part out Item..."));
    d->m_catalogAction->setText(tr("Show BrickLink Catalog Info..."));
    d->m_priceGuideAction->setText(tr("Show BrickLink Price Guide Info..."));
    d->m_lotsForSaleAction->setText(tr("Show Lots for Sale on BrickLink..."));
}

void InventoryWidget::actionEvent(QActionEvent *e)
{
    switch (e->type()) {
    case QEvent::ActionAdded:
        d->m_contextMenu->insertAction(e->before() ? e->before() : d->m_separatorAction, e->action());
        break;
    case QEvent::ActionRemoved:
        d->m_contextMenu->removeAction(e->action());
        break;
    default:
        break;
    }
    QFrame::actionEvent(e);
}

InventoryWidget::Mode InventoryWidget::mode() const
{
    return d->m_mode;
}

void InventoryWidget::setMode(Mode newMode)
{
    if ((d->m_mode != newMode) || !d->m_modeSet) {
        d->m_mode = newMode;   
        d->m_modeSet = true;

        QBitArray columnVisible { BrickLink::InventoryModel::ColumnCount, true };
        switch (d->m_mode) {
        case Mode::AppearsIn:
            d->m_appearsIn->setChecked(true);
            columnVisible[BrickLink::InventoryModel::ColorColumn] = false;
            break;
        case Mode::CanBuild:
            d->m_canBuild->setChecked(true);
            columnVisible[BrickLink::InventoryModel::QuantityColumn] = false;
            columnVisible[BrickLink::InventoryModel::ColorColumn] = false;
            break;
        case Mode::ConsistsOf:
            d->m_consistsOf->setChecked(true);
            break;
        }
        for (int c = 0; c < BrickLink::InventoryModel::ColumnCount; ++c)
            d->m_view->setColumnHidden(c, !columnVisible.at(c));

        updateModel(d->m_lots);
    }
}

BrickLink::InventoryModel::SimpleLot InventoryWidget::selected() const
{
    auto *m = qobject_cast<BrickLink::InventoryModel *>(d->m_view->model());

    if (m && !d->m_view->selectionModel()->selectedIndexes().isEmpty()) {
        const auto idx = d->m_view->selectionModel()->selectedIndexes().front().siblingAtColumn(0);
        auto i = m->data(idx, BrickLink::ItemPointerRole).value<const BrickLink::Item *>();
        auto c = m->data(idx, BrickLink::ColorPointerRole).value<const BrickLink::Color *>();
        auto q = m->data(idx, BrickLink::QuantityRole).toInt();
        return BrickLink::InventoryModel::SimpleLot { i, c, q };
    } else {
        return { };
    }
}

QByteArray InventoryWidget::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("IW") << qint32(1)
       << qint32(d->m_mode)
       << d->m_viewDelegate->zoomFactor();

    return ba;
}

bool InventoryWidget::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "IW") || (version < 1) || (version > 1))
        return false;

    qint32 mode;
    double zoom;
    ds >> mode >> zoom;
    if (ds.status() != QDataStream::Ok)
        return false;

    static QVector<int> validModes { int(Mode::AppearsIn), int(Mode::ConsistsOf), int(Mode::CanBuild) };
    d->m_viewDelegate->setZoomFactor(zoom);
    setMode(validModes.contains(mode) ? static_cast<Mode>(mode) : Mode::AppearsIn);

    return true;
}

QCoro::Task<> InventoryWidget::partOut()
{
    auto entry = selected();

    if (entry.m_item && entry.m_item->hasInventory()) {
        ImportInventoryDialog dlg(entry.m_item, 1, BrickLink::Condition::Count, this);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.show();

        if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
            Document::fromPartInventory(dlg.item(), nullptr, dlg.quantity(), dlg.condition(),
                                        dlg.extraParts(), dlg.partOutTraits());
        }
    }
}

QSize InventoryWidget::minimumSizeHint() const
{
    const QFontMetrics &fm = fontMetrics();

    return { fm.horizontalAdvance(QLatin1Char('m')) * 20, fm.height() * 4 };
}

QSize InventoryWidget::sizeHint() const
{
    return minimumSizeHint();
}

void InventoryWidget::setItem(const BrickLink::Item *item, const BrickLink::Color *color, int quantity)
{
    BrickLink::InventoryModel::SimpleLot simpleLot(item, color, quantity);
    updateModel({ simpleLot });
}

void InventoryWidget::setItems(const LotList &lots)
{
    QVector<BrickLink::InventoryModel::SimpleLot> simpleLots;
    simpleLots.reserve(lots.size());
    for (const auto &lot : lots)
        simpleLots.emplace_back(lot->item(), lot->color(), lot->quantity());
    updateModel(simpleLots);
}

void InventoryWidget::updateModel(const QVector<BrickLink::InventoryModel::SimpleLot> &lots)
{
    d->m_lots = lots;

    auto *oldModel = d->m_view->model();
    auto *newModel = new BrickLink::InventoryModel(d->m_mode, d->m_lots, this);
    d->m_view->setModel(newModel);

    auto resizeColumns = [this]() {
        setUpdatesEnabled(false);
        d->m_view->header()->setSectionResizeMode(BrickLink::InventoryModel::PictureColumn, QHeaderView::Fixed);
        d->m_view->header()->setSectionResizeMode(BrickLink::InventoryModel::ColorColumn, QHeaderView::Fixed);

        QStyleOptionViewItem sovi;
        struct MyTreeView : public QTreeView {
            using QTreeView::initViewItemOption;
        };
        static_cast<const MyTreeView *>(d->m_view)->initViewItemOption(&sovi);
        int colorWidth = sovi.decorationSize.width() + 4;

        d->m_view->header()->resizeSection(BrickLink::InventoryModel::ColorColumn, colorWidth);
        d->m_view->header()->setMinimumSectionSize(colorWidth); // not ideal, as this is for all columns

        d->m_view->resizeColumnToContents(BrickLink::InventoryModel::PictureColumn);
        d->m_view->resizeColumnToContents(BrickLink::InventoryModel::QuantityColumn);
        d->m_view->resizeColumnToContents(BrickLink::InventoryModel::ItemIdColumn);
        setUpdatesEnabled(true);
    };

    resizeColumns();
    connect(newModel, &QAbstractItemModel::modelReset, this, resizeColumns);

    delete oldModel;
}

void InventoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QFrame::changeEvent(e);
}

#include "moc_inventorywidget.cpp"
