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
#include <QLabel>
#include <QPalette>
#include <QMenu>
#include <QLayout>
#include <QApplication>
#include <QAction>
#include <QDesktopServices>
#include <QHelpEvent>
#include <QToolButton>
#include <QStyle>
#include <QClipboard>
#include <QFileDialog>
#include <QStringBuilder>

#include "bricklink/color.h"
#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/item.h"
#include "bricklink/picture.h"
#include "common/config.h"
#include "ldraw/ldraw.h"
#include "ldraw/renderwidget.h"
#include "utility/utility.h"
#include "picturewidget.h"


PictureWidget::PictureWidget(QWidget *parent)
    : QFrame(parent)
{
    setBackgroundRole(QPalette::Base);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setAutoFillBackground(true);

    w_text = new QLabel();
    w_text->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    w_text->setWordWrap(true);
    w_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    w_text->setContextMenuPolicy(Qt::DefaultContextMenu);
    QPalette pal = w_text->palette();
    pal.setColor(QPalette::Base, Qt::red);
    w_text->setPalette(pal);
    w_text->installEventFilter(this);

    w_image = new QLabel();
    w_image->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    w_image->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    w_image->setMinimumSize(BrickLink::core()->standardPictureSize() * 2);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(w_text);
    layout->addWidget(w_image, 10);
    layout->setContentsMargins(2, 6, 2, 2);

#if !defined(QT_NO_OPENGL)
#if defined(Q_OS_MACOS)
    w_ldrawWin = new LDraw::RenderWindow();
    w_ldraw = QWidget::createWindowContainer(w_ldrawWin, this);
#else
    w_ldraw = new LDraw::RenderWidget(this);
#endif
    w_ldraw->hide();

    w_2d = new QToolButton();
    w_2d->setText("2D"_l1);
    w_2d->setAutoRaise(true);
    w_2d->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(w_2d, &QToolButton::clicked, this, [this]() {
        m_prefer3D = false;
        redraw();
    });

    w_3d = new QToolButton();
    w_3d->setText("3D"_l1);
    w_3d->setAutoRaise(true);
    w_3d->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(w_3d, &QToolButton::clicked, this, [this]() {
        m_prefer3D = true;
        redraw();
    });

    auto font = w_2d->font();
    font.setBold(true);
    w_2d->setFont(font);
    w_3d->setFont(font);

    m_playIcon = QIcon::fromTheme("media-playback-start"_l1);
    m_pauseIcon = QIcon::fromTheme("media-playback-stop"_l1);

    w_playPause = new QToolButton();
    w_playPause->setAutoRaise(true);
    w_playPause->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    auto toggleAnimation = [this]() {
        m_animationActive = !m_animationActive;
        w_playPause->setIcon(m_animationActive ? m_pauseIcon : m_playIcon);
        if (m_animationActive)
            w_ldrawWin->startAnimation();
        else
            w_ldrawWin->resetCamera();
    };
    connect(w_playPause, &QToolButton::clicked,
            w_ldraw, toggleAnimation);
    m_animationActive = true;
    toggleAnimation();
    updateButtons();

    if (w_ldraw)
        layout->addWidget(w_ldraw, 10);

    auto buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(w_2d, 10);
    buttons->addWidget(w_3d, 10);
    buttons->addWidget(w_playPause, 10);
    layout->addLayout(buttons);
#endif

    m_blCatalog = new QAction(QIcon::fromTheme("bricklink-catalog"_l1), { }, this);
    connect(m_blCatalog, &QAction::triggered, this, [this]() {
        BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, m_item, m_color);
    });

    m_blPriceGuide = new QAction(QIcon::fromTheme("bricklink-priceguide"_l1), { }, this);
    connect(m_blPriceGuide, &QAction::triggered, this, [this]() {
        BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, m_item, m_color);
    });

    m_blLotsForSale = new QAction(QIcon::fromTheme("bricklink-lotsforsale"_l1), { }, this);
    connect(m_blLotsForSale, &QAction::triggered, this, [this]() {
        BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, m_item, m_color);
    });
    m_reload = new QAction(QIcon::fromTheme("view-refresh"_l1), { }, this);
    connect(m_reload, &QAction::triggered, this, [this]() {
        if (m_pic) {
            m_pic->update(true);
            redraw();
        }
    });

    m_copyImage = new QAction(QIcon::fromTheme("edit-copy"_l1), { }, this);
    connect(m_copyImage, &QAction::triggered, this, [this]() {
        if (!m_image.isNull()) {
            auto clip = QGuiApplication::clipboard();
            clip->setImage(m_image);
        }
    });

    m_saveImageAs = new QAction(QIcon::fromTheme("document-save"_l1), { }, this);
    connect(m_saveImageAs, &QAction::triggered, this, [this]() {
        if (!m_image.isNull()) {
            QStringList filters;
            filters << tr("PNG Image") % " (*.png)"_l1;

            QString fn = QFileDialog::getSaveFileName(this, tr("Save image as"),
                                                      Config::inst()->lastDirectory(),
                                                      filters.join(";;"_l1));
            if (fn.isEmpty())
                return;
            Config::inst()->setLastDirectory(QFileInfo(fn).absolutePath());

            if (fn.right(4) != ".png"_l1)
                fn += ".png"_l1;

            m_image.save(fn, "PNG");
        }
    });

    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            this, &PictureWidget::pictureWasUpdated);

    paletteChange();
    languageChange();
    redraw();
}

void PictureWidget::languageChange()
{
    m_reload->setText(tr("Update"));
    m_copyImage->setText(tr("Copy image"));
    m_saveImageAs->setText(tr("Save image as..."));
    m_blCatalog->setText(tr("Show BrickLink Catalog Info..."));
    m_blPriceGuide->setText(tr("Show BrickLink Price Guide Info..."));
    m_blLotsForSale->setText(tr("Show Lots for Sale on BrickLink..."));
}

void PictureWidget::paletteChange()
{
    if (w_ldrawWin)
        w_ldrawWin->setClearColor(palette().color(backgroundRole()));
}

void PictureWidget::resizeEvent(QResizeEvent *e)
{
    QFrame::resizeEvent(e);
    redraw();
}

PictureWidget::~PictureWidget()
{
    if (m_pic)
        m_pic->release();
    if (m_part)
        m_part->release();
}

void PictureWidget::setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color)
{
    if ((item == m_item) && (color == m_color))
        return;

    m_item = item;
    m_color = color;
    m_image = { };

    if (m_pic)
        m_pic->release();
    if (!item) {
        m_pic = nullptr;
    } else {
        m_pic = item->itemType()->hasColors() ? BrickLink::core()->picture(item, color, true)
                                              : BrickLink::core()->largePicture(item, true);
    }
    if (m_pic) {
        m_pic->addRef();
        if (m_pic->isValid())
            m_image = m_pic->image();
    }

    if (m_part)
        m_part->release();
    m_part = (LDraw::core() && item) ? LDraw::core()->partFromId(item->id()) : nullptr;
    if (m_part) {
        m_part->addRef();

        m_colorId = -1;
        if (color)
            m_colorId = color->ldrawId();
        if (m_colorId < 0)
            m_colorId = 7; // light gray
    }

    m_blCatalog->setVisible(item);
    m_blPriceGuide->setVisible(item && color);
    m_blLotsForSale->setVisible(item && color);
    redraw();
}

void PictureWidget::pictureWasUpdated(BrickLink::Picture *pic)
{
    if (pic == m_pic) {
        if (pic->isValid())
            m_image = pic->image();
        redraw();
    }
}

void PictureWidget::redraw()
{
    w_image->setPixmap({ });

    if (m_item) {
        QString cs;
        if (m_color && m_color->id()) {
            QColor color = m_color->color();
            cs = R"(<b><font color=")"_l1 % Utility::textColor(color).name() %
                    R"(" style="background-color: )"_l1 % color.name() % R"(;">&nbsp;)"_l1 %
                    m_color->name() % R"(&nbsp;</font></b>&nbsp;&nbsp;)"_l1;
        }

        w_text->setText("<center><b>"_l1 %
                        QLatin1String(m_item->id()) %
                        "</b>&nbsp; "_l1 % cs %
                        m_item->name() %
                        "</center>"_l1);
    } else {
        w_text->setText({ });
    }

    if (m_pic && (m_pic->updateStatus() == BrickLink::UpdateStatus::Updating)) {
        w_image->setText("<center><i>"_l1 +
                         tr("Please wait... updating") +
                         "</i></center>"_l1);
    } else if (m_pic) {
        bool hasImage = !m_image.isNull();
        auto dpr = devicePixelRatioF();
        QPixmap p;
        QSize pSize;
        QSize displaySize = w_image->contentsRect().size();

        if (hasImage) {
            p = QPixmap::fromImage(m_image, Qt::NoFormatConversion);
            pSize = p.size();
        } else {
            pSize = BrickLink::core()->standardPictureSize();
        }
        QSize s = pSize.scaled(displaySize, Qt::KeepAspectRatio).boundedTo(pSize * 2) * dpr;

        if (hasImage)
            p = p.scaled(s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        else
            p = QPixmap::fromImage(BrickLink::core()->noImage(s));

        p.setDevicePixelRatio(dpr);
        w_image->setPixmap(p);

    } else {
        w_image->setText({ });
    }

    bool show3D = canShow3D() && prefer3D();

    if (show3D) {
#if !defined(QT_NO_OPENGL)
        w_image->hide();

        w_ldraw->show();
        w_ldrawWin->setPartAndColor(m_part, m_colorId);
        if (m_animationActive)
            w_ldrawWin->startAnimation();
#endif
    } else {
#if !defined(QT_NO_OPENGL)
        if (w_ldraw) {
            w_ldrawWin->setPartAndColor(nullptr, -1);
            w_ldraw->hide();
            w_ldrawWin->stopAnimation();
        }
#endif
        w_image->show();
    }
    m_reload->setVisible(!show3D);
    m_copyImage->setVisible(!show3D);
    m_saveImageAs->setVisible(!show3D);

    updateButtons();
}

void PictureWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    else if (e->type() == QEvent::PaletteChange)
        paletteChange();
    QFrame::changeEvent(e);
}

bool PictureWidget::event(QEvent *e)
{
    if (m_item && e && e->type() == QEvent::ToolTip) {
        return BrickLink::ToolTip::inst()->show(m_item, m_color,
                                                static_cast<QHelpEvent *>(e)->globalPos(), this);
    }
    return QFrame::event(e);
}

bool PictureWidget::eventFilter(QObject *o, QEvent *e)
{
    if ((o == w_text) && (e->type() == QEvent::ContextMenu)) {
        w_text->setContextMenuPolicy(w_text->hasSelectedText() ? Qt::DefaultContextMenu
                                                               : Qt::NoContextMenu);
    } else if ((o == w_text) && (e->type() == QEvent::Resize)) {
        // workaround for layouts breaking, if a rich-text label with word-wrap has
        // more than one line
        QMetaObject::invokeMethod(w_text, [this]() {
            w_text->setMinimumHeight(0);
            int h = w_text->heightForWidth(w_text->width());
            if (h > 0)
                w_text->setMinimumHeight(h);
        }, Qt::QueuedConnection);
    }
    return QFrame::eventFilter(o, e);
}

void PictureWidget::contextMenuEvent(QContextMenuEvent *e)
{
    if (m_item) {
        QMenu *m = new QMenu(this);
        m->setAttribute(Qt::WA_DeleteOnClose);
        m->addAction(m_blCatalog);
        m->addAction(m_blPriceGuide);
        m->addAction(m_blLotsForSale);
        m->addSeparator();
        m->addAction(m_reload);
        m->addSeparator();
        m->addAction(m_copyImage);
        m->addAction(m_saveImageAs);
        m->popup(e->globalPos());
    }
    e->accept();
}

void PictureWidget::updateButtons()
{
    bool is3d = isShowing3D();

    w_2d->setEnabled(is3d);
    w_3d->setEnabled(!is3d && canShow3D());
    w_playPause->setEnabled(is3d);
}

bool PictureWidget::canShow3D() const
{
#if !defined(QT_NO_OPENGL)
    return m_part && w_ldraw;
#else
    return false;
#endif
}

bool PictureWidget::prefer3D() const
{
    return m_prefer3D;
}

bool PictureWidget::isShowing3D() const
{
#if !defined(QT_NO_OPENGL)
    return canShow3D() && w_ldraw->isVisible();
#else
    return false;
#endif
}

#include "moc_picturewidget.cpp"
