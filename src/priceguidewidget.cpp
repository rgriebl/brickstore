/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include "bricklink.h"
#include "config.h"
#include "currency.h"

#include "priceguidewidget.h"

namespace {

static const int hborder = 8;
static const int vborder = 3;

struct cell : public QRect {
    enum cell_type {
        Header,
        Quantity,
        Price,
        Update,
        Empty
    };

    cell(cell_type t, int x, int y, int w, int h, int tfl, const QString &str, bool flag = false)
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

    QList<cell>::const_iterator cellAtPos(const QPoint &fpos)
    {
        QPoint pos = fpos - QPoint(m_widget->frameWidth(), m_widget->frameWidth());

        return std::find_if(m_cells.constBegin(), m_cells.constEnd(), [pos](const cell &cell) {
            return cell.contains(pos) && (cell.m_type != cell::Update);
        });
    }

private:
    Q_DISABLE_COPY(PriceGuideWidgetPrivate)
    friend class PriceGuideWidget;

    PriceGuideWidget *        m_widget;
    BrickLink::PriceGuide *   m_pg = nullptr;
    QList<cell>               m_cells;
    QString                   m_ccode;
    qreal                     m_crate = 0;
    PriceGuideWidget::Layout  m_layout = PriceGuideWidget::Normal;
    bool                      m_connected = false;
    bool                      m_on_price = false;

    static constexpr int s_cond_count = int(BrickLink::Condition::Count);
    static constexpr int s_time_count = int(BrickLink::Time::Count);
    static constexpr int s_price_count = int(BrickLink::Price::Count);

    QString m_str_qty;
    QString m_str_cond[s_cond_count];
    QString m_str_price[s_price_count];
    QString m_str_vtime[s_time_count];
    QString m_str_htime[s_time_count][2];
    QString m_str_wait;
};

PriceGuideWidget::PriceGuideWidget(QWidget *parent)
    : QFrame(parent)
    , d(new PriceGuideWidgetPrivate(this))
{
    d->m_ccode = Config::inst()->defaultCurrencyCode();
    d->m_crate = Currency::inst()->rate(d->m_ccode);

    setBackgroundRole(QPalette::Base);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    setPalette(QApplication::palette("QAbstractItemView"));

    QAction *a;
    a = new QAction(this);
    a->setObjectName("priceguide_reload");
    a->setIcon(QIcon::fromTheme("view-refresh"));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::doUpdate);
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("priceguide_bl_catalog");
    a->setIcon(QIcon::fromTheme("bricklink-catalog"));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::showBLCatalogInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName("priceguide_bl_priceguide");
    a->setIcon(QIcon::fromTheme("bricklink-priceguide"));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::showBLPriceGuideInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName("priceguide_bl_lotsforsale");
    a->setIcon(QIcon::fromTheme("bricklink-lotsforsale"));
    connect(a, &QAction::triggered,
            this, &PriceGuideWidget::showBLLotsForSale);
    addAction(a);


    languageChange();
}

void PriceGuideWidget::languageChange()
{
    findChild<QAction *>("priceguide_reload")->setText(tr("Update"));
    findChild<QAction *>("priceguide_bl_catalog")->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *>("priceguide_bl_priceguide")->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *>("priceguide_bl_lotsforsale")->setText(tr("Show Lots for Sale on BrickLink..."));

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
    update();
}

PriceGuideWidget::~PriceGuideWidget()
{
    if (d->m_pg)
        d->m_pg->release();
}

void PriceGuideWidget::showBLCatalogInfo()
{
    if (d->m_pg && d->m_pg->item()) {
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_CatalogInfo,
                                                         d->m_pg->item(), d->m_pg->color()));
    }
}

void PriceGuideWidget::showBLPriceGuideInfo()
{
    if (d->m_pg && d->m_pg->item() && d->m_pg->color()) {
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo,
                                                         d->m_pg->item(), d->m_pg->color()));
    }
}

void PriceGuideWidget::showBLLotsForSale()
{
    if (d->m_pg && d->m_pg->item() && d->m_pg->color()) {
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_LotsForSale,
                                                         d->m_pg->item(), d->m_pg->color()));
    }
}

QSize PriceGuideWidget::sizeHint() const
{
    return minimumSize();
}

void PriceGuideWidget::doUpdate()
{
    if (d->m_pg)
        d->m_pg->update(true);
    update(nonStaticCells());
}


void PriceGuideWidget::gotUpdate(BrickLink::PriceGuide *pg)
{
    if (pg == d->m_pg)
        update(nonStaticCells());
}


void PriceGuideWidget::setPriceGuide(BrickLink::PriceGuide *pg)
{
    if (pg == d->m_pg)
        return;

    if (d->m_pg)
        d->m_pg->release();
    if (pg)
        pg->addRef();

    if (!d->m_connected && pg) {
        d->m_connected = connect(BrickLink::core(), &BrickLink::Core::priceGuideUpdated,
                                 this, &PriceGuideWidget::gotUpdate);
    }
    d->m_pg = pg;

    setContextMenuPolicy(pg ? Qt::ActionsContextMenu : Qt::NoContextMenu);

    update(nonStaticCells());
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
        update();
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
}

void PriceGuideWidget::recalcLayoutNormal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb)
{
    int cw[4];
    int ch = fm.height() + 2 * vborder;

    int dx, dy;

    cw[0] = 0; // not used
    cw[1] = 0;
    for (const auto &cond : d->m_str_cond)
        cw[1] = qMax(cw[1], fm.horizontalAdvance(cond));
    cw[2] = qMax(fm.horizontalAdvance(d->m_str_qty), fm.horizontalAdvance("0000000 (00000)"));
    cw[3] = fm.horizontalAdvance(Currency::toString(9000, d->m_ccode, Currency::LocalSymbol));
    for (const auto &price : d->m_str_price)
        cw[3] = qMax(cw[3], fm.horizontalAdvance(price));

    for (int i = 1; i < 4; i++)
        cw[i] += (2 * hborder);

    dx = 0;
    for (const auto &vtime : d->m_str_vtime)
        dx = qMax(dx, fmb.horizontalAdvance(vtime));

    if ((cw[1] + cw[2] + d->s_price_count * cw[3]) < dx)
        cw[1] = dx - (cw[2] + d->s_price_count * cw[3]);

    setMinimumSize(2 * frameWidth() + cw[1] + cw[2] + d->s_price_count * cw[3],
                   2 * frameWidth() + (1 + (1 + d->s_cond_count) * d->s_time_count) * ch);

    d->m_cells << cell(cell::Header, 0, 0, cw[1], ch, Qt::AlignCenter, "$$$", true);
    d->m_cells << cell(cell::Header, cw[1], 0, cw[2], ch, Qt::AlignCenter, d->m_str_qty);

    dx = cw[1] + cw[2];

    for (const auto &prive : d->m_str_price) {
        d->m_cells << cell(cell::Header, dx, 0, cw[3], ch, Qt::AlignCenter, prive);
        dx += cw[3];
    }

    d->m_cells << cell(cell::Header, dx, 0, s.width() - dx, ch, 0, QString());

    dx = cw[1] + cw[2] + cw[3] * d->s_price_count;
    dy = ch;

    for (const auto &vtime : d->m_str_vtime) {
        d->m_cells << cell(cell::Header, 0, dy, dx, ch, Qt::AlignCenter, vtime, true);
        dy += ch;

        for (int j = 0; j < d->s_cond_count; j++)
            d->m_cells << cell(cell::Header, 0, dy + j * ch, cw[1], ch, Qt::AlignCenter, d->m_str_cond [j]);

        d->m_cells << cell(cell::Update, cw[1], dy, dx - cw[1], ch * d->s_cond_count, Qt::AlignCenter | Qt::TextWordWrap, QString());
        dy += (d->s_cond_count * ch);
    }

    d->m_cells << cell(cell::Header, 0, dy, cw[1], s.height() - dy, 0, QString());

    dy = ch;
    bool flip = false;

    for (const auto time : { BrickLink::Time::PastSix, BrickLink::Time::Current }) {
        dy += ch;

        for (const auto cond : { BrickLink::Condition::New, BrickLink::Condition::Used }) {
            dx = cw[1];

            cell cq(cell::Quantity, dx, dy, cw[2], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), flip);
            cq.m_time      = time;
            cq.m_condition = cond;
            d->m_cells << cq;

            dx += cw[2];

            for (const auto price : { BrickLink::Price::Lowest, BrickLink::Price::Average,
                 BrickLink::Price::WAverage, BrickLink::Price::Highest }) {
                cell cp(cell::Price, dx, dy, cw[3], ch, Qt::AlignRight  | Qt::AlignVCenter, QString(), flip);
                cp.m_time      = time;
                cp.m_condition = cond;
                cp.m_price     = price;
                d->m_cells << cp;

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
        cw[0] = qMax(cw[0], qMax(fmb.horizontalAdvance(htime[0]),
                fmb.horizontalAdvance(htime[1])));
    cw[1] = 0;
    for (const auto &cond : d->m_str_cond)
        cw[1] = qMax(cw[1], fm.horizontalAdvance(cond));
    cw[2] = qMax(fm.horizontalAdvance(d->m_str_qty), fm.horizontalAdvance("0000000 (00000)"));
    cw[3] = fm.horizontalAdvance(Currency::toString(9000, d->m_ccode, Currency::NoSymbol));
    for (const auto &price : d->m_str_price)
        cw[3] = qMax(cw[3], fm.horizontalAdvance(price));

    for (int &i : cw)
        i += (2 * hborder);

    setMinimumSize(2 * frameWidth() + cw[0] + cw[1] + cw[2] + d->s_price_count * cw[3],
                   2 * frameWidth() + (1 + d->s_cond_count * d->s_time_count) * ch);
    setMaximumWidth(minimumWidth());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    dx = cw[0] + cw[1];
    dy = 0;

    d->m_cells << cell(cell::Header, 0,  dy, dx,    ch, Qt::AlignCenter, "$$$", true);
    d->m_cells << cell(cell::Header, dx, dy, cw[2], ch, Qt::AlignCenter, d->m_str_qty);

    dx += cw[2];

    for (const auto &price : d->m_str_price) {
        d->m_cells << cell(cell::Header, dx, dy, cw[3], ch, Qt::AlignCenter, price);
        dx += cw[3];
    }

    d->m_cells << cell(cell::Header, dx, dy, s.width() - dx, ch, 0, QString());

    dx = 0;
    dy = ch;

    for (const auto &htime : d->m_str_htime) {
        d->m_cells << cell(cell::Header, dx, dy, cw[0], d->s_cond_count * ch, Qt::AlignLeft | Qt::AlignVCenter, htime[0] + "\n" + htime[1], true);

        for (const auto &cond : d->m_str_cond) {
            d->m_cells << cell(cell::Header, dx + cw[0], dy, cw[1], ch, Qt::AlignCenter, cond);
            dy += ch;
        }
    }

    d->m_cells << cell(cell::Header, dx, dy, cw[0] + cw[1], s.height() - dy, 0, QString());

    d->m_cells << cell(cell::Update, cw[0] + cw[1], ch, cw[2] + d->s_price_count * cw[3], d->s_time_count * d->s_cond_count * ch, Qt::AlignCenter | Qt::TextWordWrap, QString());

    dy = ch;
    bool flip = false;

    for (const auto time : { BrickLink::Time::PastSix, BrickLink::Time::Current }) {
        for (const auto cond : { BrickLink::Condition::New, BrickLink::Condition::Used }) {
            dx = cw[0] + cw[1];

            cell cq(cell::Quantity, dx, dy, cw[2], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), flip);
            cq.m_time      = time;
            cq.m_condition = cond;
            d->m_cells << cq;
            dx += cw[2];

            for (const auto price : { BrickLink::Price::Lowest, BrickLink::Price::Average,
                 BrickLink::Price::WAverage, BrickLink::Price::Highest }) {
                cell cp(cell::Price, dx, dy, cw[3], ch, Qt::AlignRight  | Qt::AlignVCenter, QString(), flip);
                cp.m_time      = time;
                cp.m_condition = cond;
                cp.m_price     = price;
                d->m_cells << cp;
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
        cw[0] = qMax(cw[0], fm.horizontalAdvance(price));
    cw[0] += 2 * hborder;

    cw[1] = qMax(fm.horizontalAdvance(Currency::toString(9000, d->m_ccode, Currency::NoSymbol)),
                  fm.horizontalAdvance("0000000 (00000)"));
    for (const auto &cond : d->m_str_cond)
        cw[1] = qMax(cw[1], fm.horizontalAdvance(cond));
    cw[1] += 2 * hborder;

    dx = 0;
    for (const auto &vtime : d->m_str_vtime)
        dx = qMax(dx, fmb.horizontalAdvance(vtime));

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

    d->m_cells << cell(cell::Header, dx, dy, cw[0], ch, Qt::AlignCenter, "$$$", true);
    dx += cw[0];

    for (const auto &cond : d->m_str_cond) {
        d->m_cells << cell(cell::Header, dx, dy, cw[1], ch, Qt::AlignCenter, cond);
        dx += cw[1];
    }

    d->m_cells << cell(cell::Header, dx, dy, s.width() - dx, ch, 0, QString());
    dy += ch;

    d->m_cells << cell(cell::Empty, dx, dy, s.width() - dx, s.height() - dy, 0, QString());

    dx = 0;
    dy = ch;

    for (const auto &vtime : d->m_str_vtime) {
        d->m_cells << cell(cell::Header, dx, dy, cw[0] + d->s_cond_count * cw[1], ch, Qt::AlignCenter, vtime, true);
        dy += ch;

        d->m_cells << cell(cell::Header, dx, dy, cw[0], ch, Qt::AlignLeft | Qt::AlignVCenter, d->m_str_qty, false);

        d->m_cells << cell(cell::Update, dx + cw[0], dy, d->s_cond_count * cw[1], (1 + d->s_price_count) * ch, Qt::AlignCenter | Qt::TextWordWrap, QString());
        dy += ch;

        for (const auto &price : d->m_str_price) {
            d->m_cells << cell(cell::Header, dx, dy, cw[0], ch, Qt::AlignLeft | Qt::AlignVCenter, price, false);
            dy += ch;
        }
    }
    d->m_cells << cell(cell::Header, dx, dy, cw[0], s.height() - dy, 0, QString());

    d->m_cells << cell(cell::Empty, dx + cw[0], dy, d->s_cond_count * cw[1], s.height() - dy, 0, QString());


    dx = cw[0];

    for (const auto cond : { BrickLink::Condition::New, BrickLink::Condition::Used }) {
        dy = ch;

        for (const auto time : { BrickLink::Time::PastSix, BrickLink::Time::Current }) {
            dy += ch;

            cell cq(cell::Quantity, dx, dy, cw[1], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), false);
            cq.m_time = time;
            cq.m_condition = cond;
            d->m_cells << cq;
            dy += ch;

            bool flip = true;

            for (const auto price : { BrickLink::Price::Lowest, BrickLink::Price::Average,
                 BrickLink::Price::WAverage, BrickLink::Price::Highest }) {
                cell cp(cell::Price, dx, dy, cw[1], ch, Qt::AlignRight | Qt::AlignVCenter, QString(), flip);
                cp.m_time = time;
                cp.m_condition = cond;
                cp.m_price = price;
                d->m_cells << cp;
                dy += ch;
                flip = !flip;
            }
        }
        dx += cw[1];
    }
}



void PriceGuideWidget::paintHeader(QPainter *p, const QRect &r, Qt::Alignment align, const QString &str, bool bold)
{
    QStyleOptionHeader opt;
    opt.initFrom(this);
    opt.state           &= ~QStyle::State_MouseOver;
    opt.rect             = r;
    opt.orientation      = Qt::Horizontal;
    opt.position         = QStyleOptionHeader::Middle;
    opt.section          = 1;
    opt.selectedPosition = QStyleOptionHeader::NotAdjacent;
    opt.sortIndicator    = QStyleOptionHeader::None;
    opt.text             = str;
    opt.textAlignment    = align;
    opt.palette          = QApplication::palette("QHeaderView");

    p->save();
    if (bold)
        opt.state |= QStyle::State_On;
    style()->drawControl(QStyle::CE_Header, &opt, p, this);
    p->restore();
}

void PriceGuideWidget::paintCell(QPainter *p, const QRect &r, Qt::Alignment align,
                                 const QString &str, bool alternate)
{
    QStyleOptionViewItem opt;
    opt.initFrom(this);
    opt.palette = QApplication::palette("QAbstractItemView");
    opt.text = str;
    opt.displayAlignment = align;
    opt.rect = r;
    opt.features = QStyleOptionViewItem::HasDisplay;
    if (alternate)
        opt.features |= QStyleOptionViewItem::Alternate;
    style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, p, this);
    if (!str.isEmpty())
        style()->drawControl(QStyle::CE_ItemViewItem, &opt, p, this);
}

void PriceGuideWidget::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);

    const QPoint offset = contentsRect().topLeft();
    QPainter p(this);
    p.fillRect(contentsRect() & e->rect(), QApplication::palette("QAbstractItemView").base());
    p.translate(offset.x(), offset.y());

    bool valid = d->m_pg && d->m_pg->valid();
    bool is_updating = d->m_pg && (d->m_pg->updateStatus() == BrickLink::UpdateStatus::Updating);

    QString str = d->m_pg ? "-" : "";

    for (const cell &c : qAsConst(d->m_cells)) {
        if ((e->rect() & c).isEmpty())
            continue;

        switch (c.m_type) {
        case cell::Header:
            paintHeader(&p, c, c.m_text_flags, c.m_text == "$$$" ? Currency::localSymbol(d->m_ccode) : c.m_text, c.m_flag);
            break;

        case cell::Quantity:
            if (!is_updating) {
                if (valid)
                    str = QString("%1 (%2)").arg(d->m_pg->quantity(c.m_time, c.m_condition)).arg(d->m_pg->lots(c.m_time, c.m_condition));

                paintCell(&p, c, c.m_text_flags, str, c.m_flag);
            }
            break;

        case cell::Price:
            if (!is_updating) {
                if (valid)
                    str = Currency::toString(d->m_pg->price(c.m_time, c.m_condition, c.m_price) * d->m_crate, d->m_ccode);

                paintCell(&p, c, c.m_text_flags, str, c.m_flag);
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
    if (!d->m_pg)
        return;

    auto it = d->cellAtPos(me->pos());

    if ((it != d->m_cells.constEnd()) && ((*it).m_type == cell::Price))
        emit priceDoubleClicked(d->m_pg->price((*it).m_time, (*it).m_condition, (*it).m_price));
}

void PriceGuideWidget::mouseMoveEvent(QMouseEvent *me)
{
    if (!d->m_pg)
        return;

    auto it = d->cellAtPos(me->pos());

    d->m_on_price = ((it != d->m_cells.constEnd()) && ((*it).m_type == cell::Price));

    if (d->m_on_price)
        setCursor(Qt::PointingHandCursor);
    else
        unsetCursor();
}

void PriceGuideWidget::leaveEvent(QEvent * /*e*/)
{
    if (d->m_on_price) {
        d->m_on_price = false;
        unsetCursor();
    }
}

bool PriceGuideWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        auto it = d->cellAtPos(he->pos());

        if ((it != d->m_cells.constEnd()) && ((*it).m_type == cell::Price))
            QToolTip::showText(he->globalPos(), PriceGuideWidget::tr("Double-click to set the price of the current item."), this);
        e->accept();
        return true;
    }
    else
        return QWidget::event(e);
}

void PriceGuideWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QFrame::changeEvent(e);
}

QRegion PriceGuideWidget::nonStaticCells() const
{
    QRegion r;
    QPoint offset = contentsRect().topLeft();

    for (const auto &c : qAsConst(d->m_cells)) {
        switch (c.m_type) {
        case cell::Quantity:
        case cell::Price   :
        case cell::Update  : r |= c.translated(offset); break;
        default            : break;
        }
    }
    return r;
}

QString PriceGuideWidget::currencyCode() const
{
    return d->m_ccode;
}

void PriceGuideWidget::setCurrencyCode(const QString &code)
{
    if (code != d->m_ccode) {
        d->m_ccode = code;
        d->m_crate = Currency::inst()->rate(code);
        recalcLayout();
        update();
    }
}


#include "moc_priceguidewidget.cpp"
