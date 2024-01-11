// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QBuffer>
#include <QtCore/QStringBuilder>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QHelpEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeView>

#include "bricklink/category.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/picture.h"
#include "bricklink/item.h"
#include "bricklink/delegate.h"
#include "bricklink/model.h"
#include "common/eventfilter.h"
#include "utility/utility.h"


namespace BrickLink {

/////////////////////////////////////////////////////////////
// ITEMDELEGATE
/////////////////////////////////////////////////////////////


ItemDelegate::ItemDelegate(Options options, QAbstractItemView *parent)
    : BetterItemDelegate(options, parent)
{
    if (options & Pinnable)
        setPinnedRole(BrickLink::PinnedRole);
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    BetterItemDelegate::extendedPaint(painter, option, index, [this, painter, option, index]() {
        bool firstColumnImageOnly = (m_options & FirstColumnImageOnly) && (index.column() == 0);

        if (firstColumnImageOnly) {
            if (auto *item = index.data(ItemPointerRole).value<const Item *>()) {
                auto *color = index.data(ColorPointerRole).value<const Color *>();
                if (!color)
                    color = item->defaultColor();
                QImage image;

                Picture *pic = core()->pictureCache()->picture(item, color);
                if (pic && pic->isValid())
                    image = pic->image();
                else
                    image = core()->noImage(option.rect.size());

                painter->fillRect(option.rect, Qt::white);
                if (!image.isNull()) {
                    QSizeF s = image.size().scaled(option.rect.size(), Qt::KeepAspectRatio);
                    QPointF p = option.rect.center() - QRectF({ }, s).center() + QPointF(.5, .5);
                    // scale while drawing, so we don't have to deal with hi-dpi calculations
                    painter->drawImage({ p, s }, image, image.rect());
                }
            }
        }
    });
}

bool ItemDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::ToolTip && index.isValid()) {
        auto item = index.data(ItemPointerRole).value<const Item *>();
        auto color = index.data(ColorPointerRole).value<const Color *>();
        auto category = index.data(CategoryPointerRole).value<const Category *>();

        if (item || color || category) {
            ToolTip::inst()->show(item, color, category, event->globalPos(), view);
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}


/////////////////////////////////////////////////////////////
// ITEMTHUMBSDELEGATE
/////////////////////////////////////////////////////////////


ItemThumbsDelegate::ItemThumbsDelegate(double initialZoom, QAbstractItemView *view)
    : ItemDelegate(ItemDelegate::AlwaysShowSelection
                   | ItemDelegate::FirstColumnImageOnly, view)
    , m_zoom(initialZoom)
    , m_view(view)
{
    new EventFilter(view->viewport(), { QEvent::Wheel, QEvent::NativeGesture },
                    [this](QObject *o, QEvent *e) -> EventFilter::Result {
        if ((e->type() == QEvent::Wheel)
                && m_view && (o == m_view->viewport())) {
            const auto *we = static_cast<QWheelEvent *>(e);
            if (we->modifiers() & Qt::ControlModifier) {
                double z = std::pow(1.001, we->angleDelta().y());
                setZoomFactor(m_zoom * z);
                e->accept();
                return EventFilter::StopEventProcessing;
            }
        } else if ((e->type() == QEvent::NativeGesture)
                   && m_view && (o == m_view->viewport())) {
            const auto *nge = static_cast<QNativeGestureEvent *>(e);
            if (nge->gestureType() == Qt::ZoomNativeGesture) {
                double z = 1 + nge->value();
                setZoomFactor(m_zoom * z);
                e->accept();
                return EventFilter::StopEventProcessing;
            }
        }
        return EventFilter::ContinueEventProcessing;
    });
}

double ItemThumbsDelegate::zoomFactor() const
{
    return m_zoom;
}

void ItemThumbsDelegate::setZoomFactor(double zoom)
{
    zoom = qBound(.5, zoom, 5.);

    if (m_view && !qFuzzyCompare(zoom, m_zoom)) {
        m_zoom = zoom;

        emit zoomFactorChanged(m_zoom);
        emit sizeHintChanged(m_view->model()->index(0, 0));

        if (auto tv = qobject_cast<QTreeView *>(m_view))
            tv->resizeColumnToContents(0);
    }
}

QSize ItemThumbsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if ((index.column() == 0) && !index.data(IsSectionHeaderRole).toBool()) {
        const auto *item = qvariant_cast<const Item *>(index.data(ItemPointerRole));
        return (item ? item->itemType()->pictureSize() : QSize(80, 60)) * m_zoom;
    } else {
        return ItemDelegate::sizeHint(option, index);
    }
}


/////////////////////////////////////////////////////////////
// TOOLTIP
/////////////////////////////////////////////////////////////


ToolTip *ToolTip::s_inst = nullptr;

ToolTip *ToolTip::inst()
{
    if (!s_inst) {
        s_inst = new ToolTip();
        connect(core()->pictureCache(), &BrickLink::PictureCache::pictureUpdated,
                s_inst, &ToolTip::pictureUpdated);

        // animated tooltips do not work for us, because the tooltip widget is not "visible"
        // until the animation is finished, effectively breaking pictureUpdated() below
        QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);

#if defined(Q_OS_WINDOWS)
        // on Windows we can at least use the fade animation, as this is done via the window
        // opacity (see qeffects.cpp, QAlphaWidget::run).
        QApplication::setEffectEnabled(Qt::UI_FadeTooltip, true);
#endif
    }
    return s_inst;
}

bool ToolTip::show(const Item *item, const Color *color, const Category *category,
                   const QPoint &globalPos, QWidget *parent)
{
    QString tt;

    if (item) {
        if (Picture *pic = core()->pictureCache()->picture(item, nullptr, true)) {
            m_tooltip_pic = ((pic->updateStatus() == UpdateStatus::Updating)
                             || (pic->updateStatus() == UpdateStatus::Loading)) ? pic : nullptr;

            tt = createItemToolTip(pic->item(), pic);
        }
    } else if (color) {
        tt = createColorToolTip(color);
    } else if (category) {
        tt = createCategoryToolTip(category);
    }

    if (!tt.isEmpty()) {
        QToolTip::showText(globalPos, tt, parent);
        return true;
    }
    return false;
}

QString ToolTip::createItemToolTip(const Item *item, Picture *pic) const
{
    static const QString str = uR"(<table class="tooltip_picture"><tr><td>%2</td><td align="right"><i>&nbsp;%4</i></td></tr><tr><td colspan="2"><b>%3</b></td></tr><tr><td colspan="2">%1</td></tr></table>)"_qs;
    static const QString img_left = uR"(<center><img src="data:image/png;base64,%1" width="%2" height="%3"/></center>)"_qs;
    QString note_left = u"<i>" + ItemDelegate::tr("[Image is loading]") + u"</i>";
    QString yearStr = yearSpan(item->yearReleased(), item->yearLastProduced());
    QString id = QString::fromLatin1(item->id());

    QColor color = qApp->palette().color(QPalette::Highlight);
    id = id + uR"(&nbsp;&nbsp;<i><font color=")" + Utility::textColor(color).name()
         + uR"(" style="background-color: )" + color.name() + uR"(;">&nbsp;)"
         + item->itemType()->name() + uR"(&nbsp;</font></i>)";

    if (pic && ((pic->updateStatus() == UpdateStatus::Updating)
                || (pic->updateStatus() == UpdateStatus::Loading))) {
        return str.arg(note_left, id, item->name(), yearStr);
    } else {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        const QImage img = pic->image();
        img.save(&buffer, "PNG");

        return str.arg(img_left.arg(QString::fromLatin1(ba.toBase64())).arg(img.width()).arg(img.height()),
                       id, item->name(), yearStr);
    }
}

QString ToolTip::createColorToolTip(const Color *color) const
{
    QString str;

    if (color->id() > 0) {
        static const QString tpl = uR"(<table width="100%" border="0" bgcolor="%1"><tr><td><br><br></td></tr></table><br><b>%2</b> (%3)<br>BrickLink id: %4)"_qs;
        str = str + tpl.arg(color->color().name(), color->name(), color->color().name(),
                            QString::number(color->id()));

        if (color->ldrawId() != -1)
            str = str + u", LDraw id: " + QString::number(color->ldrawId());
    } else {
        str = u"<b>" + color->name() + u"</b>";
    }
    QString yearStr = yearSpan(color->yearReleased(), color->yearLastProduced());
    if (!yearStr.isEmpty())
        str = str + u"<br><i>" + yearStr + u"</i>";
    return str;
}

QString ToolTip::createCategoryToolTip(const Category *category) const
{
    if (category == CategoryModel::AllCategories)
        return { };

    QString str = u"<b>%1</b><br>BrickLink id: %2"_qs
                      .arg(category->name()).arg(category->id());
    QString yearStr = yearSpan(category->yearReleased(), category->yearLastProduced());
    if (!yearStr.isEmpty())
        str = str + u"<br><i>" + yearStr + u"</i>";
    return str;
}

QString ToolTip::yearSpan(int from, int to)
{
    QString yearStr;
    if (from)
        yearStr = QString::number(from);
    if (to && (to != from))
        yearStr = yearStr + u'-' + QString::number(to);
    return yearStr;
}

void ToolTip::pictureUpdated(Picture *pic)
{
    if (!pic || pic != m_tooltip_pic)
        return;

    if ((pic->updateStatus() != UpdateStatus::Updating)
            && (pic->updateStatus() != UpdateStatus::Loading)) {
        m_tooltip_pic = nullptr;
    }

    if (QToolTip::isVisible() && QToolTip::text().startsWith(uR"(<table class="tooltip_picture")")) {
        const auto tlwidgets = QApplication::topLevelWidgets();
        for (QWidget *w : tlwidgets) {
            if (w->inherits("QTipLabel")) {
                qobject_cast<QLabel *>(w)->clear();
                qobject_cast<QLabel *>(w)->setText(createItemToolTip(pic->item(), pic));

                QRect r(w->pos(), w->sizeHint());
                QRect desktop = w->window()->windowHandle()->screen()->availableGeometry();

                r.translate(r.right() > desktop.right() ? desktop.right() - r.right() : 0,
                            r.bottom() > desktop.bottom() ? desktop.bottom() - r.bottom() : 0);
                w->setGeometry(r);
                break;
            }
        }
    }
}

} // namespace BrickLink

#include "moc_delegate.cpp"
