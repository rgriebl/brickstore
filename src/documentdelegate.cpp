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
#include <QToolButton>
#include <QMenu>
#include <QPainter>
#include <QLineEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolTip>
#include <QIcon>
#include <QTableView>
#include <QHeaderView>
#include <QItemDelegate>
#include <QStyleOptionFrameV2>
#include <QStyle>
#include <QApplication>
#include <QScrollBar>
#include <QtMath>
#include <QScopeGuard>
#include <QStringBuilder>

#include "documentdelegate.h"
#include "selectitemdialog.h"
#include "selectcolordialog.h"
#include "utility.h"
#include "framework.h"
#include "smartvalidator.h"
#include "bricklink_model.h"


QVector<QColor>                 DocumentDelegate::s_shades;
QHash<int, QIcon>               DocumentDelegate::s_status_icons;
QCache<quint64, QPixmap>        DocumentDelegate::s_tag_cache;
QCache<int, QPixmap>            DocumentDelegate::s_stripe_cache;
QCache<DocumentDelegate::TextLayoutCacheKey, QTextLayout> DocumentDelegate::s_textLayoutCache(5000);


// we can't use eventFilter() from anything derived from QAbstractItemDelegate:
// the eventFilter() function there will filter ANY events, because it thinks
// everything is an editor widget.
class LanguageChangeHelper : public QObject
{
public:
    LanguageChangeHelper(DocumentDelegate *dd, QWidget *watch)
        : QObject(dd)
        , m_dd(dd)
    {
        watch->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() == QEvent::LanguageChange)
            m_dd->languageChange();
        return QObject::eventFilter(o, e);
    }
private:
    DocumentDelegate *m_dd;
};

DocumentDelegate::DocumentDelegate(Document *doc, DocumentProxyModel *view, QTableView *table)
    : QItemDelegate(view)
    , m_doc(doc)
    , m_view(view)
    , m_table(table)
{
    m_table->viewport()->setAttribute(Qt::WA_Hover);

    static bool once = false;
    if (!once) {
        qAddPostRoutine(clearCaches);
        once = false;
    }

    new LanguageChangeHelper(this, table);

    connect(BrickLink::core(), &BrickLink::Core::itemImageScaleFactorChanged,
            this, [this]() {
        m_table->resizeRowsToContents();
        m_table->resizeColumnToContents(Document::Picture);
    });
}

void DocumentDelegate::clearCaches()
{
    s_stripe_cache.clear();
    s_tag_cache.clear();
    s_status_icons.clear();
}

QColor DocumentDelegate::shadeColor(int idx, qreal alpha)
{
    if (s_shades.isEmpty()) {
        s_shades.resize(13);
        for (int i = 0; i < 13; i++)
            s_shades[i] = QColor::fromHsv(i == 0 ? -1 : (i - 1) * 30, 255, 255);
    }
    QColor c = s_shades.at(idx % s_shades.size());
    if (!qFuzzyIsNull(alpha))
        c.setAlphaF(alpha);
    return c;
}

QIcon::Mode DocumentDelegate::iconMode(QStyle::State state) const
{
    if (!(state & QStyle::State_Enabled)) return QIcon::Disabled;
    if (state & QStyle::State_Selected) return QIcon::Selected;
    return QIcon::Normal;
}

QIcon::State DocumentDelegate::iconState(QStyle::State state) const
{
    return (state & QStyle::State_Open) ? QIcon::On : QIcon::Off;
}

int DocumentDelegate::defaultItemHeight(const QWidget *w) const
{
    QSize picsize = BrickLink::core()->standardPictureSize();
    QFontMetrics fm(w ? w->font() : QApplication::font("QTableView"));

    return qMax(2 + fm.height(), picsize.height() + 1 /* the grid lines */);
}

QSize DocumentDelegate::sizeHint(const QStyleOptionViewItem &option1, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return {};

    int w = -1;

    if (idx.column() == Document::Picture)
        w = 4 + BrickLink::core()->standardPictureSize().width();
    else
        w = QItemDelegate::sizeHint(option1, idx).width();

    static const QVector<int> twoLiners = {
        Document::Description,
        Document::Remarks,
        Document::Comments
    };

    if (twoLiners.contains(idx.column())
            && (w > (m_doc->headerDataForDefaultWidthRole(static_cast<Document::Field>(idx.column()))
                     * option1.fontMetrics.averageCharWidth()))) {
        w = int(w / 1.9);  // we can wrap to two lines (plus 10% security margin)
    }

    if (idx.column() == Document::Color)
        w += (option1.decorationSize.width() * 2 + 4);

    QStyleOptionViewItem option(option1);
    return { w + 1 /* the grid lines*/, defaultItemHeight(option.widget) };
}


inline uint qHash(const DocumentDelegate::TextLayoutCacheKey &key, uint seed)
{
    auto sizeHash = qHash((key.size.width() << 16) ^ key.size.height(), seed);
    return qHash(key.text) ^ sizeHash ^ key.fontSize ^ seed;
}

void DocumentDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return;

    Document::Item *it = m_view->item(idx);
    if (!it)
        return;

    p->save();
    auto restorePainter = qScopeGuard([p] { p->restore(); });

    Qt::Alignment align = (Qt::Alignment(idx.data(Qt::TextAlignmentRole).toInt()) & ~Qt::AlignVertical_Mask) | Qt::AlignVCenter;

    if (idx.column() == Document::Index) {
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        QStyleOptionHeader headerOption;
        headerOption.init(option.widget);
        headerOption.text = idx.data().toString();
        headerOption.state = option.state;
        headerOption.textAlignment = align;
        headerOption.orientation = Qt::Vertical;
        headerOption.section = idx.row();
        headerOption.rect = option.rect;
        switch (option.viewItemPosition) {
        case QStyleOptionViewItem::Beginning:
            headerOption.position = QStyleOptionHeader::Beginning; break;
        case QStyleOptionViewItem::Middle:
            headerOption.position = QStyleOptionHeader::Middle; break;
        case QStyleOptionViewItem::End:
            headerOption.position = QStyleOptionHeader::End; break;
        default:
        case QStyleOptionViewItem::OnlyOne:
            headerOption.position = QStyleOptionHeader::OnlyOneSection; break;
        }
        style->drawControl(QStyle::CE_Header, &headerOption, p, option.widget);
        return;
    }

    bool selected = (option.state & QStyle::State_Selected);
    bool nocolor = !it->color();
    bool noitem = !it->item();

    QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    QColor normalbg = option.palette.color(cg, (option.features & QStyleOptionViewItem::Alternate) ? QPalette::AlternateBase : QPalette::Base);
    QColor bg = normalbg;
    QColor fg = option.palette.color(cg, QPalette::Text);

    if (selected) {
        if (m_read_only) {
            bg = Utility::gradientColor(bg, option.palette.color(cg, QPalette::Highlight));
        } else {
            bg = option.palette.color(cg, QPalette::Highlight);
            if (!(option.state & QStyle::State_HasFocus))
                bg.setAlphaF(0.7);
            fg = option.palette.color(cg, QPalette::HighlightedText);
        }
    }

    int x = option.rect.x(), y = option.rect.y();
    int w = option.rect.width();
    int h = option.rect.height();
    int margin = 2;

    struct Tag {
        QColor foreground;
        QColor background;
        QString text;
        bool bold;
    } tag = { QColor(), QColor(), QString(), false };

    QImage image;
    QIcon ico;
    QString str = idx.model()->data(idx, Qt::DisplayRole).toString();
    int checkmark = 0;
    bool selectionFrame = false;

    if (!selected) {
        switch (idx.column()) {
        case Document::ItemType:
            if (it->itemType())
                bg = shadeColor(it->itemType()->id(), 0.1);
            break;

        case Document::Category:
            if (it->category())
                bg = shadeColor(int(it->category()->id()), 0.2);
            break;

        case Document::Quantity:
            if (it->quantity() <= 0)
                bg = (it->quantity() == 0) ? QColor::fromRgbF(1, 1, 0, 0.4)
                                           : QColor::fromRgbF(1, 0, 0, 0.4);
            break;

        case Document::QuantityDiff:
            if (it->origQuantity() < it->quantity())
                bg = QColor::fromRgbF(0, 1, 0, 0.3);
            else if (it->origQuantity() > it->quantity())
                bg = QColor::fromRgbF(1, 0, 0, 0.3);
            break;

        case Document::PriceOrig:
        case Document::QuantityOrig:
            fg.setAlphaF(0.5);
            break;

        case Document::PriceDiff:
            if (it->origPrice() < it->price())
                bg = QColor::fromRgbF(0, 1, 0, 0.3);
            else if (it->origPrice() > it->price())
                bg = QColor::fromRgbF(1, 0, 0, 0.3);
            break;

        case Document::Total:
            bg = QColor::fromRgbF(1, 1, 0, 0.1);
            break;

        case Document::Condition:
            if (it->condition() != BrickLink::Condition::New) {
                bg = fg;
                bg.setAlphaF(0.3);
            }
            break;

        case Document::TierP1:
        case Document::TierQ1:
            bg = fg;
            bg.setAlphaF(0.06);
            break;

        case Document::TierP2:
        case Document::TierQ2:
            bg = fg;
            bg.setAlphaF(0.12);
            break;

        case Document::TierP3:
        case Document::TierQ3:
            bg = fg;
            bg.setAlphaF(0.18);
            break;

        }
    }

    switch (idx.column()) {
    case Document::Status: {
        ico = s_status_icons[int(it->status())];
        if (ico.isNull()) {
            switch (it->status()) {
            case BrickLink::Status::Exclude: ico = QIcon(":/images/edit_status_exclude.png"); break;
            case BrickLink::Status::Extra  : ico = QIcon(":/images/edit_status_extra.png"); break;
            default                        :
            case BrickLink::Status::Include: ico = QIcon(":/images/edit_status_include.png"); break;
            }
            s_status_icons.insert(int(it->status()), ico);
        }

        uint altid = it->alternateId();
        bool cp = it->counterPart();
        if (altid || cp) {
            tag.text = cp ? QLatin1String("CP") : QString::number(altid);
            tag.bold = (cp || !it->alternate());
            tag.foreground = fg;
            if (cp || selected) {
                tag.background = fg;
                tag.background.setAlphaF(0.3);
            } else {
                tag.background = shadeColor(int(1 + altid), qreal(0.5));
            }
        }
        break;
    }
    case Document::Description:
        if (it->item() && it->item()->hasInventory()) {
            tag.text = tr("Inv");
            tag.foreground = bg;
            tag.background = fg;
            tag.background.setAlphaF(0.3);

            if (option.state & QStyle::State_MouseOver) {
                tag.foreground = option.palette.color(QPalette::HighlightedText);
                tag.background = option.palette.color(QPalette::Highlight);
            }
        }
        break;

    case Document::Picture: {
        if (!it->image().isNull()) {
            image = it->image().scaled(option.rect.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
            selectionFrame = true;
        }
        break;
    }
    case Document::Color: {
        image = BrickLink::core()->colorImage(it->color(), option.decorationSize.width() * 1.5, option.rect.height());
        break;
    }
    case Document::Retain:
        checkmark = it->retain() ? 1 : -1;
        break;

    case Document::Stockroom:
        if (it->stockroom() == BrickLink::Stockroom::None)
            checkmark = -1;
        else
            str = QLatin1Char('A' + char(it->stockroom()) - char(BrickLink::Stockroom::A));
        break;
    }

    // on macOS, the dark mode alternate base color is white with alpha, making it a medium gray
    // since we need an opaque color for filling, we need to premultiply it:
    normalbg = Utility::premultiplyAlpha(normalbg);

    // we only want to do a single, opaque color fill, so we calculate the
    // final fill color using the normal bg color and our special bg color
    // (which most likely has an alpha component)
    bg = Utility::gradientColor(normalbg, bg, bg.alphaF());
    bg.setAlpha(255);
    p->fillRect(option.rect, selectionFrame ? Qt::white : bg);

    if (nocolor || noitem) {
        int d = option.rect.height();
        QPixmap *pix = s_stripe_cache[d];
        if (!pix) {
            pix = new QPixmap(2 * d, d);
            pix->fill(Qt::transparent);
            QPainter pixp(pix);
            pixp.setPen(Qt::transparent);
            QLinearGradient grad(pix->rect().bottomLeft(), pix->rect().topRight());
            grad.setColorAt(0, QColor(255, 0, 0, 16));
            grad.setColorAt(0.2, QColor(255, 0, 0, 64));
            grad.setColorAt(0.6, QColor(255, 0, 0, 32));
            grad.setColorAt(0.8, QColor(255, 0, 0, 64));
            grad.setColorAt(1, QColor(255, 0, 0, 16));
            pixp.setBrush(grad);
            pixp.drawPolygon(QPolygon() << QPoint(d, 0) << QPoint(2 * d, 0) << QPoint(d, d) << QPoint(0, d));
            pixp.end();
            s_stripe_cache.insert(d, pix);
        }
        int offset = (option.features & QStyleOptionViewItem::Alternate) ? d : 0;
        offset -= m_table->horizontalScrollBar()->value();
        p->drawTiledPixmap(option.rect, *pix, QPoint(option.rect.left() - offset, 0));
    }

    if (!tag.text.isEmpty()) {
        QFont font = option.font;
        font.setBold(tag.bold);
        QFontMetrics fontmetrics(font);
        int itw = qMax(int(1.5 * fontmetrics.height()),
                       2 * fontmetrics.horizontalAdvance(tag.text));

        union { struct { qint32 i1; quint32 i2; } s; quint64 q; } key;
        key.s.i1 = itw;
        key.s.i2 = tag.background.rgba();

        QPixmap noCacheFallbackPix;
        QPixmap *pix = s_tag_cache[key.q];
        if (!pix) {
            pix = new QPixmap(itw, itw);
            pix->fill(Qt::transparent);
            QPainter pixp(pix);

            QRadialGradient grad(pix->rect().bottomRight(), pix->width());
            grad.setColorAt(0, tag.background);
            grad.setColorAt(0.6, tag.background);
            grad.setColorAt(1, Qt::transparent);

            pixp.fillRect(pix->rect(), grad);
            pixp.end();
            noCacheFallbackPix = *pix;
            if (!s_tag_cache.insert(key.q, pix))
                pix = &noCacheFallbackPix;
        }

        int pw = qMin(pix->width(), option.rect.width());
        int ph = qMin(pix->height(), option.rect.height());

        p->drawPixmap(option.rect.right() - pw + 1, option.rect.bottom() - ph + 1, *pix,
                      pix->width() - pw, pix->height() - ph, pw, ph);
        p->setPen(tag.foreground);
        QFont oldfont = p->font();
        p->setFont(font);
        p->drawText(option.rect.adjusted(0, 0, -1, -1), Qt::AlignRight | Qt::AlignBottom, tag.text);
        p->setFont(oldfont);
    }


    if (m_doc->itemErrors(it) & (1ULL << idx.column())) {
        p->setPen(QColor::fromRgbF(1, 0, 0, 0.75));
        p->drawRect(QRectF(x+.5, y+.5, w-1, h-1));
        p->setPen(QColor::fromRgbF(1, 0, 0, 0.50));
        p->drawRect(QRectF(x+1.5, y+1.5, w-3, h-3));
    }

    p->setPen(fg);

    x++; // extra spacing
    w -=2;

    if (checkmark != 0) {
        QStyleOptionViewItem opt(option);
        opt.state &= ~QStyle::State_HasFocus;
        opt.state |= ((checkmark > 0) ? QStyle::State_On : QStyle::State_Off);
        opt.features |= QStyleOptionViewItem::HasCheckIndicator;

        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        QRect r = style->subElementRect(QStyle::SE_ViewItemCheckIndicator, &opt, option.widget);
        int dx = margin;
        if (align & Qt::AlignHCenter)
            dx = (opt.rect.width() - r.width()) / 2;
        else if (align & Qt::AlignRight)
            dx = (opt.rect.width() - r.width() - margin);
        opt.rect = r.translated(dx, 0);
        style->drawPrimitive(QStyle::PE_IndicatorViewItemCheck, &opt, p, option.widget);
    }
    else if (!image.isNull()) {
        int px = x;
        int py = y;

        if (align & Qt::AlignHCenter)
            px += (option.rect.width() - image.width()) / 2;
        if (align & Qt::AlignVCenter)
            py += (option.rect.height() - image.height()) / 2;

        p->drawImage(px, py, image);

        int delta = px + image.width() + margin;
        w -= (delta - x);
        x = delta;
    }
    else if (!ico.isNull()) {
        ico.paint(p, x + margin, y, w - 2 * margin, h, align, iconMode(option.state), iconState(option.state));
    }

    static const QVector<int> richTextColumns = {
        Document::Description,
        Document::Color,
        Document::Condition,
        Document::Comments,
        Document::Remarks,
        Document::Category,
        Document::ItemType,
        Document::Reserved
    };

    if (!str.isEmpty()) {
        int rw = w - 2 * margin;
        const QFontMetrics &fm = p->fontMetrics();

        if (!richTextColumns.contains(idx.column())) {
            p->drawText(x + margin, y, rw, h, align, str);
            return;
        }

        static const QString elide = QLatin1String("...");
        auto key = TextLayoutCacheKey { str, QSize(rw, h), option.font.pixelSize() };
        QTextLayout *tlp = s_textLayoutCache.object(key);

        // an unlikely hash collision
        if (tlp && !(tlp->text() == str && tlp->font().pixelSize() == option.font.pixelSize())) {
            qDebug() << "TextLayoutCache: hash collision on:" << str;
            tlp = nullptr;
        }

        if (!tlp) {
            tlp = new QTextLayout(str, option.font, nullptr);
            tlp->setCacheEnabled(true);
            tlp->setTextOption(QTextOption(align));

            int lineCount = (h + fm.leading()) / fm.lineSpacing();
            int height = 0;

            tlp->beginLayout();
            for (int i = 0; i < lineCount; i++) {
                QTextLine line = tlp->createLine();
                if (!line.isValid())
                    break;

                line.setLineWidth(rw);
                height += fm.leading();
                line.setPosition(QPoint(0, height));
                height += int(line.height());

                if ((i == (lineCount - 1)) && ((line.textStart() + line.textLength()) < str.length())) {
                    int elide_width = fm.horizontalAdvance(elide) + margin;
                    line.setLineWidth(rw - elide_width);
                }
            }
            tlp->endLayout();

            if (!s_textLayoutCache.insert(key, tlp))
                tlp = nullptr;
        }

        if (tlp && tlp->lineCount()) {
            const auto lastLine = tlp->lineAt(tlp->lineCount() - 1);
            int height = tlp->lineCount() * (fm.leading() + lastLine.height());

            quint64 elideHash = quint64(idx.row()) << 32 | quint64(idx.column());

            if (align & Qt::AlignHCenter) // QTextLayout doesn't clip in this case
                p->setClipRect(option.rect);
            tlp->draw(p, QPoint(x + margin, y + (h - height)/2));
            if (lastLine.textStart() + lastLine.textLength() < str.length()) {
                int elidePos = int(lastLine.naturalTextWidth());
                p->drawText(QPoint(x + margin + int(elidePos),
                                   y + (h - height)/2 + (tlp->lineCount() - 1) * fm.lineSpacing() + fm.ascent()),
                            elide);
                m_elided.insert(elideHash);
            } else {
                m_elided.remove(elideHash);
            }
        }
    }
    if (selectionFrame && selected) {
        int lines = qMax(4, option.rect.height() / 20);
        QColor c = bg;
        for (int i = 0; i < lines; ++i) {
            c.setAlphaF(1. - i / double(lines + lines - 2));
            p->setPen(c);
            p->drawRect(option.rect.adjusted(i, i, -i - 1, -i -1));
        }
    }
}

bool DocumentDelegate::editorEvent(QEvent *e, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    if (m_read_only)
        return false;

    if (!e || !model || !idx.isValid())
        return false;

    Document::Item *it = m_view->item(idx);
    if (!it)
        return false;

    switch (e->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonDblClick: {
        if (nonInlineEdit(e, it, option, idx))
            return true;
        break;
    }
    default: break;
    }

    return QItemDelegate::editorEvent(e, model, option, idx);
}

bool DocumentDelegate::nonInlineEdit(QEvent *e, Document::Item *it, const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    bool accept = true;

    bool dblclick = (e->type() == QEvent::MouseButtonDblClick);
    bool keypress = (e->type() == QEvent::KeyPress);
    bool editkey = false;
    int key = -1;

    if (keypress) {
        key = static_cast<QKeyEvent *>(e)->key();

        if (key == Qt::Key_Space ||
            key == Qt::Key_Return ||
            key == Qt::Key_Enter ||
#if defined(Q_OS_MACOS)
            (key == Qt::Key_O && static_cast<QKeyEvent *>(e)->modifiers() & Qt::ControlModifier)
#else
            key == Qt::Key_F2
#endif
           ) {
            editkey = true;
        }
    }


    switch (idx.column()) {
    case Document::Retain:
        if (dblclick || (keypress && editkey)) {
            Document::Item item = *it;
            item.setRetain(!it->retain());
            m_doc->changeItem(it, item);
        }
        break;

    case Document::Stockroom: {
        bool noneKey = (QKeySequence(key) == QKeySequence(tr("-", "set stockroom to none")));

        if (dblclick || (keypress && (editkey || key == Qt::Key_A || key == Qt::Key_B
                                      || key == Qt::Key_C  || noneKey))) {
            BrickLink::Stockroom st = it->stockroom();
            if (key == Qt::Key_A)
                st = BrickLink::Stockroom::A;
            else if (key == Qt::Key_B)
                st = BrickLink::Stockroom::B;
            else if (key == Qt::Key_C)
                st = BrickLink::Stockroom::C;
            else if (noneKey)
                st = BrickLink::Stockroom::None;
            else
                switch (st) {
                case BrickLink::Stockroom::None: st = BrickLink::Stockroom::A; break;
                case BrickLink::Stockroom::A   : st = BrickLink::Stockroom::B; break;
                case BrickLink::Stockroom::B   : st = BrickLink::Stockroom::C; break;
                default                        :
                case BrickLink::Stockroom::C   : st = BrickLink::Stockroom::None; break;
                }

            Document::Item item = *it;
            item.setStockroom(st);
            m_doc->changeItem(it, item);
        }
        break;
    }
    case Document::Condition: {
        bool newKey = (QKeySequence(key) == QKeySequence(tr("N", "set condition to new")));
        bool usedKey = (QKeySequence(key) == QKeySequence(tr("U", "set condition to used")));

        if (dblclick || (keypress && (editkey || newKey || usedKey))) {
            BrickLink::Condition cond;
            if (newKey)
                cond = BrickLink::Condition::New;
            else if (usedKey)
                cond = BrickLink::Condition::Used;
            else
                cond = (it->condition() == BrickLink::Condition::New) ? BrickLink::Condition::Used : BrickLink::Condition::New;

            Document::Item item = *it;
            item.setCondition(cond);
            m_doc->changeItem(it, item);
        }
        break;
    }
    case Document::Status: {
        bool includeKey = (QKeySequence(key) == QKeySequence(tr("I", "set status to include")));
        bool excludeKey = (QKeySequence(key) == QKeySequence(tr("E", "set status to exclude")));
        bool extraKey = (QKeySequence(key) == QKeySequence(tr("X", "set status to extra")));

        if (dblclick || (keypress && (editkey || includeKey || excludeKey || extraKey))) {
            BrickLink::Status st = it->status();
            if (includeKey)
                st = BrickLink::Status::Include;
            else if (excludeKey)
                st = BrickLink::Status::Exclude;
            else if (extraKey)
                st = BrickLink::Status::Extra;
            else
                switch (st) {
                        case BrickLink::Status::Include: st = BrickLink::Status::Exclude; break;
                        case BrickLink::Status::Exclude:
                        case BrickLink::Status::Extra  :
                        default                        : st = BrickLink::Status::Include; break;
                }

            Document::Item item = *it;
            item.setStatus(st);
            m_doc->changeItem(it, item);
        }
        break;
    }
    case Document::Description:
        if (dblclick && it->item() && it->item()->hasInventory()) {
            auto me = static_cast<QMouseEvent *>(e);
            int d = option.rect.height() / 2;

            if ((me->x() > (option.rect.right() - d)) && (me->y() > (option.rect.bottom() - d))) {
                if (auto a = FrameWork::inst()->findAction("edit_partoutitems")) {
                    a->trigger();
                    break;
                }
            }
        }
        Q_FALLTHROUGH();
    case Document::Picture:
        if (dblclick || (keypress && editkey)) {
            if (!m_select_item) {
                m_select_item = new SelectItemDialog(false, m_table);
                m_select_item->setWindowFlag(Qt::Tool);
                m_select_item->setWindowTitle(tr("Modify Item"));
            }
            m_select_item->setItem(it->item());

            if (m_select_item->execAtPosition(
                        QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()),
                              option.rect.size())) == QDialog::Accepted) {
                Document::Item item = *it;
                item.setItem(m_select_item->item());
                m_doc->changeItem(it, item);
            }
        }
        break;

    case Document::Color:
        if (dblclick || (keypress && editkey)) {
            if (!m_select_color) {
                m_select_color = new SelectColorDialog(m_table);
                m_select_color->setWindowFlag(Qt::Tool);
                m_select_color->setWindowTitle(tr("Modify Color"));
            }
            m_select_color->setColorAndItem(it->color(), it->item());

            if (m_select_color->execAtPosition(QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()), option.rect.size())) == QDialog::Accepted) {
                Document::Item item = *it;
                item.setColor(m_select_color->color());
                m_doc->changeItem(it, item);
            }
        }
        break;

    default:
        accept = false;
        break;
    }
    return accept;
}

QWidget *DocumentDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &idx) const
{
    if (m_read_only)
        return nullptr;

    Document::Item *it = m_view->item(idx);
    if (!it)
        return nullptr;

    QValidator *valid = nullptr;
    switch (idx.column()) {
    case Document::Sale        : valid = new SmartIntValidator(-1000, 99, 0, nullptr); break;
    case Document::Quantity    :
    case Document::QuantityDiff: valid = new SmartIntValidator(-FrameWork::maxQuantity,
                                                               FrameWork::maxQuantity, 0, nullptr); break;
    case Document::Bulk        : valid = new SmartIntValidator(1, FrameWork::maxQuantity, 1, nullptr); break;
    case Document::TierQ1      :
    case Document::TierQ2      :
    case Document::TierQ3      : valid = new SmartIntValidator(0, FrameWork::maxQuantity, 0, nullptr); break;
    case Document::Price       :
    case Document::TierP1      :
    case Document::TierP2      :
    case Document::TierP3      : valid = new SmartDoubleValidator(0, FrameWork::maxPrice, 3, 0, nullptr); break;
    case Document::PriceDiff   : valid = new SmartDoubleValidator(-FrameWork::maxPrice,
                                                                  FrameWork::maxPrice, 3, 0, nullptr); break;
    case Document::Weight      : valid = new SmartDoubleValidator(0., 100000., 4, 0, nullptr); break;
    default                    : break;
    }

    if (!m_lineedit) {
        m_lineedit = new QLineEdit(parent);
        m_lineedit->setFrame(m_lineedit->style()->styleHint(QStyle::SH_ItemView_DrawDelegateFrame,
                                                            nullptr, m_lineedit));
    }
    m_lineedit->setAlignment(Qt::Alignment(idx.data(Qt::TextAlignmentRole).toInt()));
    if (valid)
        m_lineedit->setValidator(valid);

    return m_lineedit;
}

void DocumentDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

bool DocumentDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    if (!event || !view || (event->type() != QEvent::ToolTip))
        return QItemDelegate::helpEvent(event, view, option, idx);

    if (idx.isValid() && (idx.column() == Document::Picture)) {
        Document::Item *it = m_view->item(idx);
        if (it && it->item())
            BrickLink::ToolTip::inst()->show(it->item(), nullptr, event->globalPos(), view);
    } else {
        QString text = idx.data(Qt::DisplayRole).toString();
        QString toolTip = idx.data(Qt::ToolTipRole).toString();
        quint64 elideHash = quint64(idx.row()) << 32 | quint64(idx.column());
        if ((text != toolTip) || m_elided.contains(elideHash)) {
            if (!QToolTip::isVisible() || (QToolTip::text() != toolTip))
                QToolTip::showText(event->globalPos(), toolTip, view, option.rect);
        } else {
            QToolTip::hideText();
        }
    }
    return true;
}

void DocumentDelegate::languageChange()
{
    // these get recreated on the next use with the correct title
    delete m_select_color.data();
    delete m_select_item.data();
}

void DocumentDelegate::setReadOnly(bool ro)
{
    m_read_only = ro;
}

bool DocumentDelegate::isReadOnly() const
{
    return m_read_only;
}
