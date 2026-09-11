// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QLabel>
#include <QPalette>
#include <QMenu>
#include <QLayout>
#include <QVector>
#include <QApplication>
#include <QAction>
#include <QDesktopServices>
#include <QHelpEvent>
#include <QToolButton>
#include <QStyle>
#include <QClipboard>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QScopeGuard>
#include <QStackedLayout>

#include <QCoro/QCoroSignal>

#include "bricklink/color.h"
#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/item.h"
#include "bricklink/picture.h"
#include "common/application.h"
#include "common/config.h"
#include "common/eventfilter.h"
#include "common/uihelpers.h"
#include "ldraw/library.h"
#include "ldraw/partimagerenderer.h"
#include "ldraw/renderwidget.h"
#include "picturewidget.h"
#include "renderimagedialog.h"
#include "rendersettingsdialog.h"


PictureWidget::PictureWidget(QWidget *parent)
    : QFrame(parent)
{
    setBackgroundRole(QPalette::Base);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setAutoFillBackground(true);

    w_text = new QLabel(this);
    w_text->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    w_text->setWordWrap(true);
    w_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    w_text->setContextMenuPolicy(Qt::DefaultContextMenu);

    new EventFilter(w_text, { QEvent::ContextMenu }, [this](QObject *, QEvent *) {
        w_text->setContextMenuPolicy(w_text->hasSelectedText() ? Qt::DefaultContextMenu
                                                               : Qt::NoContextMenu);
        return EventFilter::ContinueEventProcessing;
    });

    w_image = new QLabel(this);
    w_image->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    w_image->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    w_image->setMinimumSize(2 * BrickLink::core()->standardPictureSize());
    w_image->setAutoFillBackground(true);
    w_image->setWordWrap(true);

    w_ldraw = new LDraw::RenderWidget(Application::inst()->qmlEngine(), this);

    w_stackLayout = new QStackedLayout();
    w_stackLayout->setStackingMode(QStackedLayout::StackAll);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(w_text);
    layout->addLayout(w_stackLayout, 10);
    w_stackLayout->addWidget(w_image);
    w_stackLayout->addWidget(w_ldraw);
    layout->setContentsMargins(2, 6, 2, 2);

    w_2d = new QToolButton(this);
    w_2d->setText(u"2D"_qs);
    w_2d->setProperty("iconScaling", true);
    w_2d->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(w_2d, &QToolButton::clicked, this, [this]() {
        setPrefer3D(false);
    });

    w_3d = new QToolButton(this);
    w_3d->setText(u"3D"_qs);
    w_3d->setProperty("iconScaling", true);
    w_3d->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(w_3d, &QToolButton::clicked, this, [this]() {
        setPrefer3D(true);
    });

    auto font = w_2d->font();
    font.setBold(true);
    w_2d->setFont(font);
    w_3d->setFont(font);

    m_rescaleIcon = QIcon::fromTheme(u"zoom-fit-best"_qs);
    m_reloadIcon = QIcon::fromTheme(u"view-refresh"_qs);

    w_reloadRescale = new QToolButton(this);
    w_reloadRescale->setProperty("iconScaling", true);
    w_reloadRescale->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    w_reloadRescale->setIcon(m_reloadIcon);
    connect(w_reloadRescale, &QToolButton::clicked,
            this, [this]() {
        if (w_stackLayout->currentWidget() == w_ldraw) {
            w_ldraw->resetCamera();
        } else if (m_pic) {
            BrickLink::core()->pictureCache()->updatePicture(m_pic, true);
            m_currentImageSize = { };
            showImage();
        }
    });

    auto buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(w_2d, 10);
    buttons->addWidget(w_3d, 10);
    buttons->addWidget(w_reloadRescale, 10);
    layout->addLayout(buttons);

    m_blCatalog = new QAction(QIcon::fromTheme(u"bricklink-catalog"_qs), { }, this);
    connect(m_blCatalog, &QAction::triggered, this, [this]() {
        Application::openUrl(BrickLink::Core::urlForCatalogInfo(m_item, m_color));
    });

    m_blPriceGuide = new QAction(QIcon::fromTheme(u"bricklink-priceguide"_qs), { }, this);
    connect(m_blPriceGuide, &QAction::triggered, this, [this]() {
        Application::openUrl(BrickLink::Core::urlForPriceGuideInfo(m_item, m_color));
    });

    m_blLotsForSale = new QAction(QIcon::fromTheme(u"bricklink-lotsforsale"_qs), { }, this);
    connect(m_blLotsForSale, &QAction::triggered, this, [this]() {
        Application::openUrl(BrickLink::Core::urlForLotsForSale(m_item, m_color));
    });
    m_renderSettings = new QAction({ }, this);
    connect(m_renderSettings, &QAction::triggered, this, []() {
        RenderSettingsDialog::inst()->show();
    });

    m_screenshot3D = new QAction(QIcon::fromTheme(u"camera-photo"_qs), { }, this);
    connect(m_screenshot3D, &QAction::triggered, this, &PictureWidget::screenshot3D);

    m_copyImage = new QAction(QIcon::fromTheme(u"edit-copy"_qs), { }, this);
    connect(m_copyImage, &QAction::triggered, this, [this]() {
        if (!m_image.isNull()) // never replace the clipboard's contents with nothing
            QGuiApplication::clipboard()->setImage(m_image);
    });

    m_saveImageAs = new QAction(QIcon::fromTheme(u"document-save"_qs), { }, this);
    connect(m_saveImageAs, &QAction::triggered, this, [this]() {
        if (!m_image.isNull())
            saveImage(m_image);
    });

    connect(BrickLink::core()->pictureCache(), &BrickLink::PictureCache::pictureUpdated,
            this, [this](const BrickLink::PictureRef &pic) {
        if (pic == m_pic) {
            if (pic->isValid())
                m_image = pic->image();
            m_currentImageSize = { };
            showImage();
        }
    });

    connect(LDraw::library(), &LDraw::Library::libraryAboutToBeReset,
            this, [this]() {
        w_ldraw->clear();
    });

    connect(w_stackLayout, &QStackedLayout::currentChanged, this, &PictureWidget::stackSwitch);

    connect(w_ldraw, &LDraw::RenderWidget::canRenderChanged,
            this, [this](bool b) {
        if (!b)
            w_stackLayout->setCurrentWidget(w_image);
        else if (prefer3D() && m_supports3D) {
            w_stackLayout->setCurrentWidget(w_ldraw);
        }
        w_3d->setEnabled(b);
    });

    new EventFilter(w_image, { QEvent::Resize, QEvent::Show }, [this](QObject *, QEvent *) {
        showImage();
        return EventFilter::ContinueEventProcessing;
    });

    paletteChange();
    languageChange();

    m_supports3D = w_ldraw->isGPUSupported();
    if (!m_supports3D)
        w_3d->setVisible(false);

    setPrefer3D(m_prefer3D);
    stackSwitch();
}

void PictureWidget::stackSwitch()
{
    bool is3D = (w_stackLayout->currentWidget() == w_ldraw);
    w_reloadRescale->setIcon(is3D ? m_rescaleIcon : m_reloadIcon);
    w_reloadRescale->setToolTip(is3D ? tr("Center view") : tr("Update"));
    m_renderSettings->setVisible(is3D);
    m_screenshot3D->setVisible(is3D && LDraw::canRenderPartImages);
    m_copyImage->setVisible(!is3D);
    m_saveImageAs->setVisible(!is3D);

    if (!is3D) {
        w_ldraw->stopAnimation();
        showImage();
    }

    auto markText = [](const char *text, bool marked) {
        QString str = QString::fromLatin1(text);
        if (marked) {
            str.prepend(u'\x2308');
            str.append(u'\x230b');
        }
        return str;
    };

    w_2d->setText(markText("2D", !is3D));
    w_3d->setText(markText("3D", is3D));
}

void PictureWidget::languageChange()
{
    m_renderSettings->setText(tr("3D Render Settings..."));
    m_screenshot3D->setText(tr("3D Screenshot..."));
    m_copyImage->setText(tr("Copy Image"));
    m_saveImageAs->setText(tr("Save Image as..."));
    m_blCatalog->setText(tr("Show BrickLink Catalog Info..."));
    m_blPriceGuide->setText(tr("Show BrickLink Price Guide Info..."));
    m_blLotsForSale->setText(tr("Show Lots for Sale on BrickLink..."));
}

void PictureWidget::paletteChange()
{
    if (w_image) {
        auto pal = w_image->palette();
        pal.setColor(w_image->backgroundRole(), Qt::white);
        w_image->setPalette(pal);
    }
}

PictureWidget::~PictureWidget() = default;

void PictureWidget::setItemAndColor(const BrickLink::Item *item, const BrickLink::Color *color)
{
    if ((item == m_item) && (color == m_color))
        return;

    m_item = item;
    m_color = color;
    m_image = { };
    m_currentImageSize = { };

    m_pic = item ? BrickLink::core()->pictureCache()->picture(item, color, true)
                 : BrickLink::PictureRef { };
    if (m_pic && m_pic->isValid())
        m_image = m_pic->image();

    m_blCatalog->setVisible(item);
    m_blPriceGuide->setVisible(item && color);
    m_blLotsForSale->setVisible(item && color);

    QString s = BrickLink::core()->itemHtmlDescription(m_item, m_color,
                                                       palette().color(QPalette::Highlight));
    w_text->setText(u"<center>" + s + u"</center>");
    w_image->setPixmap({ });
    m_currentImageSize = { }; // showImage() caches the pixmap's size: clearing one clears both
    w_reloadRescale->setEnabled(m_item);

    // has to come last: a 2D-only item makes this emit canRender(false) synchronously, which
    // switches the stack over to w_image and repaints it - anything still pending above would
    // clear that pixmap again. Only canRender(true) is asynchronous.
    w_ldraw->setItemAndColor(item, color);
    w_3d->setEnabled(w_ldraw->canRender());

    if (w_stackLayout->currentWidget() == w_image)
        showImage();
}

QCoro::Task<> PictureWidget::screenshot3D()
{
    const auto *item = m_item;
    const auto *color = m_color;
    if (!item)
        co_return;

    LDraw::PartImageOptions opts;
    // a screenshot is a still of what is on screen, so it keeps the orientation the user
    // dragged to. Zoom is not carried over - the border replaces them.
    opts.rotation = w_ldraw->modelRotation();
    opts.renderLines = w_ldraw->renderLines();

    QPointer<PictureWidget> that = this;
    RenderImageDialog::Action action = { };
    {
        RenderImageDialog dlg(this);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.show();
        if (co_await qCoro(&dlg, &QDialog::finished) != QDialog::Accepted)
            co_return;
        if (!that) // safeguard
            co_return;

        action = dlg.action();
        opts.size = dlg.imageSize();
        opts.background = dlg.background();
        opts.margin = dlg.border();
    }

    QImage img = co_await LDraw::renderPartImage(item, color, opts);
    if (img.isNull() || !that)
        co_return;

    if (action == RenderImageDialog::Action::CopyToClipboard)
        QGuiApplication::clipboard()->setImage(img);
    else
        co_await saveImage(img);
}

QCoro::Task<> PictureWidget::saveImage(QImage img)
{
    if (img.isNull())
        co_return;

    QString suggestion;
    if (m_item) {
        suggestion = QString::fromLatin1(m_item->id());
        if (m_color)
            suggestion = suggestion + u' ' + m_color->name();
    }

    // the filter list cannot be a braced-init-list in the co_await expression: gcc 13 ICEs on
    // braced-init temporaries in a co_await operand (build_special_member_call, cp/call.cc)
    const QList<QPair<QString, QStringList>> filters = { { tr("PNG Image"), { u"png"_qs } } };

    const auto fileName = co_await UIHelpers::getSaveFileName(
                { }, filters, tr("Save Image"), suggestion);
    if (!fileName)
        co_return;

    QString fn = *fileName;
    if (fn.right(4) != u".png")
        fn = fn + u".png";
    if (!img.save(fn, "PNG"))
        co_await UIHelpers::warning(tr("Failed to save the image to %1.").arg(fn));
}

bool PictureWidget::prefer3D() const
{
    return m_prefer3D;
}

void PictureWidget::setPrefer3D(bool b)
{
    if (m_prefer3D != b)
        m_prefer3D = b;

    w_stackLayout->setCurrentWidget((b && m_supports3D) ? static_cast<QWidget *>(w_ldraw) : w_image);
}

void PictureWidget::showImage()
{
    if (w_stackLayout->currentWidget() != w_image)
        return;

    if (m_pic && ((m_pic->updateStatus() == BrickLink::UpdateStatus::Updating) ||
                  (m_pic->updateStatus() == BrickLink::UpdateStatus::Loading))) {
        w_image->setText(u"<center><i>" + tr("Please wait... updating") + u"</i></center>");
    } else if (m_pic && w_image->isVisible()) {
        bool hasImage = !m_image.isNull();
        QPixmap p;
        QSize pSize;
        QSize displaySize = w_image->contentsRect().size();

        if (hasImage) {
            if (displaySize == m_currentImageSize)
                return;

            pSize = m_image.size();
        } else {
            pSize = 4 * BrickLink::core()->standardPictureSize();
        }
        m_currentImageSize = hasImage ? displaySize : QSize { };

        QSize sz = pSize.scaled(displaySize, Qt::KeepAspectRatio).boundedTo(pSize * 2);

        if (hasImage) {
            auto dpr = devicePixelRatioF();
            QImage img = m_image.scaled(sz * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p = QPixmap::fromImage(std::move(img));
            p.setDevicePixelRatio(dpr);
        } else {
            p = QPixmap::fromImage(BrickLink::core()->noImage(sz));
        }
        w_image->setPixmap(p);

    } else {
        w_image->setText({ });
    }
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
        return BrickLink::ToolTip::inst()->show(m_item, m_color, nullptr,
                                                static_cast<QHelpEvent *>(e)->globalPos(), this);
    }
    return QFrame::event(e);
}

void PictureWidget::contextMenuEvent(QContextMenuEvent *e)
{
    if (m_item) {
        if (!m_contextMenu) {
            m_contextMenu = new QMenu(this);
            m_contextMenu->addAction(m_blCatalog);
            m_contextMenu->addAction(m_blPriceGuide);
            m_contextMenu->addAction(m_blLotsForSale);
            m_contextMenu->addSeparator();
            m_contextMenu->addAction(m_renderSettings);
            m_contextMenu->addSeparator();
            m_contextMenu->addAction(m_screenshot3D);
            m_contextMenu->addAction(m_copyImage);
            m_contextMenu->addAction(m_saveImageAs);
        }
        m_contextMenu->popup(e->globalPos());
    }
    e->accept();
}

#include "moc_picturewidget.cpp"
