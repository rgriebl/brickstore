// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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

#include <QCoro/QCoroSignal>

#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/model.h"
#include "common/application.h"
#include "common/document.h"
#include "importinventorydialog.h"
#include "inventorywidget.h"


class InventoryWidgetPrivate {
public:
    QVector<BrickLink::InventoryModel::SimpleLot> m_lots;
    InventoryWidget::Mode m_mode = InventoryWidget::Mode::AppearsIn;
    BrickLink::InventoryModel *m_model = nullptr;
    bool m_modeSet = false;
    bool m_showCanBuild = false;
    QTreeView *m_view;
    BrickLink::ItemThumbsDelegate *m_viewDelegate;
    QToolButton *m_appearsIn;
    QToolButton *m_consistsOf;
    QToolButton *m_canBuild;
    QToolButton *m_relations;
    QMenu *m_contextMenu;
    QAction *m_partOutAction;
    QAction *m_separatorAction;
    QAction *m_catalogAction;
    QAction *m_priceGuideAction;
    QAction *m_lotsForSaleAction;
    QAction *m_activateAction = nullptr;
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

    d->m_view = new QTreeView(this);
    setFrameStyle(d->m_view->frameStyle());
    d->m_view->setFrameStyle(QFrame::NoFrame);
    d->m_view->setAlternatingRowColors(true);
    d->m_view->setAllColumnsShowFocus(true);
    d->m_view->setWordWrap(true);
    d->m_view->setRootIsDecorated(false);
    d->m_view->setItemsExpandable(false);
    d->m_view->setIndentation(0);
    d->m_view->setSortingEnabled(true);
    d->m_view->sortByColumn(0, Qt::DescendingOrder);
    d->m_view->header()->setSortIndicatorShown(false);
    d->m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_viewDelegate = new BrickLink::ItemThumbsDelegate(1., d->m_view);
    d->m_viewDelegate->setSectionHeaderRole(BrickLink::IsSectionHeaderRole);
    d->m_view->setItemDelegate(d->m_viewDelegate);

    auto createButton = [this](InventoryWidget::Mode mode, const char *text, const char *iconName) {
        auto b = new QToolButton(this);
        b->setIcon(QIcon::fromTheme(QString::fromLatin1(iconName)));
        b->setCheckable(true);
        b->setChecked(false);
        b->setAutoExclusive(true);
        b->setProperty("iconScaling", true);
        b->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        connect(b, &QToolButton::toggled, b, [b](bool checked) {
            b->setToolButtonStyle(checked ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
            b->setToolTip(checked ? QString { } : b->text());
        });
        connect(b, &QToolButton::clicked, this, [this, mode]() { setMode(mode); });
        b->setProperty("bsUntranslatedText", QByteArray(text));
        return b;
    };

    d->m_appearsIn  = createButton(Mode::AppearsIn,     QT_TR_NOOP("Appears in"),  "bootstrap-box-arrow-in-right");
    d->m_consistsOf = createButton(Mode::ConsistsOf,    QT_TR_NOOP("Consists of"), "bootstrap-box-arrow-right");
    d->m_canBuild   = createButton(Mode::CanBuild,      QT_TR_NOOP("Can build"),   "bootstrap-bricks");
    d->m_relations  = createButton(Mode::Relationships, QT_TR_NOOP("Related"),     "bootstrap-share-fill");

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
    grid->addWidget(d->m_relations, 0, d->m_showCanBuild ? 3 : 2);
    grid->addWidget(d->m_view, 1, 0, 1, d->m_showCanBuild ? 4 : 3);

    d->m_partOutAction = new QAction(this);
    d->m_partOutAction->setObjectName(u"appearsin_partoutitems"_qs);
    d->m_partOutAction->setIcon(QIcon::fromTheme(u"edit-partoutitems"_qs));
    connect(d->m_partOutAction, &QAction::triggered,
            this, &InventoryWidget::partOut);
    d->m_activateAction = d->m_partOutAction;

    d->m_catalogAction = new QAction(this);
    d->m_catalogAction->setObjectName(u"appearsin_bl_catalog"_qs);
    d->m_catalogAction->setIcon(QIcon::fromTheme(u"bricklink-catalog"_qs));
    connect(d->m_catalogAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.m_item)
            Application::openUrl(BrickLink::Core::urlForCatalogInfo(entry.m_item));
    });

    d->m_priceGuideAction = new QAction(this);
    d->m_priceGuideAction->setObjectName(u"appearsin_bl_priceguide"_qs);
    d->m_priceGuideAction->setIcon(QIcon::fromTheme(u"bricklink-priceguide"_qs));
    connect(d->m_priceGuideAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.m_item) {
            Application::openUrl(BrickLink::Core::urlForPriceGuideInfo(
                                             entry.m_item,
                                             entry.m_color ? entry.m_color
                                                           : BrickLink::core()->color(0)));
        }
    });

    d->m_lotsForSaleAction = new QAction(this);
    d->m_lotsForSaleAction->setObjectName(u"appearsin_bl_lotsforsale"_qs);
    d->m_lotsForSaleAction->setIcon(QIcon::fromTheme(u"bricklink-lotsforsale"_qs));
    connect(d->m_lotsForSaleAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.m_item) {
            Application::openUrl(BrickLink::Core::urlForLotsForSale(
                                             entry.m_item,
                                             entry.m_color ? entry.m_color
                                                           : BrickLink::core()->color(0)));
        }
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

    connect(d->m_view, &QAbstractItemView::activated,
            this, [this]() {
        if (d->m_activateAction)
            d->m_activateAction->trigger();
    });

    languageChange();
    updateModel({ });
    setMode(d->m_mode);
}

InventoryWidget::~InventoryWidget()
{ /* needed to use std::unique_ptr on d */ }

void InventoryWidget::languageChange()
{
    auto translateButton = [](QToolButton *b) {
        b->setText(tr(b->property("bsUntranslatedText").toByteArray().constData()));
        if (!b->isChecked())
            b->setToolTip(b->text());
    };

    for (auto *b : { d->m_appearsIn, d->m_consistsOf, d->m_canBuild, d->m_relations })
        translateButton(b);
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
        case Mode::Relationships:
            d->m_relations->setChecked(true);
            columnVisible[BrickLink::InventoryModel::QuantityColumn] = false;
            columnVisible[BrickLink::InventoryModel::ColorColumn] = false;
            break;
        }
        for (int c = 0; c < BrickLink::InventoryModel::ColumnCount; ++c)
            d->m_view->setColumnHidden(c, !columnVisible.at(c));

        updateModel(d->m_lots);
    }
}

BrickLink::InventoryModel::SimpleLot InventoryWidget::selected() const
{
    if (d->m_model && !d->m_view->selectionModel()->selectedIndexes().isEmpty()) {
        const auto idx = d->m_view->selectionModel()->selectedIndexes().front().siblingAtColumn(0);
        auto i = d->m_model->data(idx, BrickLink::ItemPointerRole).value<const BrickLink::Item *>();
        auto c = d->m_model->data(idx, BrickLink::ColorPointerRole).value<const BrickLink::Color *>();
        auto q = d->m_model->data(idx, BrickLink::QuantityRole).toInt();
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

    static QVector<int> validModes { int(Mode::AppearsIn), int(Mode::ConsistsOf),
                                   int(Mode::CanBuild), int(Mode::Relationships) };
    d->m_viewDelegate->setZoomFactor(zoom);
    setMode(validModes.contains(mode) ? static_cast<Mode>(mode) : Mode::AppearsIn);

    return true;
}

void InventoryWidget::setActivateAction(QAction *action)
{
    if (actions().contains(action))
        d->m_activateAction = action;
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

    return { fm.horizontalAdvance(u'm') * 20, fm.height() * 4 };
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
    if ((d->m_lots == lots) && d->m_model && (d->m_model->mode() == d->m_mode))
        return;
    d->m_lots = lots;

    auto *oldModel = d->m_model;
    d->m_model = new BrickLink::InventoryModel(d->m_mode, d->m_lots, this);
    d->m_view->setModel(d->m_model);
    d->m_view->setUniformRowHeights(!d->m_model->hasSections());

    auto resizeColumns = [this]() {
        setUpdatesEnabled(false);

        if (d->m_model && d->m_model->hasSections()) {
            for (int row = 0; row < d->m_model->rowCount(); ++row) {
                if (d->m_model->index(row, 0).data(BrickLink::IsSectionHeaderRole).toBool())
                    d->m_view->setFirstColumnSpanned(row, QModelIndex { }, true);
            }
        }

        d->m_view->header()->setSectionResizeMode(BrickLink::InventoryModel::PictureColumn, QHeaderView::Fixed);
        d->m_view->header()->setSectionResizeMode(BrickLink::InventoryModel::ColorColumn, QHeaderView::Fixed);

        QStyleOptionViewItem sovi;
        struct MyTreeView : public QTreeView {
            static void publicInitViewItemOption(QTreeView *view, QStyleOptionViewItem *option) {
                (view->*(&MyTreeView::initViewItemOption))(option);
            }
        };
        MyTreeView::publicInitViewItemOption(d->m_view, &sovi);
        int colorWidth = sovi.decorationSize.width() + 4;

        d->m_view->header()->resizeSection(BrickLink::InventoryModel::ColorColumn, colorWidth);
        d->m_view->header()->setMinimumSectionSize(colorWidth); // not ideal, as this is for all columns

        d->m_view->resizeColumnToContents(BrickLink::InventoryModel::PictureColumn);
        d->m_view->resizeColumnToContents(BrickLink::InventoryModel::QuantityColumn);
        d->m_view->resizeColumnToContents(BrickLink::InventoryModel::ItemIdColumn);
        setUpdatesEnabled(true);
    };

    resizeColumns();
    connect(d->m_model, &QAbstractItemModel::modelReset, this, resizeColumns);

    delete oldModel;
}

void InventoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QFrame::changeEvent(e);
}

#include "moc_inventorywidget.cpp"
