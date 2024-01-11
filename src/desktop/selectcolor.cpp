// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QLayout>
#include <QHeaderView>
#include <QTreeView>
#include <QEvent>
#include <QComboBox>
#include <QToolButton>
#include <QIODevice>

#include "bricklink/model.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/delegate.h"
#include "common/config.h"
#include "selectcolor.h"

class ColorTreeView : public QTreeView
{
    Q_OBJECT
public:
    ColorTreeView(QWidget *parent)
        : QTreeView(parent)
    { }

    QSize sizeHint() const override
    {
        QSize s = QTreeView::sizeHint();
        s.rheight() *= 2;
        return s;
    }
};

enum {
    KnownColors = 0,
    AllColors = -1,
    PopularColors = -2,
    MostPopularColors = -3,
};


SelectColor::SelectColor(QWidget *parent)
    : SelectColor({ }, parent)
{ }

SelectColor::SelectColor(const QVector<Feature> &features, QWidget *parent)
    : QWidget(parent)
    , m_hasLock(features.contains(Feature::ColorLock))
{
    w_filter = new QComboBox(this);
    w_filter->addItem(QString { }, int(KnownColors));
    w_filter->insertSeparator(w_filter->count());
    w_filter->addItem(QString { }, int(AllColors));
    w_filter->addItem(QString { }, int(PopularColors));
    w_filter->addItem(QString { }, int(MostPopularColors));
    w_filter->insertSeparator(w_filter->count());

    for (auto ct : BrickLink::Color::allColorTypes()) {
        if (!BrickLink::Color::typeName(ct).isEmpty())
            w_filter->addItem(QString { }, int(ct));
    }
    w_filter->setMaxVisibleItems(w_filter->count());

    w_lock = new QToolButton(this);
    w_lock->setIcon(QIcon::fromTheme(u"folder-locked"_qs));
    w_lock->setProperty("iconScaling", true);
    w_lock->setCheckable(true);
    w_lock->setChecked(false);
    w_lock->setVisible(m_hasLock);
    connect(w_lock, &QToolButton::toggled, this, &SelectColor::setColorLock);

    w_colors = new ColorTreeView(this);
    w_colors->setAlternatingRowColors(true);
    w_colors->setAllColumnsShowFocus(true);
    w_colors->setUniformRowHeights(true);
    w_colors->setRootIsDecorated(false);
    w_colors->setItemDelegate(new BrickLink::ItemDelegate(BetterItemDelegate::AlwaysShowSelection
                                                              | BetterItemDelegate::Pinnable, w_colors));
    m_colorModel = new BrickLink::ColorModel(this);
    m_colorModel->setFilterDelayEnabled(true);

    m_colorModel->setPinnedIds(Config::inst()->pinnedColorIds());
    connect(Config::inst(), &Config::pinnedColorIdsChanged, this, [this]() {
        m_colorModel->setPinnedIds(Config::inst()->pinnedColorIds());
    });
    connect(m_colorModel, &BrickLink::ColorModel::pinnedIdsChanged, this, [this]() {
        Config::inst()->setPinnedColorIds(m_colorModel->pinnedIds());
    });

    w_colors->setModel(m_colorModel);

    w_colors->header()->setSortIndicatorShown(true);
    w_colors->header()->setSectionsClickable(true);

    connect(w_colors->header(), &QHeaderView::sectionClicked,
                     this, [this](int section) {
        w_colors->sortByColumn(section, w_colors->header()->sortIndicatorOrder());
        w_colors->scrollTo(w_colors->currentIndex());
    });

    setFocusProxy(w_colors);

    connect(w_colors->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        auto col = currentColor();
        emit colorSelected(col, false);
        if (colorLock()) {
            if (col)
                emit colorLockChanged(col);
            else
                setColorLock(false);
        }
        w_lock->setEnabled(col);
    });
    connect(m_colorModel, &QAbstractItemModel::modelReset, this, [this]() {
        emit colorSelected(nullptr, false);
    });

    connect(w_colors, &QAbstractItemView::activated,
            this, &SelectColor::colorConfirmed);
    connect(w_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SelectColor::updateColorFilter);

    auto *lay = new QGridLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setRowStretch(0, 0);
    lay->setRowStretch(1, 1);
    lay->setColumnStretch(0, 1);
    lay->setColumnStretch(1, 0);
    if (m_hasLock) {
        lay->addWidget(w_filter, 0, 0);
        lay->addWidget(w_lock, 0, 1);
    } else {
        lay->addWidget(w_filter, 0, 0, 1, 2);
    }
    lay->addWidget(w_colors, 1, 0, 1, 2);

    languageChange();
}

void SelectColor::languageChange()
{
    w_filter->setItemText(w_filter->findData(KnownColors), tr("Known Colors"));
    w_filter->setItemText(w_filter->findData(AllColors), tr("All Colors"));
    w_filter->setItemText(w_filter->findData(PopularColors), tr("Popular Colors"));
    w_filter->setItemText(w_filter->findData(MostPopularColors), tr("Most Popular Colors"));

    for (auto ct : BrickLink::Color::allColorTypes()) {
        const QString ctName = BrickLink::Color::typeName(ct);
        if (!ctName.isEmpty())
            w_filter->setItemText(w_filter->findData(int(ct)), tr("Only \"%1\" Colors").arg(ctName));
    }
    w_lock->setToolTip(tr("Lock color selection: only shows items known to be available in this color"));
}

void SelectColor::setWidthToContents(bool b)
{
    if (b) {
        int w1 = w_colors->QAbstractItemView::sizeHintForColumn(0)
                + 2 * w_colors->frameWidth()
                + w_colors->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4;
        int w2 = w_filter->minimumSizeHint().width();
        int w3 = w_lock->minimumSizeHint().width() + 4;
        w_filter->setMinimumWidth(std::max(w1 - w3, w2));
        w_colors->setMinimumWidth(std::max(w1, w2 + w3));
        w_colors->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    }
}

void SelectColor::updateColorFilter(int index)
{
    int filter = index < 0 ? -1 : w_filter->itemData(index).toInt();

    auto oldColor = currentColor();

    m_colorModel->clearFilters();

    if (filter > 0) {
        m_colorModel->setColorTypeFilter(static_cast<BrickLink::ColorTypeFlag>(filter));
        m_colorModel->setPopularityFilter(0);
    } else if (filter < 0){
        float popularity = 0.f;
        if (filter == -2)
            popularity = 0.005f;
        else if (filter == -3)
            popularity = 0.05f;

        // Modulex colors are fine in their own category, but not in the 'all' lists
        m_colorModel->setColorTypeFilter(BrickLink::ColorType(BrickLink::ColorTypeFlag::Mask).setFlag(BrickLink::ColorTypeFlag::Modulex, false));
        m_colorModel->setPopularityFilter(popularity);
    } else if (filter == 0 && m_item) {
        const auto known = m_item->knownColors();
        m_colorModel->setColorListFilter(known);
        if (known.isEmpty())
            m_colorModel->setColorTypeFilter(BrickLink::ColorType(BrickLink::ColorTypeFlag::Mask).setFlag(BrickLink::ColorTypeFlag::Modulex, false));
    }

    m_colorModel->invalidateFilterNow();

    auto colorIndex = m_colorModel->index(oldColor);
    if (colorIndex.isValid())
        w_colors->setCurrentIndex(colorIndex);
    else
        w_colors->clearSelection();
    w_colors->scrollTo(colorIndex);
}

const BrickLink::Color *SelectColor::currentColor() const
{
    if (w_colors->selectionModel()->hasSelection()) {
        QModelIndex idx = w_colors->selectionModel()->selectedIndexes().front();
        return qvariant_cast<const BrickLink::Color *>(w_colors->model()->data(idx, BrickLink::ColorPointerRole));
    }
    return nullptr;
}

bool SelectColor::colorLock() const
{
    return m_hasLock && m_locked;
}

void SelectColor::setColorLock(bool locked)
{
    if (!m_hasLock)
        return;

    if (locked != m_locked) {
        m_locked = locked;
        w_lock->setChecked(locked);
        emit colorLockChanged(locked ? currentColor() : nullptr);
    }
}

QByteArray SelectColor::saveState() const
{
    auto col = currentColor();

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SC") << qint32(2)
       << (col ? col->id() : uint(-1))
       << qint8(m_item && m_item->itemType() ? m_item->itemTypeId() : char(-1))
       << (m_item ? QString::fromLatin1(m_item->id()) : QString())
       << w_filter->currentIndex()
       << (w_colors->header()->sortIndicatorOrder() == Qt::AscendingOrder)
       << colorLock();

    return ba;
}

bool SelectColor::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SC") || (version < 1) || (version > 2))
        return false;

    uint col;
    qint8 itt;
    QString itemid;
    int filterIndex;
    bool colSortAsc;
    bool colLock = false;

    ds >> col >> itt >> itemid >> filterIndex >> colSortAsc;

    if (version >= 2)
        ds >> colLock;

    if (ds.status() != QDataStream::Ok)
        return false;

    if (col != uint(-1))
        setCurrentColorAndItem(BrickLink::core()->color(col), BrickLink::core()->item(itt, itemid.toLatin1()));
    w_filter->setCurrentIndex(filterIndex);

    w_colors->sortByColumn(0, colSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    setColorLock(colLock);

    return true;
}

QByteArray SelectColor::defaultState()
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SC") << qint32(1)
       << uint(0)
       << qint8(-1)
       << QString()
       << int(0)
       << true;
    return ba;
}

void SelectColor::setCurrentColor(const BrickLink::Color *color)
{
    setCurrentColorAndItem(color, nullptr);
}

void SelectColor::setCurrentColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item)
{
    m_item = item;

    updateColorFilter(w_filter->currentIndex());

    auto colorIndex = m_colorModel->index(color);
    if (colorIndex.isValid())
        w_colors->setCurrentIndex(colorIndex);
    else
        w_colors->clearSelection();
    w_colors->scrollTo(colorIndex);
}

void SelectColor::colorConfirmed()
{
    emit colorSelected(currentColor(), true);
}

void SelectColor::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    if (const BrickLink::Color *color = currentColor())
        w_colors->scrollTo(m_colorModel->index(color), QAbstractItemView::PositionAtCenter);
}

void SelectColor::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

#include "moc_selectcolor.cpp"
#include "selectcolor.moc"
