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
#include <QPainter>
#include <QStyle>
#include <QList>
#include <QMenu>
#include <QCursor>
#include <QAction>
#include <QToolTip>
#include <QEvent>
#include <QPaintEvent>
#include <QStyleOptionHeader>
#include <QDesktopServices>
#include <QApplication>

#include "bricklink/core.h"
#include "bricklink/priceguide.h"
#include "common/application.h"
#include "common/config.h"
#include "common/currency.h"
#include "priceguidewidget.h"

namespace {

static const int hborder = 8;
static const int vborder = 3;

struct cell : public QRect {
    enum cell_type {
        Header,
        VatHeader,
        Quantity,
        Price,
        Update,
        Empty
    };

    cell(cell_type t, int x, int y, int w, int h, int tfl = Qt::AlignLeft, const QString &str = {},
         bool flag = false)
        : QRect(x, y, w, h), m_type(t), m_text_flags(tfl), m_text(str), m_flag(flag)
    { }

    cell_type     m_type;
    Qt::Alignment m_text_flags;
    QString       m_text;
    bool          m_flag;

    BrickLink::Price     m_price;
    BrickLink::Time      m_time;
    BrickLink::Condition m_condition;
};

} // namespace

class PriceGuideWidgetPrivate
{
public:
    explicit PriceGuideWidgetPrivate(PriceGuideWidget *parent)
        : m_widget(parent)
    { }

    const cell *cellAtPos(const QPoint &fpos)
    {
        QPoint pos = fpos - QPoint(m_widget->frameWidth(), m_widget->frameWidth());

        auto it = std::find_if(m_cells.cbegin(), m_cells.cend(), [pos](const cell &cell) {
            return cell.contains(pos) && (cell.m_type != cell::Update);
        });
        if (it == m_cells.cend())
            return nullptr;
        else
            return &(*it);
    }

private:
    Q_DISABLE_COPY(PriceGuideWidgetPrivate)
    friend class PriceGuideWidget;

    PriceGuideWidget *        m_widget;
    BrickLink::PriceGuide *   m_pg = nullptr;
    std::vector<cell>         m_cells;
    const cell *              m_cellUnderMouse = nullptr;
    QString                   m_ccode;
    PriceGuideWidget::Layout  m_layout = PriceGuideWidget::Normal;

    static constexpr int s_cond_count = int(BrickLink::Condition::Count);
    static constexpr int s_time_count = int(BrickLink::Time::Count);
    static constexpr int s_price_count = int(BrickLink::Price::Count);

    QString m_str_qty;
    QString m_str_cond[s_cond_count];
    QString m_str_price[s_price_count];
    QString m_str_vtime[s_time_count];
    QString m_str_htime[s_time_count][2];
    QString m_str_wait;

    QIcon m_vatIcon;
    QString m_vatDescription;
};

PriceGuideWidget::PriceGuideWidget(QWidget *parent)
    : QTreeView(parent)
    , d(new PriceGuideWidgetPrivate(this))
{
    d->m_ccode = Config::inst()->defaultCurrencyCode();
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &PriceGuideWidget::updateNonStaticCells);

    connect(BrickLink::core()->priceGuideCache(), &BrickLink::PriceGuideCache::priceGuideUpdated,
            this, &PriceGuideWidget::gotUpdate);

    connect(BrickLink::core()->priceGuideCache(), &BrickLink::PriceGuideCache::currentVatTypeChanged,
            this, &PriceGuideWidget::updateVatType);

    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction *a;
    a = new QAction(this);
    a->setObjectName(u"priceguide_reload"_qs);
    a->setIcon(QIcon::fromTheme(u"view-refresh"_qs));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::doUpdate);
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName(u"priceguide_bl_catalog"_qs);
    a->setIcon(QIcon::fromTheme(u"bricklink-catalog"_qs));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::showBLCatalogInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName(u"priceguide_bl_priceguide"_qs);
    a->setIcon(QIcon::fromTheme(u"bricklink-priceguide"_qs));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::showBLPriceGuideInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName(u"priceguide_bl_lotsforsale"_qs);
    a->setIcon(QIcon::fromTheme(u"bricklink-lotsforsale"_qs));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::showBLLotsForSale);
    addAction(a);

    languageChange();
    updateVatType(BrickLink::core()->priceGuideCache()->currentVatType());
}

void PriceGuideWidget::languageChange()
{
    findChild<QAction *>(u"priceguide_reload"_qs)->setText(tr("Update"));
    findChild<QAction *>(u"priceguide_bl_catalog"_qs)->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *>(u"priceguide_bl_priceguide"_qs)->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *>(u"priceguide_bl_lotsforsale"_qs)->setText(tr("Show Lots for Sale on BrickLink..."));

    d->m_str_qty                                     = tr("Qty.");
    d->m_str_cond[int(BrickLink::Condition::New)]    = tr("New");
    d->m_str_cond[int(BrickLink::Condition::Used)]   = tr("Used");
    d->m_str_price[int(BrickLink::Price::Lowest)]    = tr("Min.");
    d->m_str_price[int(BrickLink::Price::Average)]   = tr("Avg.");
    d->m_str_price[int(BrickLink::Price::WAverage)]  = tr("Q.Avg.");
    d->m_str_price[int(BrickLink::Price::Highest)]   = tr("Max.");
    d->m_str_htime[int(BrickLink::Time::PastSix)][0] = tr("Last 6");
    d->m_str_htime[int(BrickLink::Time::PastSix)][1] = tr("Months Sales");
    d->m_str_htime[int(BrickLink::Time::Current)][0] = tr("Current");
    d->m_str_htime[int(BrickLink::Time::Current)][1] = tr("Inventory");
    d->m_str_vtime[int(BrickLink::Time::PastSix)]    = tr("Last 6 Months Sales");
    d->m_str_vtime[int(BrickLink::Time::Current)]    = tr("Current Inventory");
    d->m_str_wait                                    = tr("Please wait... updating");

    recalcLayout();
}

PriceGuideWidget::~PriceGuideWidget()
{
    if (d->m_pg)
        d->m_pg->release();
}

void PriceGuideWidget::showBLCatalogInfo()
{
    if (d->m_pg && d->m_pg->item())
        Application::openUrl(BrickLink::Core::urlForCatalogInfo(d->m_pg->item(), d->m_pg->color()));
}

void PriceGuideWidget::showBLPriceGuideInfo()
{
    if (d->m_pg && d->m_pg->item() && d->m_pg->color())
        Application::openUrl(BrickLink::Core::urlForPriceGuideInfo(d->m_pg->item(), d->m_pg->color()));
}

void PriceGuideWidget::showBLLotsForSale()
{
    if (d->m_pg && d->m_pg->item() && d->m_pg->color())
        Application::openUrl(BrickLink::Core::urlForLotsForSale(d->m_pg->item(), d->m_pg->color()));
}

QSize PriceGuideWidget::sizeHint() const
{
    return minimumSize();
}

void PriceGuideWidget::doUpdate()
{
    if (d->m_pg)
        d->m_pg->update(true);
    updateNonStaticCells();
}


void PriceGuideWidget::gotUpdate(BrickLink::PriceGuide *pg)
{
    if (pg == d->m_pg)
        updateNonStaticCells();
}


void PriceGuideWidget::setPriceGuide(BrickLink::PriceGuide *pg)
{
    if (pg == d->m_pg)
        return;

    if (d->m_pg)
        d->m_pg->release();
    if (pg)
        pg->addRef();
    d->m_pg = pg;

    setContextMenuPolicy(pg ? Qt::ActionsContextMenu : Qt::NoContextMenu);

    updateNonStaticCells();
}

BrickLink::PriceGuide *PriceGuideWidget::priceGuide() const
{
    return d->m_pg;
}

void PriceGuideWidget::setLayout(Layout l)
{
    if (l != d->m_layout) {
        d->m_layout = l;
        recalcLayout();
    }
}

void PriceGuideWidget::resizeEvent(QResizeEvent *e)
{
    QFrame::resizeEvent(e);
    recalcLayout();
}

void PriceGuideWidget::recalcLayout()
{
    d->m_cells.clear();
    d->m_cellUnderMouse = nullptr;

    ensurePolished();
    const QFontMetrics &fm = fontMetrics();

    QFont fb = font();
    fb.setBold(true);
    QFontMetrics fmb(fb);

    QSize s = contentsRect().size();

    switch (d->m_layout) {
    case Normal    : recalcLayoutNormal(s, fm, fmb); break;
    case Horizontal: recalcLayoutHorizontal(s, fm, fmb); break;
    case Vertical  : recalcLayoutVertical(s, fm, fmb); break;
    }
    viewport()->update();
}

void PriceGuideWidget::recalcLayoutNormal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb)
{
    int cw[4];
    int ch = fm.height() + 2 * vborder;

    int dx, dy;

    cw[0] = 0; // not used
    cw[1] = 0;
    for (const auto &cond : d->m_str_cond)
        cw[1] = std::max(cw[1], fm.horizontalAdvance(cond));
    cw[2] = std::max(fm.horizontalAdvance(d->m_str_qty), fm.horizontalAdvance(u"9.000.000 (90.000)"_qs));
    cw[3] = fm.horizontalAdvance(Currency::toDisplayString(9000));
    for (const auto &price : d->m_str_price)
        cw[3] = std::max(cw[3], fm.horizontalAdvance(price));

    for (int i = 1; i < 4; i++)
        cw[i] += (2 * hborder);

    dx = 0;
    for (const auto &vtime : d->m_str_vtime)
        dx = std::max(dx, fmb.horizontalAdvance(vtime));

    if ((cw[1] + cw[2] + d->s_price_count * cw[3]) < dx)
        cw[1] = dx - (cw[2] + d->s_price_count * cw[3]);

    setMinimumSize(2 * frameWidth() + cw[1] + cw[2] + d->s_price_count * cw[3],
                   2 * frameWidth() + (1 + (1 + d->s_cond_count) * d->s_time_count) * ch);

    d->m_cells.emplace_back(cell::Header, 0, 0, cw[1], ch, Qt::AlignCenter, u"$$$"_qs, true);
    d->m_cells.emplace_back(cell::Header, cw[1], 0, cw[2], ch, Qt::AlignCenter, d->m_str_qty);

    dx = cw[1] + cw[2];

    for (const auto &price : d->m_str_price) {
        d->m_cells.emplace_back(cell::Header, dx, 0, cw[3], ch, Qt::AlignCenter, price);
        dx += cw[3];
    }

    d->m_cells.emplace_back(cell::Header, dx, 0, s.width() - dx, ch, 0, QString());

    dx = cw[1] + cw[2] + cw[3] * d->s_price_count;
    dy = ch;

    for (int tc = 0; tc < d->s_time_count; ++tc) {
        if (tc == 0)
            d->m_cells.emplace_back(cell::VatHeader, 0, dy, cw[1], ch, Qt::AlignCenter, u"%%%"_qs, false);
        else
            d->m_cells.emplace_back(cell::Header, 0, dy, cw[1], ch);
        d->m_cells.emplace_back(cell::Header, cw[1], dy, dx - cw[1], ch, Qt::AlignCenter, d->m_str_vtime[tc], true);
        dy += ch;

        for (int j = 0; j < d->s_cond_count; j++)
            d->m_cells.emplace_back(cell::Header, 0, dy + j * ch, cw[1], ch, Qt::AlignCenter, d->m_str_cond [j]);

        d->m_cells.emplace_back(cell::Update, cw[1], dy, dx - cw[1], ch * d->s_cond_count, Qt::AlignCenter | Qt::TextWordWrap, QString());
        dy += (d->s_cond_count * ch);
    }

    d->m_cells.emplace_back(cell::Header, 0, dy, cw[1], s.height() - dy, 0, QString());

    dy = ch;
    bool flip = false;

    for (const auto time : { BrickLink::Time::PastSix, BrickLink::Time::Current }) {
        dy += ch;

        for (const auto cond : { BrickLink::Condition::New, BrickLink::Condition::Used }) {
            dx = cw[1];

            cell cq(cell::Quantity, dx, dy, cw[2], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), flip);
            cq.m_time = time;
            cq.m_condition = cond;
            cq.m_price = BrickLink::Price::Count;
            d->m_cells.push_back(cq);

            dx += cw[2];

            for (const auto price : { BrickLink::Price::Lowest, BrickLink::Price::Average,
                 BrickLink::Price::WAverage, BrickLink::Price::Highest }) {
                cell cp(cell::Price, dx, dy, cw[3], ch, Qt::AlignRight  | Qt::AlignVCenter, QString(), flip);
                cp.m_time      = time;
                cp.m_condition = cond;
                cp.m_price     = price;
                d->m_cells.push_back(cp);

                dx += cw[3];
            }
            dy += ch;
            flip = !flip;
        }
    }
}


void PriceGuideWidget::recalcLayoutHorizontal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb)
{
    int cw[4];
    int ch = fm.height() + 2 * vborder;

    int dx, dy;

    cw[0] = 0;
    for (auto &htime : d->m_str_htime)
        cw[0] = std::max({ cw[0], fmb.horizontalAdvance(htime[0]), fmb.horizontalAdvance(htime[1]) });
    cw[1] = 0;
    for (const auto &cond : d->m_str_cond)
        cw[1] = std::max(cw[1], fm.horizontalAdvance(cond));
    cw[2] = std::max(fm.horizontalAdvance(d->m_str_qty), fm.horizontalAdvance(u"9.000.000 (90.000)"_qs));
    cw[3] = fm.horizontalAdvance(Currency::toDisplayString(9000));
    for (const auto &price : d->m_str_price)
        cw[3] = std::max(cw[3], fm.horizontalAdvance(price));

    for (int &i : cw)
        i += (2 * hborder);

    setMinimumSize(2 * frameWidth() + cw[0] + cw[1] + cw[2] + d->s_price_count * cw[3],
                   2 * frameWidth() + (1 + d->s_cond_count * d->s_time_count) * ch);
    setMaximumWidth(minimumWidth());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    dx = cw[0] + cw[1];
    dy = 0;

    d->m_cells.emplace_back(cell::Header, 0,  dy, cw[0], ch, Qt::AlignCenter, u"$$$"_qs, true);
    d->m_cells.emplace_back(cell::VatHeader, cw[0], dy, cw[1], ch, Qt::AlignCenter, u"%%%"_qs, false);
    d->m_cells.emplace_back(cell::Header, dx, dy, cw[2], ch, Qt::AlignCenter, d->m_str_qty);

    dx += cw[2];

    for (const auto &price : d->m_str_price) {
        d->m_cells.emplace_back(cell::Header, dx, dy, cw[3], ch, Qt::AlignCenter, price);
        dx += cw[3];
    }

    d->m_cells.emplace_back(cell::Header, dx, dy, s.width() - dx, ch, 0, QString());

    dx = 0;
    dy = ch;

    for (const auto &htime : d->m_str_htime) {
        d->m_cells.emplace_back(cell::Header, dx, dy, cw[0], d->s_cond_count * ch, Qt::AlignLeft | Qt::AlignVCenter, htime[0] + u'\n' + htime[1], true);

        for (const auto &cond : d->m_str_cond) {
            d->m_cells.emplace_back(cell::Header, dx + cw[0], dy, cw[1], ch, Qt::AlignCenter, cond);
            dy += ch;
        }
    }

    d->m_cells.emplace_back(cell::Header, dx, dy, cw[0] + cw[1], s.height() - dy, 0, QString());

    d->m_cells.emplace_back(cell::Update, cw[0] + cw[1], ch, cw[2] + d->s_price_count * cw[3], d->s_time_count * d->s_cond_count * ch, Qt::AlignCenter | Qt::TextWordWrap, QString());

    dy = ch;
    bool flip = false;

    for (const auto time : { BrickLink::Time::PastSix, BrickLink::Time::Current }) {
        for (const auto cond : { BrickLink::Condition::New, BrickLink::Condition::Used }) {
            dx = cw[0] + cw[1];

            cell cq(cell::Quantity, dx, dy, cw[2], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), flip);
            cq.m_time = time;
            cq.m_condition = cond;
            cq.m_price = BrickLink::Price::Count;
            d->m_cells.push_back(cq);
            dx += cw[2];

            for (const auto price : { BrickLink::Price::Lowest, BrickLink::Price::Average,
                 BrickLink::Price::WAverage, BrickLink::Price::Highest }) {
                cell cp(cell::Price, dx, dy, cw[3], ch, Qt::AlignRight  | Qt::AlignVCenter, QString(), flip);
                cp.m_time      = time;
                cp.m_condition = cond;
                cp.m_price     = price;
                d->m_cells.push_back(cp);
                dx += cw[3];
            }
            dy += ch;
            flip = !flip;
        }
    }
}


void PriceGuideWidget::recalcLayoutVertical(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb)
{
    int cw[2];
    int ch = fm.height() + 2 * vborder;

    int dx, dy;

    cw[0] = fm.horizontalAdvance(d->m_str_qty);

    for (const auto &price : d->m_str_price)
        cw[0] = std::max(cw[0], fm.horizontalAdvance(price));
    cw[0] += 2 * hborder;

    cw[1] = std::max(fm.horizontalAdvance(Currency::toDisplayString(9000)),
                     fm.horizontalAdvance(u"9.000.000 (90.000)"_qs));
    for (const auto &cond : d->m_str_cond)
        cw[1] = std::max(cw[1], fm.horizontalAdvance(cond));
    cw[1] += 2 * hborder;

    dx = 0;
    for (const auto &vtime : d->m_str_vtime)
        dx = std::max(dx, fmb.horizontalAdvance(vtime));

    if (dx > (cw[0] + d->s_cond_count * cw[1])) {
        dx -= (cw[0] + d->s_cond_count * cw[1]);
        dx = (dx + d->s_cond_count) / (d->s_cond_count + 1);

        cw[0] += dx;
        cw[1] += dx;
    }
    setMinimumSize(2 * frameWidth() + cw[0] + d->s_cond_count * cw[1],
                   2 * frameWidth() + (1 + d->s_time_count * (2 + d->s_price_count)) * ch);

    if (minimumWidth() < s.width()) {
        int freeSpace = s.width() - minimumWidth();
        int delta = freeSpace / (d->s_cond_count * 2 + 1);
        cw[0] += (freeSpace - delta * d->s_cond_count * 2);
        cw[1] += delta * 2;
    }

    dx = 0;
    dy = 0;

    d->m_cells.emplace_back(cell::Header, dx, dy, cw[0], ch, Qt::AlignCenter, u"$$$"_qs, true);
    dx += cw[0];

    for (const auto &cond : d->m_str_cond) {
        d->m_cells.emplace_back(cell::Header, dx, dy, cw[1], ch, Qt::AlignCenter, cond);
        dx += cw[1];
    }

    d->m_cells.emplace_back(cell::Header, dx, dy, s.width() - dx, ch, 0, QString());
    dy += ch;

    d->m_cells.emplace_back(cell::Empty, dx, dy, s.width() - dx, s.height() - dy, 0, QString());

    dx = 0;
    dy = ch;

    for (int tc = 0; tc < d->s_time_count; ++tc) {
        if (tc == 0)
            d->m_cells.emplace_back(cell::VatHeader, dx, dy, cw[0], ch, Qt::AlignCenter, u"%%%"_qs, false);
        else
            d->m_cells.emplace_back(cell::Header, dx, dy, cw[0], ch);

        d->m_cells.emplace_back(cell::Header, dx + cw[0], dy, d->s_cond_count * cw[1], ch, Qt::AlignCenter, d->m_str_vtime[tc], true);
        dy += ch;

        d->m_cells.emplace_back(cell::Header, dx, dy, cw[0], ch, Qt::AlignLeft | Qt::AlignVCenter, d->m_str_qty, false);

        d->m_cells.emplace_back(cell::Update, dx + cw[0], dy, d->s_cond_count * cw[1], (1 + d->s_price_count) * ch, Qt::AlignCenter | Qt::TextWordWrap, QString());
        dy += ch;

        for (const auto &price : d->m_str_price) {
            d->m_cells.emplace_back(cell::Header, dx, dy, cw[0], ch, Qt::AlignLeft | Qt::AlignVCenter, price, false);
            dy += ch;
        }
    }
    d->m_cells.emplace_back(cell::Header, dx, dy, cw[0], s.height() - dy, 0, QString());

    d->m_cells.emplace_back(cell::Empty, dx + cw[0], dy, d->s_cond_count * cw[1], s.height() - dy, 0, QString());


    dx = cw[0];

    for (const auto cond : { BrickLink::Condition::New, BrickLink::Condition::Used }) {
        dy = ch;

        for (const auto time : { BrickLink::Time::PastSix, BrickLink::Time::Current }) {
            dy += ch;

            cell cq(cell::Quantity, dx, dy, cw[1], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), false);
            cq.m_time = time;
            cq.m_condition = cond;
            cq.m_price = BrickLink::Price::Count;
            d->m_cells.push_back(cq);
            dy += ch;

            bool flip = true;

            for (const auto price : { BrickLink::Price::Lowest, BrickLink::Price::Average,
                 BrickLink::Price::WAverage, BrickLink::Price::Highest }) {
                cell cp(cell::Price, dx, dy, cw[1], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), flip);
                cp.m_time = time;
                cp.m_condition = cond;
                cp.m_price = price;
                d->m_cells.push_back(cp);
                dy += ch;
                flip = !flip;
            }
        }
        dx += cw[1];
    }
}



void PriceGuideWidget::paintHeader(QPainter *p, const QRect &r, Qt::Alignment align,
                                   const QString &str, bool bold, bool left, bool right)
{
    QStyleOptionHeader opt;
    opt.initFrom(this);
    opt.state           &= ~QStyle::State_MouseOver;
    opt.rect             = r;
    opt.orientation      = Qt::Horizontal;
    opt.position         = left && right ? QStyleOptionHeader::OnlyOneSection
                                         : left ? QStyleOptionHeader::Beginning
                                                : right ? QStyleOptionHeader::End
                                                        : QStyleOptionHeader::Middle;
    opt.section          = 1;
    opt.selectedPosition = QStyleOptionHeader::NotAdjacent;
    opt.sortIndicator    = QStyleOptionHeader::None;
    opt.text             = str;
    opt.textAlignment    = align;
    opt.palette          = QApplication::palette("QHeaderView");

    p->save();
    if (bold)
        opt.state |= QStyle::State_On;
    style()->drawControl(QStyle::CE_HeaderSection, &opt, p, this);
    opt.rect.adjust(hborder, 0, -hborder, 0);
    style()->drawControl(QStyle::CE_HeaderLabel, &opt, p, this);
    p->restore();
}

void PriceGuideWidget::paintCell(QPainter *p, const QRect &r, Qt::Alignment align,
                                 const QString &str, bool alternate, bool mouseOver)
{
    QStyleOptionViewItem opt;
    opt.initFrom(this);
    opt.text = str;
    opt.displayAlignment = align;
    opt.rect = r;
    opt.state = QStyle::State_Enabled | QStyle::State_Sibling;
    if (mouseOver) {
        opt.state |= QStyle::State_MouseOver;
    }
    opt.showDecorationSelected = true;
    opt.features = QStyleOptionViewItem::HasDisplay;
    if (alternate)
        opt.features |= QStyleOptionViewItem::Alternate;
    style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, p, this);
    if (!str.isEmpty())
        style()->drawControl(QStyle::CE_ItemViewItem, &opt, p, this);
}

void PriceGuideWidget::paintEvent(QPaintEvent *e)
{
    QPainter p(viewport());
    p.fillRect(e->rect(), palette().base());

    bool valid = d->m_pg && d->m_pg->isValid();
    bool is_updating = d->m_pg && (d->m_pg->updateStatus() == BrickLink::UpdateStatus::Updating);

    QString str = d->m_pg ? QStringLiteral("-") : QString();
    auto crate = Currency::inst()->rate(d->m_ccode);

    for (const cell &c : std::as_const(d->m_cells)) {
        if ((e->rect() & c).isEmpty())
            continue;

        switch (c.m_type) {
        case cell::Header: {
            bool left = (c.left() == 0);
            bool right = (c.right() == (viewport()->width() - 1));

            paintHeader(&p, c, c.m_text_flags, (c.m_text == u"$$$") ? d->m_ccode
                                                                    : c.m_text, c.m_flag, left, right);
            break;
        }
        case cell::VatHeader: {
            bool left = (c.left() == 0);
            bool right = (c.right() == (viewport()->width() - 1));

            paintHeader(&p, c, c.m_text_flags, { }, c.m_flag, left, right);
            d->m_vatIcon.paint(&p, c, Qt::AlignCenter, QIcon::Normal, QIcon::On);
            break;
        }
        case cell::Quantity:
            if (!is_updating) {
                if (valid) {
                    auto qty = d->m_pg->quantity(c.m_time, c.m_condition);
                    auto lots = d->m_pg->lots(c.m_time, c.m_condition);
                    if (!qty && !lots)
                        str = u"-"_qs;
                    else
                        str = u"%L1 (%L2)"_qs.arg(qty).arg(lots);
                }
                paintCell(&p, c, c.m_text_flags, str, c.m_flag);
            }
            break;

        case cell::Price:
            if (!is_updating) {
                if (valid) {
                    auto price = d->m_pg->price(c.m_time, c.m_condition, c.m_price) * crate;
                    if (qFuzzyIsNull(price))
                        str = u"-"_qs;
                    else
                        str = Currency::toDisplayString(price);
                }
                paintCell(&p, c, c.m_text_flags, str, c.m_flag,
                          (&c == d->m_cellUnderMouse) && d->m_pg && d->m_pg->isValid());
            }
            break;

        case cell::Update:
            if (is_updating)
                paintCell(&p, c, c.m_text_flags, d->m_str_wait, c.m_flag);
            break;

        case cell::Empty:
            paintCell(&p, c, Qt::AlignCenter, QString(), false);
            break;
        }
    }
}

void PriceGuideWidget::mouseDoubleClickEvent(QMouseEvent *me)
{
    if (!d->m_pg || !d->m_pg->isValid())
        return;

    if (auto c = d->cellAtPos(me->pos())) {
        if (c->m_type == cell::Price)
            emit priceDoubleClicked(d->m_pg->price(c->m_time, c->m_condition, c->m_price));
    }
}

void PriceGuideWidget::mouseMoveEvent(QMouseEvent *me)
{
    if (!d->m_pg)
        return;

    auto c = d->cellAtPos(me->pos());

    if (c && (c->m_type == cell::Price) && d->m_pg->isValid())
        setCursor(Qt::PointingHandCursor);
    else
        unsetCursor();
    if (c != d->m_cellUnderMouse) {
        QRect r;
        if (c)
            r |= *c;
        if (d->m_cellUnderMouse)
            r |= *d->m_cellUnderMouse;
        viewport()->update(r);
        d->m_cellUnderMouse = c;
    }
}

void PriceGuideWidget::leaveEvent(QEvent * /*e*/)
{
    if (d->m_cellUnderMouse) {
        viewport()->update(*d->m_cellUnderMouse);
        d->m_cellUnderMouse = nullptr;
    }
    unsetCursor();
}

bool PriceGuideWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        auto c = d->cellAtPos(he->pos());
        QString tt;

        if (c && (c->m_type == cell::Price) && d->m_pg && d->m_pg->isValid()) {
            QString pt;

            switch (c->m_price) {
            case BrickLink::Price::Lowest  : pt = tr("Lowest price"); break;
            case BrickLink::Price::Average : pt = tr("Average price (arithmetic mean)"); break;
            case BrickLink::Price::WAverage: pt = tr("Weighted average price (weighted arithmetic mean)"); break;
            case BrickLink::Price::Highest : pt = tr("Highest price"); break;
            default: break;
            }

            tt = tr("Double-click to set the price of the current item")
                    + u"<br><br><i>" + pt + u"</i>";

        } else if (c && (c->m_type == cell::Quantity) && d->m_pg && d->m_pg->isValid()) {
            int items = d->m_pg->quantity(c->m_time, c->m_condition);
            int lots = d->m_pg->lots(c->m_time, c->m_condition);

            if (items && lots) {
                if (c->m_time == BrickLink::Time::Current) {
                    tt = tr("A total quantity of %Ln item(s) is available for purchase", nullptr, items)
                            + u" (" + tr("in %Ln individual lot(s)", nullptr, lots) + u")";
                } else {
                    tt = tr("A total quantity of %Ln item(s) has been sold", nullptr, items)
                            + u" (" + tr("in %Ln individual lot(s)", nullptr, lots) + u")";
                }
            } else {
                if (c->m_time == BrickLink::Time::Current)
                    tt = tr("No items currently for sale");
                else
                    tt = tr("No items have been sold");
            }
        } else if (c && (c->m_type == cell::VatHeader)) {
            tt = d->m_vatDescription;
        }
        if (!tt.isEmpty())
            QToolTip::showText(he->globalPos(), tt, this);
        e->accept();
        return true;
    }
    else
        return QTreeView::event(e);
}

void PriceGuideWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    if (e->type() == QEvent::FontChange)
        recalcLayout();
    QTreeView::changeEvent(e);
}

void PriceGuideWidget::updateNonStaticCells() const
{
    QRegion r;
    for (const auto &c : std::as_const(d->m_cells)) {
        switch (c.m_type) {
        case cell::Quantity:
        case cell::Price   :
        case cell::Update  : r |= c; break;
        default            : break;
        }
    }
    viewport()->update(r);
}

void PriceGuideWidget::updateVatType(BrickLink::VatType vatType)
{
    if (d->m_pg) {
        setPriceGuide(BrickLink::core()->priceGuideCache()->priceGuide(
                          d->m_pg->item(), d->m_pg->color(), vatType, true));
    }
    d->m_vatIcon = BrickLink::PriceGuideCache::iconForVatType(vatType);
    d->m_vatDescription = BrickLink::PriceGuideCache::descriptionForVatType(vatType);
    recalcLayout();
}

QString PriceGuideWidget::currencyCode() const
{
    return d->m_ccode;
}

void PriceGuideWidget::setCurrencyCode(const QString &code)
{
    if (code != d->m_ccode) {
        d->m_ccode = code;
        recalcLayout();
    }
}


#include "moc_priceguidewidget.cpp"
