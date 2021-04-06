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
#include <QDebug>

#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/currency.h"
#include "common/documentmodel.h"
#include "common/eventfilter.h"
#include "utility/utility.h"
#include "documentdelegate.h"
#include "selectitemdialog.h"
#include "selectcolordialog.h"
#include "smartvalidator.h"


QCache<DocumentDelegate::TextLayoutCacheKey, QTextLayout> DocumentDelegate::s_textLayoutCache(5000);

static const quint64 differenceWarningMask = 0ULL
        | (1ULL << DocumentModel::PartNo)
        | (1ULL << DocumentModel::Color)
        | (1ULL << DocumentModel::Reserved);

static int columnSpacingInPixel()
{
    return 2 << Config::inst()->columnSpacing(); // 0,1,2 -> 2,4,8
}


DocumentDelegate::DocumentDelegate(QTableView *table)
    : QItemDelegate(table)
    , m_table(table)
{
    m_table->viewport()->setAttribute(Qt::WA_Hover);

    // we can't use eventFilter() in anything derived from QAbstractItemDelegate:
    // the eventFilter() function there will filter ANY event, because it thinks
    // everything is an editor widget.
    new EventFilter(table, { QEvent::LanguageChange }, [this](QObject *, QEvent *) {
        languageChange();
        return EventFilter::ContinueEventProcessing;
    });
}

int DocumentDelegate::defaultItemHeight(const QWidget *w)
{
    QSize picsize = BrickLink::core()->standardPictureSize()
            * double(Config::inst()->rowHeightPercent()) / 100.;

    QFontMetrics fm(w ? w->font() : QApplication::font("QTableView"));

    return qMax(2 + fm.height(), picsize.height() + 1 /* the grid lines */);
}

QSize DocumentDelegate::sizeHint(const QStyleOptionViewItem &option1, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return {};

    int w = -1;
    const int columnSpacing = columnSpacingInPixel();

    if (idx.column() == DocumentModel::Picture) {
        w = 4 + (BrickLink::core()->standardPictureSize()
                 * double(Config::inst()->rowHeightPercent()) / 100.0).width();
    } else {
        w = QItemDelegate::sizeHint(option1, idx).width();

        const int qtTextMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr) + 1;
        w -= 2 * qtTextMargin;
        w += 2 * columnSpacing;
    }

    static const QVector<int> twoLiners = {
        DocumentModel::Description,
        DocumentModel::Remarks,
        DocumentModel::Comments
    };

    if (twoLiners.contains(idx.column())
            && (w > (idx.model()->headerData(idx.column(), Qt::Horizontal, DocumentModel::HeaderDefaultWidthRole).toInt()
                     * option1.fontMetrics.averageCharWidth()))) {
        w = int(w / 1.9);  // we can wrap to two lines (plus 10% safety margin)
    }

    if (idx.column() == DocumentModel::Color)
        w += (option1.decorationSize.width() + columnSpacing);

    QStyleOptionViewItem option(option1);

    return { w + 1 /* the grid lines*/, defaultItemHeight(option.widget) };
}

inline size_t qHash(const DocumentDelegate::TextLayoutCacheKey &key, size_t seed)
{
    size_t sizeHash = qHash((key.size.width() << 16) ^ key.size.height(), seed);
    return qHash(key.text) ^ sizeHash ^ key.fontSize ^ seed;
}

void DocumentDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return;

    const auto *lot = idx.data(DocumentModel::LotPointerRole).value<const Lot *>();
    const auto *base = idx.data(DocumentModel::BaseLotPointerRole).value<const Lot *>();
    const auto errorFlags = idx.data(DocumentModel::ErrorFlagsRole).toULongLong();
    const auto differenceFlags = idx.data(DocumentModel::DifferenceFlagsRole).toULongLong();

    p->save();
    auto restorePainter = qScopeGuard([p] { p->restore(); });

    Qt::Alignment align = (Qt::Alignment(idx.data(Qt::TextAlignmentRole).toInt()) & ~Qt::AlignVertical_Mask) | Qt::AlignVCenter;

    bool isPrinting = (p->device()->devType() == QInternal::Printer);

    bool selected = (option.state & QStyle::State_Selected);
    bool active = (option.state & QStyle::State_Active);
    bool disabled = !(option.state & QStyle::State_Enabled);
    bool focus = (option.state & QStyle::State_HasFocus);
    bool nocolor = !lot->color();
    bool noitem = !lot->item();

    const QFontMetrics &fm = option.fontMetrics;
    QPalette::ColorGroup cg = disabled ? QPalette::Disabled : (active ? QPalette::Active : QPalette::Inactive);
    QColor normalbg = Utility::premultiplyAlpha(option.palette.color(cg, (option.features & QStyleOptionViewItem::Alternate) ? QPalette::AlternateBase : QPalette::Base));
    QColor bg = normalbg;
    QColor fg = option.palette.color(cg, QPalette::Text);

    if (selected) {
        if (m_read_only) {
            bg = Utility::gradientColor(bg, option.palette.color(cg, QPalette::Highlight));
        } else {
            bg = Utility::premultiplyAlpha(option.palette.color(QPalette::Active, QPalette::Highlight));
            fg = Utility::premultiplyAlpha(option.palette.color(QPalette::Active, QPalette::HighlightedText));
            if (!focus)
                bg.setAlphaF(0.65f);
            if (!active || disabled)
                bg = QColor::fromHslF(bg.hslHueF(), bg.hslSaturationF() / 4, bg.lightnessF(), bg.alphaF());
        }
    }

    int x = option.rect.x(), y = option.rect.y();
    int w = option.rect.width();
    int h = option.rect.height();
    int margin = columnSpacingInPixel();

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
        QString key = u"dd_ti_"_qs % (warn ? u"!"_qs : u""_qs) % QString::number(s);
        QPixmap pix;

        if (!QPixmapCache::find(key, &pix)) {
            QIcon icon = warn ? QIcon::fromTheme(u"vcs-locally-modified-unstaged-small"_qs)
                              : QIcon::fromTheme(u"vcs-locally-modified-small"_qs);
            pix = QPixmap(icon.pixmap(s, QIcon::Normal, QIcon::On));
            QPixmapCache::insert(key, pix);
        }
        tag.icon = pix;
    }

    QImage image;
    QVariant display = idx.data(Qt::DisplayRole);
    QString str = displayData(idx, display, false);
    int checkmark = 0;
    bool selectionFrame = false;
    QColor selectionFrameFill = Qt::white;

    if (!selected) {
        switch (idx.column()) {
        case DocumentModel::ItemType:
            if (lot->itemType())
                bg = Utility::shadeColor(lot->itemTypeId(), 0.1f);
            break;

        case DocumentModel::Category:
            if (lot->category())
                bg = Utility::shadeColor(int(lot->categoryId()), 0.2f);
            break;

        case DocumentModel::Quantity:
            if (lot->quantity() <= 0)
                bg = (lot->quantity() == 0) ? QColor::fromRgbF(1, 1, 0, 0.4f)
                                            : QColor::fromRgbF(1, 0, 0, 0.4f);
            break;

        case DocumentModel::QuantityDiff:
            if (base && (base->quantity() < lot->quantity()))
                bg = QColor::fromRgbF(0, 1, 0, 0.3f);
            else if (base && (base->quantity() > lot->quantity()))
                bg = QColor::fromRgbF(1, 0, 0, 0.3f);
            break;

        case DocumentModel::PriceOrig:
        case DocumentModel::QuantityOrig:
            fg.setAlphaF(0.5f);
            break;

        case DocumentModel::PriceDiff: {
            if (base && (base->price() < lot->price()))
                bg = QColor::fromRgbF(0, 1, 0, 0.3f);
            else if (base && (base->price() > lot->price()))
                bg = QColor::fromRgbF(1, 0, 0, 0.3f);
            break;
        }
        case DocumentModel::Total:
            bg = QColor::fromRgbF(1, 1, 0, 0.1f);
            break;

        case DocumentModel::Condition:
            if (lot->condition() != BrickLink::Condition::New) {
                bg = fg;
                bg.setAlphaF(0.3f);
            }
            break;

        case DocumentModel::TierP1:
        case DocumentModel::TierQ1:
            bg = fg;
            bg.setAlphaF(0.06f);
            break;

        case DocumentModel::TierP2:
        case DocumentModel::TierQ2:
            bg = fg;
            bg.setAlphaF(0.12f);
            break;

        case DocumentModel::TierP3:
        case DocumentModel::TierQ3:
            bg = fg;
            bg.setAlphaF(0.18f);
            break;

        }
    }

    switch (idx.column()) {
    case DocumentModel::Marker: {
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
    case DocumentModel::Status: {
        int iconSize = std::min(fm.height() * 5 / 4, h * 3 / 4);
        QString key = u"dd_st_" % QString::number(quint32(lot->status())) % u"-" %
                QString::number(iconSize);
        QPixmap pix;

        if (!QPixmapCache::find(key, &pix)) {
            QIcon icon;
            switch (lot->status()) {
            case BrickLink::Status::Exclude: icon = QIcon::fromTheme(u"vcs-removed"_qs); break;
            case BrickLink::Status::Extra  : icon = QIcon::fromTheme(u"vcs-added"_qs); break;
            default                        :
            case BrickLink::Status::Include: icon = QIcon::fromTheme(u"vcs-normal"_qs); break;
            }
            pix = QPixmap(icon.pixmap(iconSize));
            QPixmapCache::insert(key, pix);
        }
        image = pix.toImage();

        uint altid = lot->alternateId();
        bool cp = lot->counterPart();
        if (altid || cp) {
            tag.text = cp ? u"CP"_qs : QString::number(altid);
            tag.bold = (cp || !lot->alternate());
            tag.foreground = fg;
            if (cp || selected) {
                tag.background = fg;
                tag.background.setAlphaF(0.3f);
            } else {
                tag.background = Utility::shadeColor(int(1 + altid), .5f);
            }
        }
        break;
    }
    case DocumentModel::Description:
        if (lot->item() && lot->item()->hasInventory()) {
            tag.text = tr("Inv");
            tag.foreground = bg;
            tag.background = fg;
            tag.background.setAlphaF(0.3f);

            if ((option.state & QStyle::State_MouseOver) && lot->quantity()) {
                tag.foreground = option.palette.color(QPalette::HighlightedText);
                tag.background = option.palette.color(QPalette::Highlight);
            }
        }
        break;

    case DocumentModel::Picture: {
        image = display.value<QImage>();
        double dpr = p->device()->devicePixelRatioF();
        QSize s = option.rect.size();

        if (!image.isNull()) {
            image = image.scaled(s * dpr, Qt::KeepAspectRatio, Qt::FastTransformation);
            image.setDevicePixelRatio(dpr);
        } else {
            image = BrickLink::core()->noImage(s);
        }

        selectionFrame = true;
        break;
    }
    case DocumentModel::Color: {
        if (lot->color())
            image = lot->color()->sampleImage(option.decorationSize.width(), option.rect.height());
        break;
    }
    case DocumentModel::Retain:
        checkmark = lot->retain() ? 1 : -1;
        break;

    case DocumentModel::Stockroom:
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
        QString key = u"dd_stripe_" % QString::number(d);
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

        QString key = u"dd_tag_" % QString::number(itw) % u"-" % tag.background.name();
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
        if (isPrinting) {
            str = QString((checkmark > 0) ? u'\x2714' : u' ');
        } else {
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
        DocumentModel::Description,
        DocumentModel::Color,
        DocumentModel::Condition,
        DocumentModel::Comments,
        DocumentModel::Remarks,
        DocumentModel::Category,
        DocumentModel::ItemType,
        DocumentModel::Reserved,
        DocumentModel::Marker,
        DocumentModel::DateAdded,
        DocumentModel::DateLastSold,
    };

    if (!str.isEmpty()) {
        int rw = w - 2 * margin;

        if (!richTextColumns.contains(idx.column())) {
            p->drawText(x + margin, y, rw, h, int(align), str);
        } else {
            static const QString elide = u"..."_qs;
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
                p->setClipRect(option.rect);
            }
        }
    }
    if (selectionFrame && selected) {
        int lines = qMax(4, option.rect.height() / 20);
        QColor c = bg;
        for (int i = 0; i < lines; ++i) {
            c.setAlphaF(1.f - float(i) / float(lines + lines - 2));
            p->setPen(c);
            p->drawRect(option.rect.adjusted(i, i, -i - 1, -i -1));
        }
    }
    if (focus && !selected) {
        p->setPen(QPen { fg, 1, Qt::DotLine });
        p->drawRect(option.rect.adjusted(0, 0, -1, -1));
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
    QAbstractItemModel *model = const_cast<QAbstractItemModel *>(idx.model());

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
    case DocumentModel::Retain:
        if (dblclick || (keypress && editkey))
            setModelDataInternal(!idx.data(Qt::EditRole).toBool(), model, idx);
        break;

    case DocumentModel::Stockroom: {
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
    case DocumentModel::Condition: {
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
    case DocumentModel::Status: {
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
    case DocumentModel::Description: {
        auto item = idx.data(Qt::EditRole).value<const BrickLink::Item *>();

        if (dblclick && item && item->hasInventory()) {
            auto me = static_cast<QMouseEvent *>(e);
            int d = option.rect.height() / 2;

            if ((me->pos().x() > (option.rect.right() - d))
                    && (me->pos().y() > (option.rect.bottom() - d))) {
                if (auto a = ActionManager::inst()->qAction("edit_partoutitems")) {
                    if (a->isEnabled())
                        a->trigger();
                    break;
                }
            }
        }
        Q_FALLTHROUGH();
    }
    case DocumentModel::Picture:
        if (dblclick || (keypress && (editkey || fakeeditkey))) {
            if (!m_select_item) {
                m_select_item = new SelectItemDialog(true /*popup mode*/, m_table);
                m_select_item->setWindowTitle(tr("Modify Item"));
                m_select_item->setWindowModality(Qt::ApplicationModal);

                connect(m_select_item, &QDialog::finished, this, [this](int result) {
                    if (result == QDialog::Accepted) {
                        auto ctxIdx = m_select_item->property("contextIndex").toModelIndex();
                        QAbstractItemModel *ctxModel = const_cast<QAbstractItemModel *>(ctxIdx.model());
                        setModelDataInternal(QVariant::fromValue(m_select_item->item()), ctxModel, ctxIdx);
                    }
                });
            }
            auto item = idx.data(Qt::EditRole).value<const BrickLink::Item *>();
            m_select_item->setItem(item);

            QRect centerOn;
            if (!fakeeditkey) {
                centerOn = QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()),
                                 option.rect.size());
            }

            m_select_item->setPopupPosition(centerOn);
            m_select_item->setProperty("contextIndex", idx);
            m_select_item->show();
        }
        break;

    case DocumentModel::Color:
        if (dblclick || (keypress && (editkey || fakeeditkey))) {
            if (!m_select_color) {
                m_select_color = new SelectColorDialog(true /*popup mode*/, m_table);
                m_select_color->setWindowTitle(tr("Modify Color"));
                m_select_color->setWindowModality(Qt::ApplicationModal);

                connect(m_select_color, &QDialog::finished, this, [this](int result) {
                    if (result == QDialog::Accepted) {
                        auto ctxIdx = m_select_color->property("contextIndex").toModelIndex();
                        QAbstractItemModel *ctxModel = const_cast<QAbstractItemModel *>(ctxIdx.model());
                        setModelDataInternal(QVariant::fromValue(m_select_color->color()), ctxModel, ctxIdx);
                    }
                });
            }
            auto color = idx.data(Qt::EditRole).value<const BrickLink::Color *>();
            auto item = idx.siblingAtColumn(DocumentModel::Description).data(Qt::EditRole)
                    .value<const BrickLink::Item *>();
            m_select_color->setColorAndItem(color, item);

            QRect centerOn;
            if (!fakeeditkey) {
                centerOn = QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()),
                                 option.rect.size());
            }

            m_select_color->setPopupPosition(centerOn);
            m_select_color->setProperty("contextIndex", idx);
            m_select_color->show();
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

    auto *model = static_cast<const DocumentModel *>(idx.model());

    QValidator *valid = nullptr;
    switch (idx.column()) {
    case DocumentModel::PartNo      : valid = new QRegularExpressionValidator(QRegularExpression(uR"([a-zA-Z0-9._-]+)"_qs), nullptr); break;
    case DocumentModel::Sale        : valid = new SmartIntValidator(-1000, 99, 0, nullptr); break;
    case DocumentModel::Quantity    :
    case DocumentModel::QuantityDiff: valid = new SmartIntValidator(-DocumentModel::maxQuantity,
                                                                    DocumentModel::maxQuantity, 0, nullptr); break;
    case DocumentModel::Bulk        : valid = new SmartIntValidator(1, DocumentModel::maxQuantity, 1, nullptr); break;
    case DocumentModel::TierQ1      :
    case DocumentModel::TierQ2      :
    case DocumentModel::TierQ3      : valid = new SmartIntValidator(0, DocumentModel::maxQuantity, 0, nullptr); break;
    case DocumentModel::Price       :
    case DocumentModel::Cost        :
    case DocumentModel::TierP1      :
    case DocumentModel::TierP2      :
    case DocumentModel::TierP3      : valid = new SmartDoubleValidator(0, DocumentModel::maxLocalPrice(model->currencyCode()), 3, 0, nullptr); break;
    case DocumentModel::PriceDiff   : valid = new SmartDoubleValidator(-DocumentModel::maxLocalPrice(model->currencyCode()),
                                                                       DocumentModel::maxLocalPrice(model->currencyCode()), 3, 0, nullptr); break;
    case DocumentModel::TotalWeight :
    case DocumentModel::Weight      : valid = new SmartDoubleValidator(0., 100000., 2, 0, nullptr); break;
    default                         : break;
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

    auto f = static_cast<DocumentModel::Field>(idx.column());

    const auto *item = idx.data(DocumentModel::LotPointerRole).value<const Lot *>();
    if (!item)
        return false;

    if (f == DocumentModel::Picture) {
        if (item->item())
            BrickLink::ToolTip::inst()->show(item->item(), nullptr, event->globalPos(), view);
    } else {
        QVariant v = idx.data(Qt::DisplayRole);
        QString text = displayData(idx, v, false);
        QString tip = displayData(idx, v, true);

        const auto differenceFlags = idx.data(DocumentModel::DifferenceFlagsRole).toULongLong();
        if (differenceFlags & (1ULL << idx.column())) {
            if (tip.isEmpty())
                tip = text;
            if (differenceFlags & differenceWarningMask & (1ULL << idx.column())) {
                tip = tip % u"<br><br>" %
                        tr("This change cannot be applied via BrickLink's Mass-Update mechanism!");
            }
            QVariant vold = idx.data(DocumentModel::BaseDisplayRole);
            QString oldText = displayData(idx, vold, true);

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
    switch (static_cast<DocumentModel::Field>(index.column())) {
    case DocumentModel::Price:
    case DocumentModel::PriceDiff:
    case DocumentModel::Cost:
    case DocumentModel::TierP1:
    case DocumentModel::TierP2:
    case DocumentModel::TierP3:
        v = Currency::toString(v.toDouble());
        break;
    case DocumentModel::TotalWeight:
    case DocumentModel::Weight:
        v = Utility::weightToString(v.toDouble(), Config::inst()->measurementSystem());
        break;
    default:
        break;
    }

    QByteArray n = editor->metaObject()->userProperty().name();
    if (!n.isEmpty()) {
        if (!v.isValid())
            v = QVariant(editor->property(n).metaType(), nullptr);
        editor->setProperty(n, v);
    }
}

void DocumentDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QByteArray n = editor->metaObject()->userProperty().name();
    if (!n.isEmpty()) {
        QVariant v = editor->property(n);

        switch (static_cast<DocumentModel::Field>(index.column())) {
        case DocumentModel::Price:
        case DocumentModel::PriceDiff:
        case DocumentModel::Cost:
        case DocumentModel::TierP1:
        case DocumentModel::TierP2:
        case DocumentModel::TierP3:
            if (!v.toString().startsWith(u'='))
                v = Currency::fromString(v.toString());
            break;
        case DocumentModel::TotalWeight:
        case DocumentModel::Weight:
            if (!v.toString().startsWith(u'='))
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
    bool intCalc = false;
    bool doubleCalc = false;
    int intNumber = 0;
    double doubleNumber = 0;
    char op = 0;

    QString str = value.toString();
    if ((str.length() >= 3) && str.startsWith(u'=')) {
        auto type = model->data(index, Qt::EditRole).userType();
        intCalc = (type == QMetaType::Int);
        doubleCalc = (type == QMetaType::Double);
        op = str.at(1).toLatin1();
        if (QByteArray("+-*/").contains(op)) {
            if (intCalc)
                intNumber = QLocale().toInt(str.mid(2));
            else if (doubleCalc)
                doubleNumber = QLocale().toDouble(str.mid(2));
        }
    }

    auto selection = m_table->selectionModel()->selectedRows();
    for (const auto &s : selection) {
        QVariant newValue = value;

        if (intCalc || doubleCalc) {
            QVariant oldValue = model->data(index.siblingAtRow(s.row()), Qt::EditRole);
            switch (op) {
            case '+': intCalc ? newValue = oldValue.toInt() + intNumber
                        : newValue = oldValue.toDouble() + doubleNumber; break;
            case '-': intCalc ? newValue = oldValue.toInt() - intNumber
                        : newValue = oldValue.toDouble() - doubleNumber; break;
            case '*': intCalc ? newValue = oldValue.toInt() * intNumber
                        : newValue = oldValue.toDouble() * doubleNumber; break;
            case '/': intCalc ? newValue = oldValue.toInt() / (intNumber ? intNumber : 1)
                        : newValue = oldValue.toDouble() / (!qFuzzyIsNull(doubleNumber)
                                                            ? doubleNumber : 1); break;
            default: newValue = oldValue; break;
            }
        }
        model->setData(index.siblingAtRow(s.row()), newValue);
    }
}

QString DocumentDelegate::displayData(const QModelIndex &idx, const QVariant &display, bool toolTip)
{
    QLocale loc;
    static const QString dash = QStringLiteral("-");

    switch (idx.column()) {
    case DocumentModel::Status: {
        if (!toolTip)
            return { };

        QString tip;
        switch (display.value<BrickLink::Status>()) {
        case BrickLink::Status::Exclude: tip = tr("Exclude"); break;
        case BrickLink::Status::Extra  : tip = tr("Extra"); break;
        case BrickLink::Status::Include: tip = tr("Include"); break;
        default                        : break;
        }

        const auto *item = idx.data(DocumentModel::LotPointerRole).value<const Lot *>();
        if (item->counterPart())
            tip = tip % u"<br>(" % tr("Counter part") % u")";
        else if (item->alternateId())
            tip = tip % u"<br>(" % tr("Alternate match id: %1").arg(item->alternateId()) % u")";
        return tip;
    }
    case DocumentModel::Condition: {
        QString str;
        if (!toolTip) {
            str = (display.value<BrickLink::Condition>() == BrickLink::Condition::New)
                    ?  tr("N", "List>Cond>New") : tr("U", "List>Cond>Used");
        } else {
            str = (display.value<BrickLink::Condition>() == BrickLink::Condition::New)
                    ?  tr("New", "ToolTip Cond>New") : tr("Used", "ToolTip Cond>Used");
        }

        const auto *item = idx.data(DocumentModel::LotPointerRole).value<const Lot *>();
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
    case DocumentModel::Retain: {
        if (!toolTip)
            return { };
        return display.toBool() ? tr("Retain") : tr("Do not retain");
    }
    case DocumentModel::Stockroom: {
        if (!toolTip)
            return { };

        QString tip;
        switch (display.value<BrickLink::Stockroom>()) {
        case BrickLink::Stockroom::A   : tip = QString::fromLatin1("A"); break;
        case BrickLink::Stockroom::B   : tip = QString::fromLatin1("B"); break;
        case BrickLink::Stockroom::C   : tip = QString::fromLatin1("C"); break;
        default:
        case BrickLink::Stockroom::None: tip = tr("None", "ToolTip Stockroom>None"); break;
        }
        tip = tr("Stockroom") % u": " % tip;
        return tip;
    }
    case DocumentModel::QuantityOrig:
    case DocumentModel::QuantityDiff:
    case DocumentModel::Quantity:
    case DocumentModel::TierQ1:
    case DocumentModel::TierQ2:
    case DocumentModel::TierQ3: {
        int i = display.toInt();
        return (!i && !toolTip) ? dash : loc.toString(i);
    }
    case DocumentModel::YearReleased: {
        int yearFrom = display.toInt();
        if (!yearFrom && !toolTip) {
            return dash;
        } else {
            const auto *lot = idx.data(DocumentModel::LotPointerRole).value<const Lot *>();
            int yearTo = lot->itemYearLastProduced();
            if (yearTo && (yearTo != yearFrom))
                return QString::number(yearFrom) % u" - " % QString::number(yearTo);
            else
                return QString::number(yearFrom);
        }
    }
    case DocumentModel::LotId: {
        uint i = display.toUInt();
        return (!i && !toolTip) ? dash : QString::number(i);
    }
    case DocumentModel::PriceOrig:
    case DocumentModel::PriceDiff:
    case DocumentModel::Price:
    case DocumentModel::Total:
    case DocumentModel::Cost:
    case DocumentModel::TierP1:
    case DocumentModel::TierP2:
    case DocumentModel::TierP3: {
        double d = display.toDouble();
        return (qFuzzyIsNull(d) && !toolTip) ? dash : Currency::toDisplayString(d);
    }
    case DocumentModel::Bulk: {
        int i = display.toInt();
        return ((i == 1) && !toolTip) ? dash : loc.toString(i);
    }
    case DocumentModel::Sale: {
        int i = display.toInt();
        return (!i && !toolTip) ? dash : loc.toString(i) % u'%';
    }
    case DocumentModel::Weight:
    case DocumentModel::TotalWeight: {
        double d = display.toDouble();
        return (qFuzzyIsNull(d) && !toolTip) ? dash : Utility::weightToString
                                               (d, Config::inst()->measurementSystem(), true, true);
    }
    case DocumentModel::DateAdded:
    case DocumentModel::DateLastSold: {
        QDateTime dt = display.toDateTime();
        if (dt.isValid() && (dt.time() == QTime(0, 0))) {
            return loc.toString(dt.date(), QLocale::ShortFormat);
            //return HumanReadableTimeDelta::toString(QDate::currentDate().startOfDay(), dt);
        } else {
            return dt.isValid() ? loc.toString(dt.toLocalTime(), QLocale::ShortFormat) : dash;
            //return HumanReadableTimeDelta::toString(QDateTime::currentDateTime(), dt);
        }
    }
    default:
        return display.toString();
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

#include "moc_documentdelegate.cpp"
