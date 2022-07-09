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
#include <QtCore/QCoreApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QHelpEvent>
#include <QtQuick3D/QQuick3D>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickItemGrabResult>

#include "rendercontroller.h"
#include "renderwidget.h"

namespace LDraw {


RenderWidget::RenderWidget(QQmlEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_controller(new RenderController(this))
{
    connect(m_controller, &RenderController::tumblingAnimationActiveChanged,
            this, &RenderWidget::animationActiveChanged);
    connect(m_controller, &RenderController::requestContextMenu,
            this, [this](const QPointF &pos) {
        auto cme = new QContextMenuEvent(QContextMenuEvent::Mouse, pos.toPoint(), mapToGlobal(pos.toPoint()));
        QCoreApplication::postEvent(this, cme);
    });
    connect(m_controller, &RenderController::requestToolTip,
            this, [this](const QPointF &pos) {
        auto he = new QHelpEvent(QHelpEvent::ToolTip, pos.toPoint(), mapToGlobal(pos.toPoint()));
        QCoreApplication::postEvent(this, he);
    });

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_window = engine ? new QQuickView(engine, nullptr) : new QQuickView();

    QSurfaceFormat fmt = QQuick3D::idealSurfaceFormat();
    m_window->setFormat(fmt);
    m_window->setResizeMode(QQuickView::SizeRootObjectToView);
    paletteChange();

    m_window->setInitialProperties({
                                       { u"renderController"_qs, QVariant::fromValue(m_controller) },
                                   });
    m_window->setSource(QUrl(u"qrc:/LDraw/PartRenderer.qml"_qs));

    m_widget = QWidget::createWindowContainer(m_window, this);
    m_widget->setMinimumSize(100, 100);

    layout->addWidget(m_widget, 10);
    languageChange();
}

RenderWidget::~RenderWidget()
{
    // we need to make sure the widget/window dies before the RenderController
    delete m_widget;
    m_widget = nullptr;
}

RenderController *RenderWidget::controller()
{
    return m_controller;
}

void RenderWidget::setPartAndColor(Part *part, int ldrawColorId)
{
    m_controller->setPartAndColor(part, ldrawColorId);
}

void RenderWidget::setPartAndColor(Part *part, const BrickLink::Color *color)
{
    m_controller->setPartAndColor(part, color);
}

bool RenderWidget::isAnimationActive() const
{
    return m_controller->isTumblingAnimationActive();
}

void RenderWidget::setAnimationActive(bool active)
{
    m_controller->setTumblingAnimationActive(active);
}

bool RenderWidget::startGrab()
{
    if (!m_grabResult) {
        m_grabResult = m_window->rootObject()->grabToImage();
        if (m_grabResult) {
            connect(m_grabResult.get(), &QQuickItemGrabResult::ready,
                    this, [this]() {
                emit grabFinished(m_grabResult->image());
                m_grabResult.clear();
            });
            return true;
        }
    }
    return false;
}

void RenderWidget::resetCamera()
{
    m_controller->resetCamera();
}

void RenderWidget::startAnimation()
{
    m_controller->setTumblingAnimationActive(true);
}

void RenderWidget::stopAnimation()
{
    m_controller->setTumblingAnimationActive(false);
}

void LDraw::RenderWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::PaletteChange)
        paletteChange();
    else if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

void RenderWidget::paletteChange()
{
    m_controller->setClearColor(palette().color(QPalette::Active, backgroundRole()));
}

void RenderWidget::languageChange()
{
    setToolTip(tr("Hold left button: Rotate\nHold right button: Move\nMouse wheel: Zoom\nDouble click: Reset camera\nRight click: Menu"));
}

} // namespace LDraw

#include "moc_renderwidget.cpp"
