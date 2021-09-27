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
#include <QStyleOptionFrame>
#include <QStyle>
#include <QApplication>
#include <QScrollBar>
#include <QtMath>
#include <QScopeGuard>
#include <QStringBuilder>
#include <QMetaProperty>
#include <QPixmapCache>

#include "documentdelegate.h"
#include "selectitemdialog.h"
#include "selectcolordialog.h"
#include "utility/utility.h"
#include "framework.h"
#include "utility/smartvalidator.h"
#include "bricklink/bricklink_model.h"
#include "config.h"


QVector<QColor>                 DocumentDelegate::s_shades;
QCache<DocumentDelegate::TextLayoutCacheKey, QTextLayout> DocumentDelegate::s_textLayoutCache(5000);

static const quint64 differenceWarningMask = 0ULL
        | (1ULL << Document::PartNo)
        | (1ULL << Document::Color)
        | (1ULL << Document::Reserved);


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
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    DocumentDelegate *m_dd;
};

bool LanguageChangeHelper::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        m_dd->languageChange();
    return QObject::eventFilter(o, e);
}


DocumentDelegate::DocumentDelegate(QTableView *table)
    : QItemDelegate(table)
    , m_table(table)
{
    m_table->viewport()->setAttribute(Qt::WA_Hover);

    new LanguageChangeHelper(this, table);

    connect(BrickLink::core(), &BrickLink::Core::itemImageScaleFactorChanged,
            this, [this]() {
        m_table->resizeRowsToContents();
    });
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
            && (w > (idx.model()->headerData(idx.column(), Qt::Horizontal, Document::HeaderDefaultWidthRole).toInt()
                     * option1.fontMetrics.averageCharWidth()))) {
        w = int(w / 1.9);  // we can wrap to two lines (plus 10% safety margin)
    }

    if (idx.column() == Document::Color)
        w += (option1.decorationSize.width() + 4);

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

    const auto *lot = idx.data(Document::LotPointerRole).value<const Lot *>();
    const auto *base = idx.data(Document::BaseLotPointerRole).value<const Lot *>();
    const auto errorFlags = idx.data(Document::ErrorFlagsRole).value<quint64>();
    const auto differenceFlags = idx.data(Document::DifferenceFlagsRole).value<quint64>();

    p->save();
    auto restorePainter = qScopeGuard([p] { p->restore(); });

    Qt::Alignment align = (Qt::Alignment(idx.data(Qt::TextAlignmentRole).toInt()) & ~Qt::AlignVertical_Mask) | Qt::AlignVCenter;

    if ((idx.column() == Document::Index) && (p->device()->devType() != QInternal::Printer)) {
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        QStyleOptionHeader headerOption;
        headerOption.initFrom(option.widget);
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
    bool nocolor = !lot->color();
    bool noitem = !lot->item();

    const QFontMetrics &fm = option.fontMetrics;
    QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    QColor normalbg = Utility::premultiplyAlpha(option.palette.color(cg, (option.features & QStyleOptionViewItem::Alternate) ? QPalette::AlternateBase : QPalette::Base));
    QColor bg = normalbg;
    QColor fg = option.palette.color(cg, QPalette::Text);

    if (selected) {
        if (m_read_only) {
            bg = Utility::gradientColor(bg, option.palette.color(cg, QPalette::Highlight));
        } else {
            bg = Utility::premultiplyAlpha(option.palette.color(cg, QPalette::Highlight));
            if (!(option.state & QStyle::State_HasFocus))
                bg.setAlphaF(0.5);
            fg = Utility::premultiplyAlpha(option.palette.color(cg, QPalette::HighlightedText));
        }
    }

    int x = option.rect.x(), y = option.rect.y();
    int w = option.rect.width();
    int h = option.rect.height();
    int margin = int(std::ceil(2 * float(p->device()->logicalDpiX()) / 96.f));

    struct Tag {
        QColor foreground { Qt::transparent };
        QColor background { Qt::transparent };
        QString text;
        QPixmap icon;
        bool bold = false;
    } tag;

    if (differenceFlags & (1ULL << idx.column())) {
        bool warn = (differenceFlags & differenceWarningMask & (1ULL << idx.column()));
        int s = option.fontMetrics.height() / 10 * 8;
        QString key = "dd_ti_"_l1 % (warn ? "!"_l1 : ""_l1) % QString::number(s);
        QPixmap pix;

        if (!QPixmapCache::find(key, &pix)) {
            QIcon icon = warn ? QIcon::fromTheme("vcs-locally-modified-unstaged-small"_l1)
                              : QIcon::fromTheme("vcs-locally-modified-small"_l1);
            pix = QPixmap(icon.pixmap(s, QIcon::Normal, QIcon::On));
            QPixmapCache::insert(key, pix);
        }
        tag.icon = pix;
    }

    QImage image;
    QString str = displayData(idx, false);
    int checkmark = 0;
    bool selectionFrame = false;
    QColor selectionFrameFill = Qt::white;

    if (!selected) {
        switch (idx.column()) {
        case Document::ItemType:
            if (lot->itemType())
                bg = shadeColor(lot->itemTypeId(), 0.1);
            break;

        case Document::Category:
            if (lot->category())
                bg = shadeColor(int(lot->categoryId()), 0.2);
            break;

        case Document::Quantity:
            if (lot->quantity() <= 0)
                bg = (lot->quantity() == 0) ? QColor::fromRgbF(1, 1, 0, 0.4)
                                            : QColor::fromRgbF(1, 0, 0, 0.4);
            break;

        case Document::QuantityDiff:
            if (base && (base->quantity() < lot->quantity()))
                bg = QColor::fromRgbF(0, 1, 0, 0.3);
            else if (base && (base->quantity() > lot->quantity()))
                bg = QColor::fromRgbF(1, 0, 0, 0.3);
            break;

        case Document::PriceOrig:
        case Document::QuantityOrig:
            fg.setAlphaF(0.5);
            break;

        case Document::PriceDiff: {
            if (base && (base->price() < lot->price()))
                bg = QColor::fromRgbF(0, 1, 0, 0.3);
            else if (base && (base->price() > lot->price()))
                bg = QColor::fromRgbF(1, 0, 0, 0.3);
            break;
        }
        case Document::Total:
            bg = QColor::fromRgbF(1, 1, 0, 0.1);
            break;

        case Document::Condition:
            if (lot->condition() != BrickLink::Condition::New) {
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
    case Document::Marker: {
        if (lot->isMarked()) {
            selectionFrame = true;
            if (lot->markerColor().isValid()) {
                selectionFrameFill = lot->markerColor();
                fg = Utility::textColor(selectionFrameFill);
            } else {
                selectionFrameFill = bg;
            }
        }
        break;
    }
    case Document::Status: {
        int iconSize = std::min(fm.height() * 5 / 4, h * 3 / 4);
        QString key = "dd_st_"_l1 % QString::number(quint32(lot->status())) % "-"_l1 %
                QString::number(iconSize);
        QPixmap pix;

        if (!QPixmapCache::find(key, &pix)) {
            QIcon icon;
            switch (lot->status()) {
            case BrickLink::Status::Exclude: icon = QIcon::fromTheme("vcs-removed"_l1); break;
            case BrickLink::Status::Extra  : icon = QIcon::fromTheme("vcs-added"_l1); break;
            default                        :
            case BrickLink::Status::Include: icon = QIcon::fromTheme("vcs-normal"_l1); break;
            }
            pix = QPixmap(icon.pixmap(iconSize));
            QPixmapCache::insert(key, pix);
        }
        image = pix.toImage();

        uint altid = lot->alternateId();
        bool cp = lot->counterPart();
        if (altid || cp) {
            tag.text = cp ? "CP"_l1 : QString::number(altid);
            tag.bold = (cp || !lot->alternate());
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
        if (lot->item() && lot->item()->hasInventory()) {
            tag.text = tr("Inv");
            tag.foreground = bg;
            tag.background = fg;
            tag.background.setAlphaF(0.3);

            if ((option.state & QStyle::State_MouseOver) && lot->quantity()) {
                tag.foreground = option.palette.color(QPalette::HighlightedText);
                tag.background = option.palette.color(QPalette::Highlight);
            }
        }
        break;

    case Document::Picture: {
        BrickLink::Picture *pic = BrickLink::core()->picture(lot->item(), lot->color());
        double dpr = p->device()->devicePixelRatioF();
        QSize s = option.rect.size();

        if (pic && pic->isValid()) {
            image = pic->image().scaled(s * dpr, Qt::KeepAspectRatio, Qt::FastTransformation);
            image.setDevicePixelRatio(dpr);
        } else {
            image = BrickLink::core()->noImage(s);
        }

        selectionFrame = true;
        break;
    }
    case Document::Color: {
        image = BrickLink::core()->colorImage(lot->color(), option.decorationSize.width(),
                                              option.rect.height());
        break;
    }
    case Document::Retain:
        checkmark = lot->retain() ? 1 : -1;
        break;

    case Document::Stockroom:
        if (lot->stockroom() == BrickLink::Stockroom::None)
            checkmark = -1;
        else
            str = QLatin1Char('A' + char(lot->stockroom()) - char(BrickLink::Stockroom::A));
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
    p->fillRect(option.rect, selectionFrame ? selectionFrameFill : bg);

    if (nocolor || noitem) {
        int d = option.rect.height();
        QString key = "dd_stripe_"_l1 % QString::number(d);
        QPixmap pix;
        if (!QPixmapCache::find(key, &pix)) {
            pix = QPixmap::fromImage(Utility::stripeImage(d, Qt::red));
            QPixmapCache::insert(key, pix);
        }
        int offset = (option.features & QStyleOptionViewItem::Alternate) ? d : 0;
        offset -= m_table->horizontalScrollBar()->value();
        p->drawTiledPixmap(option.rect, pix, QPoint(option.rect.left() - offset, 0));
    }

    if (!tag.text.isEmpty()) {
        QFont font = option.font;
        font.setBold(tag.bold);
        QFontMetrics fontmetrics(font);
        int itw = qMax(int(1.5 * fontmetrics.height()),
                       2 * fontmetrics.horizontalAdvance(tag.text));

        QString key = "dd_tag_"_l1 % QString::number(itw) % "-"_l1 % tag.background.name();
        QPixmap pix;
        if (!QPixmapCache::find(key, &pix)) {
            pix = QPixmap(itw, itw);
            pix.fill(Qt::transparent);
            QPainter pixp(&pix);

            QRadialGradient grad(pix.rect().bottomRight(), pix.width());
            grad.setColorAt(0, tag.background);
            grad.setColorAt(0.6, tag.background);
            grad.setColorAt(1, Qt::transparent);

            pixp.fillRect(pix.rect(), grad);
            pixp.end();
            QPixmapCache::insert(key, pix);
        }
        int pw = qMin(pix.width(), option.rect.width());
        int ph = qMin(pix.height(), option.rect.height());

        p->drawPixmap(option.rect.right() - pw + 1, option.rect.bottom() - ph + 1, pix,
                      pix.width() - pw, pix.height() - ph, pw, ph);

        if (!tag.text.isEmpty()) {
            p->setPen(tag.foreground);
            QFont oldfont = p->font();
            p->setFont(font);
            p->drawText(option.rect.adjusted(0, 0, -1, -1), Qt::AlignRight | Qt::AlignBottom, tag.text);
            p->setFont(oldfont);
        }
    }
    if (!tag.icon.isNull()) {
        int s = tag.icon.size().height();

        QRect r { option.rect.topLeft(), QSize(s, s) };
        if (align & Qt::AlignLeft)
            r.moveRight(option.rect.right());
        p->setOpacity(0.5);
        p->drawPixmap(r, tag.icon);
        p->setOpacity(1);
    }

    if (errorFlags & (1ULL << idx.column())) {
        p->setPen(QColor::fromRgbF(1, 0, 0, 0.75));
        p->drawRect(QRectF(x+.5, y+.5, w-1, h-1));
        p->setPen(QColor::fromRgbF(1, 0, 0, 0.50));
        p->drawRect(QRectF(x+1.5, y+1.5, w-3, h-3));
    }

    p->setPen(fg);

    if (checkmark != 0) {
        QStyleOptionViewItem opt(option);
        opt.state &= ~QStyle::State_HasFocus;
        opt.state |= ((checkmark > 0) ? QStyle::State_On : QStyle::State_Off);
        opt.features |= QStyleOptionViewItem::HasCheckIndicator;

        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        QRect r = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, option.widget);
        r = r.translated(-r.left() + opt.rect.left(), 0);
        int dx = margin;
        if (align & Qt::AlignHCenter)
            dx = (w - r.width()) / 2;
        else if (align & Qt::AlignRight)
            dx = (w - r.width() - margin);
        opt.rect = r.translated(dx, 0);
        style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &opt, p, option.widget);
    }
    else if (!image.isNull()) {
        int px = x;
        int py = y;

        QSize imgSize = image.size() / image.devicePixelRatio();

        if (align & Qt::AlignHCenter)
            px = option.rect.left() + (w - imgSize.width()) / 2;
        if (align & Qt::AlignVCenter)
            py = option.rect.top() + (h - imgSize.height()) / 2;

        p->drawImage(QPointF(px, py), image);

        int delta = px + imgSize.width() + margin;
        w -= (delta - x);
        x = delta;
    }

    static const QVector<int> richTextColumns = {
        Document::Description,
        Document::Color,
        Document::Condition,
        Document::Comments,
        Document::Remarks,
        Document::Category,
        Document::ItemType,
        Document::Reserved,
        Document::Marker,
    };

    if (!str.isEmpty()) {
        int rw = w - 2 * margin;

        if (!richTextColumns.contains(idx.column())) {
            p->drawText(x + margin, y, rw, h, int(align), str);
            return;
        }

        static const QString elide = "..."_l1;
        auto key = TextLayoutCacheKey { str, QSize(rw, h), uint(fm.height()) };
        QTextLayout *tlp = s_textLayoutCache.object(key);

        // an unlikely hash collision
        if (tlp && !(tlp->text() == str && QFontMetrics(tlp->font()).height() == fm.height())) {
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
            int height = tlp->lineCount() * (fm.leading() + int(lastLine.height()));

            quint64 elideHash = quint64(idx.row()) << 32 | quint64(idx.column());

            p->setClipRect(option.rect.adjusted(margin, 0, -margin, 0)); // QTextLayout doesn't clip
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

    switch (e->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonDblClick: {
        m_multiEdit = true; /*(e->type() == QEvent::KeyPress)
                && (static_cast<QInputEvent *>(e)->modifiers() & Qt::ControlModifier);*/
        if (nonInlineEdit(e, option, idx))
            return true;
        break;
    }
    default: break;
    }

    return QItemDelegate::editorEvent(e, model, option, idx);
}

bool DocumentDelegate::nonInlineEdit(QEvent *e, const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    QAbstractItemModel *model = const_cast<QAbstractItemModel *>(idx.model()); //TODO: replace with explicit parameter

    bool accept = true;

    bool dblclick = (e->type() == QEvent::MouseButtonDblClick);
    bool keypress = (e->type() == QEvent::KeyPress);
    bool editkey = false;
    bool fakeeditkey = false;
    int key = -1;

    if (keypress) {
        auto *ke = static_cast<QKeyEvent *>(e);
        fakeeditkey = (ke->key() == Qt::Key_Execute);
        editkey = ((ke->key() == Qt::Key_Return) || (ke->key() == Qt::Key_Enter))
                && ((ke->modifiers() == Qt::NoModifier) || (ke->modifiers() == Qt::ControlModifier));
    }

    switch (idx.column()) {
    case Document::Retain:
        if (dblclick || (keypress && editkey))
            setModelDataInternal(!idx.data(Qt::EditRole).toBool(), model, idx);
        break;

    case Document::Stockroom: {
        bool noneKey = (QKeySequence(key) == QKeySequence(tr("-", "set stockroom to none")));
        const BrickLink::Stockroom value = idx.data(Qt::EditRole).value<BrickLink::Stockroom>();
        BrickLink::Stockroom setValue = value;

        if (keypress && (key == Qt::Key_A))
            setValue = BrickLink::Stockroom::A;
        else if (keypress && (key == Qt::Key_B))
            setValue = BrickLink::Stockroom::B;
        else if (keypress && (key == Qt::Key_C))
            setValue = BrickLink::Stockroom::C;
        else if (keypress && noneKey)
            setValue = BrickLink::Stockroom::None;
        else if (dblclick || (keypress && editkey))
            setValue = BrickLink::Stockroom((int(value) + 1) % int(BrickLink::Stockroom::Count));

        if (setValue != value)
            setModelDataInternal(QVariant::fromValue(setValue), model, idx);
        break;
    }
    case Document::Condition: {
        bool newKey = (QKeySequence(key) == QKeySequence(tr("N", "set condition to new")));
        bool usedKey = (QKeySequence(key) == QKeySequence(tr("U", "set condition to used")));

        const BrickLink::Condition value = idx.data(Qt::EditRole).value<BrickLink::Condition>();
        BrickLink::Condition setValue = value;

        if (keypress && newKey)
            setValue = BrickLink::Condition::New;
        else if (keypress && usedKey)
            setValue = BrickLink::Condition::Used;
        else if (dblclick || (keypress && editkey))
            setValue = BrickLink::Condition((int(value) + 1) % int(BrickLink::Condition::Count));

        if (setValue != value)
            setModelDataInternal(QVariant::fromValue(setValue), model, idx);
        break;
    }
    case Document::Status: {
        bool includeKey = (QKeySequence(key) == QKeySequence(tr("I", "set status to include")));
        bool excludeKey = (QKeySequence(key) == QKeySequence(tr("E", "set status to exclude")));
        bool extraKey = (QKeySequence(key) == QKeySequence(tr("X", "set status to extra")));

        const BrickLink::Status value = idx.data(Qt::EditRole).value<BrickLink::Status>();
        BrickLink::Status setValue = value;

        if (keypress && includeKey)
            setValue = BrickLink::Status::Include;
        else if (keypress && excludeKey)
            setValue = BrickLink::Status::Exclude;
        else if (keypress && extraKey)
            setValue = BrickLink::Status::Extra;
        else if (dblclick || (keypress && editkey))
            setValue = BrickLink::Status((int(value) + 1) % int(BrickLink::Status::Count));

        if (setValue != value)
            setModelDataInternal(QVariant::fromValue(setValue), model, idx);
        break;
    }
    case Document::Description: {
        auto item = idx.data(Qt::EditRole).value<const BrickLink::Item *>();

        if (dblclick && item && item->hasInventory()) {
            auto me = static_cast<QMouseEvent *>(e);
            int d = option.rect.height() / 2;

            if ((me->pos().x() > (option.rect.right() - d))
                    && (me->pos().y() > (option.rect.bottom() - d))) {
                if (auto a = FrameWork::inst()->findAction("edit_partoutitems")) {
                    if (a->isEnabled())
                        a->trigger();
                    break;
                }
            }
        }
        Q_FALLTHROUGH();
    }
    case Document::Picture:
        if (dblclick || (keypress && (editkey || fakeeditkey))) {
            if (!m_select_item) {
                m_select_item = new SelectItemDialog(true /*popup mode*/, m_table);
                m_select_item->setWindowTitle(tr("Modify Item"));
            }
            auto item = idx.data(Qt::EditRole).value<const BrickLink::Item *>();

            m_select_item->setItem(item);

            QRect centerOn;
            if (!fakeeditkey) {
                centerOn = QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()),
                                 option.rect.size());
            }

            if (m_select_item->execAtPosition(centerOn) == QDialog::Accepted)
                setModelDataInternal(QVariant::fromValue(m_select_item->item()), model, idx);
        }
        break;

    case Document::Color:
        if (dblclick || (keypress && (editkey || fakeeditkey))) {
            if (!m_select_color) {
                m_select_color = new SelectColorDialog(true /*popup mode*/, m_table);
                m_select_color->setWindowTitle(tr("Modify Color"));
            }
            auto color = idx.data(Qt::EditRole).value<const BrickLink::Color *>();
            auto item = idx.siblingAtColumn(Document::Description).data(Qt::EditRole)
                    .value<const BrickLink::Item *>();

            m_select_color->setColorAndItem(color, item);

            QRect centerOn;
            if (!fakeeditkey) {
                centerOn = QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()),
                                 option.rect.size());
            }

            if (m_select_color->execAtPosition(centerOn) == QDialog::Accepted)
                setModelDataInternal(QVariant::fromValue(m_select_color->color()), model, idx);
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

    if (!idx.isValid())
        return nullptr;

    QValidator *valid = nullptr;
    switch (idx.column()) {
    case Document::PartNo      : valid = new QRegularExpressionValidator(QRegularExpression(R"([a-zA-Z0-9._-]+)"_l1), nullptr); break;
    case Document::Sale        : valid = new SmartIntValidator(-1000, 99, 0, nullptr); break;
    case Document::Quantity    :
    case Document::QuantityDiff: valid = new SmartIntValidator(-FrameWork::maxQuantity,
                                                               FrameWork::maxQuantity, 0, nullptr); break;
    case Document::Bulk        : valid = new SmartIntValidator(1, FrameWork::maxQuantity, 1, nullptr); break;
    case Document::TierQ1      :
    case Document::TierQ2      :
    case Document::TierQ3      : valid = new SmartIntValidator(0, FrameWork::maxQuantity, 0, nullptr); break;
    case Document::Price       :
    case Document::Cost        :
    case Document::TierP1      :
    case Document::TierP2      :
    case Document::TierP3      : valid = new SmartDoubleValidator(0, FrameWork::maxPrice, 3, 0, nullptr); break;
    case Document::PriceDiff   : valid = new SmartDoubleValidator(-FrameWork::maxPrice,
                                                                  FrameWork::maxPrice, 3, 0, nullptr); break;
    case Document::Weight      : valid = new SmartDoubleValidator(0., 100000., 2, 0, nullptr); break;
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
    m_multiEdit = true; /*(qApp->keyboardModifiers() & Qt::ControlModifier);*/

    return m_lineedit;
}

void DocumentDelegate::destroyEditor(QWidget *editor, const QModelIndex &index) const
{
    if (editor != m_lineedit) {
        QItemDelegate::destroyEditor(editor, index);
    } else {
        auto oldValid = m_lineedit->validator();
        m_lineedit->setValidator(nullptr);
        delete oldValid;
    }
}

void DocumentDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

bool DocumentDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                 const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    if (!event || !view || !idx.isValid() || (event->type() != QEvent::ToolTip))
        return QItemDelegate::helpEvent(event, view, option, idx);

    auto f = static_cast<Document::Field>(idx.column());

    const auto *item = idx.data(Document::LotPointerRole).value<const Lot *>();
    if (!item)
        return false;

    if (f == Document::Picture) {
        if (item->item())
            BrickLink::ToolTip::inst()->show(item->item(), nullptr, event->globalPos(), view);
    } else {
        QString text = displayData(idx, false);
        QString tip = displayData(idx, true);

        const auto differenceFlags = idx.data(Document::DifferenceFlagsRole).value<quint64>();
        if (differenceFlags & (1ULL << idx.column())) {
            if (tip.isEmpty())
                tip = text;
            if (differenceFlags & differenceWarningMask & (1ULL << idx.column())) {
                tip = tip % u"<br><br>" %
                        tr("This change cannot be applied via BrickLink's Mass-Update mechanism!");
            }
            QString oldText = displayData(idx, true, true);

            tip = tip % u"<br><br>" %
                    tr("The original value of this field was:") % u"<br><b>" % oldText % u"</b>";
        }

        bool isElided = m_elided.contains(quint64(idx.row()) << 32 | quint64(idx.column()));
        if (!tip.isEmpty() && ((tip != text) || isElided)) {
            if (!QToolTip::isVisible() || (QToolTip::text() != tip))
                QToolTip::showText(event->globalPos(), tip, view, option.rect);
        } else {
            QToolTip::hideText();
        }
    }
    return true;
}

void DocumentDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QVariant v = index.data(Qt::EditRole);
    switch (static_cast<Document::Field>(index.column())) {
    case Document::Price:
    case Document::PriceDiff:
    case Document::Cost:
    case Document::TierP1:
    case Document::TierP2:
    case Document::TierP3:
        v = Currency::toString(v.toDouble());
        break;
    case Document::Weight:
        v = Utility::weightToString(v.toDouble(), Config::inst()->measurementSystem());
        break;
    default:
        break;
    }

    QByteArray n = editor->metaObject()->userProperty().name();
    if (!n.isEmpty()) {
        if (!v.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            v = QVariant(editor->property(n).userType(), nullptr);
#else
            v = QVariant(editor->property(n).metaType(), nullptr);
#endif
        }
        editor->setProperty(n, v);
    }
}

void DocumentDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QByteArray n = editor->metaObject()->userProperty().name();
    if (!n.isEmpty()) {
        QVariant v = editor->property(n);
        switch (static_cast<Document::Field>(index.column())) {
        case Document::Price:
        case Document::PriceDiff:
        case Document::Cost:
        case Document::TierP1:
        case Document::TierP2:
        case Document::TierP3:
            v = Currency::fromString(v.toString());
            break;
        case Document::Weight:
            v = Utility::stringToWeight(v.toString(), Config::inst()->measurementSystem());
            break;
        default:
            break;
        }

        setModelDataInternal(v, model, index);
    }
}

void DocumentDelegate::setModelDataInternal(const QVariant &value, QAbstractItemModel *model,
                                            const QModelIndex &index) const
{
    if (!m_multiEdit) {
        model->setData(index, value);
    } else {
        auto selection = m_table->selectionModel()->selectedRows();
        for (const auto &s : selection)
            model->setData(index.siblingAtRow(s.row()), value);
    }
}

QString DocumentDelegate::displayData(const QModelIndex &idx, bool toolTip, bool differenceBase)
{
    QVariant v = idx.data(differenceBase ? int(Document::BaseDisplayRole) : int(Qt::DisplayRole));
    QLocale loc;
    static const QString dash = QStringLiteral("-");

    switch (idx.column()) {
    case Document::Status: {
        if (!toolTip)
            return { };

        QString tip;
        switch (v.value<BrickLink::Status>()) {
        case BrickLink::Status::Exclude: tip = tr("Exclude"); break;
        case BrickLink::Status::Extra  : tip = tr("Extra"); break;
        case BrickLink::Status::Include: tip = tr("Include"); break;
        default                        : break;
        }

        const auto *item = idx.data(Document::LotPointerRole).value<const Lot *>();
        if (item->counterPart())
            tip = tip % u"<br>(" % tr("Counter part") % u")";
        else if (item->alternateId())
            tip = tip % u"<br>(" % tr("Alternate match id: %1").arg(item->alternateId()) % u")";
        return tip;
    }
    case Document::Condition: {
        QString str;
        if (!toolTip) {
            str = (v.value<BrickLink::Condition>() == BrickLink::Condition::New)
                    ?  tr("N", "List>Cond>New") : tr("U", "List>Cond>Used");
        } else {
            str = (v.value<BrickLink::Condition>() == BrickLink::Condition::New)
                    ?  tr("New", "ToolTip Cond>New") : tr("Used", "ToolTip Cond>Used");
        }

        const auto *item = idx.data(Document::LotPointerRole).value<const Lot *>();
        if (item && item->itemType() && item->itemType()->hasSubConditions()
                && (item->subCondition() != BrickLink::SubCondition::None)) {
            QString scStr;
            switch (item->subCondition()) {
            case BrickLink::SubCondition::None      : scStr = QStringLiteral("-"); break;
            case BrickLink::SubCondition::Sealed    : scStr = tr("Sealed"); break;
            case BrickLink::SubCondition::Complete  : scStr = tr("Complete"); break;
            case BrickLink::SubCondition::Incomplete: scStr = tr("Incomplete"); break;
            default: break;
            }
            if (!scStr.isEmpty()) {
                if (toolTip)
                    str = str % u"<br><i>" % scStr % u"</i>";
                else
                    str = str % u" (" % scStr % u")";
            }
        }
        return str;
    }
    case Document::Retain: {
        if (!toolTip)
            return { };
        return v.toBool() ? tr("Retain") : tr("Do not retain");
    }
    case Document::Stockroom: {
        if (!toolTip)
            return { };

        QString tip;
        switch (v.value<BrickLink::Stockroom>()) {
        case BrickLink::Stockroom::A   : tip = QString::fromLatin1("A"); break;
        case BrickLink::Stockroom::B   : tip = QString::fromLatin1("B"); break;
        case BrickLink::Stockroom::C   : tip = QString::fromLatin1("C"); break;
        default:
        case BrickLink::Stockroom::None: tip = tr("None", "ToolTip Stockroom>None"); break;
        }
        tip = tr("Stockroom") % u": " % tip;
        return tip;
    }
    case Document::QuantityOrig:
    case Document::QuantityDiff:
    case Document::Quantity:
    case Document::TierQ1:
    case Document::TierQ2:
    case Document::TierQ3: {
        int i = v.toInt();
        return (!i && !toolTip) ? dash : loc.toString(i);
    }
    case Document::YearReleased:
    case Document::LotId: {
        uint i = v.toUInt();
        return (!i && !toolTip) ? dash : QString::number(i);
    }
    case Document::PriceOrig:
    case Document::PriceDiff:
    case Document::Price:
    case Document::Total:
    case Document::Cost:
    case Document::TierP1:
    case Document::TierP2:
    case Document::TierP3: {
        double d = v.toDouble();
        return (qFuzzyIsNull(d) && !toolTip) ? dash : Currency::toDisplayString(d);
    }
    case Document::Bulk: {
        int i = v.toInt();
        return ((i == 1) && !toolTip) ? dash : loc.toString(i);
    }
    case Document::Sale: {
        int i = v.toInt();
        return (!i && !toolTip) ? dash : loc.toString(i) % u'%';
    }
    case Document::Weight: {
        double d = v.toDouble();
        return (qFuzzyIsNull(d) && !toolTip) ? dash : Utility::weightToString
                                               (d, Config::inst()->measurementSystem(), true, true);
    }
    default:
        return v.toString();
    }
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
