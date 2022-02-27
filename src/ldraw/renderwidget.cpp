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
#include <qtguiglobal.h>

#if !defined(QT_NO_OPENGL)

#include <cfloat>
#include <cmath>

#include "library.h"
#include "glrenderer.h"
#include "renderwidget.h"


LDraw::RenderWidget::RenderWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_AlwaysStackOnTop);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QSurfaceFormat fmt = format();
    GLRenderer::adjustSurfaceFormat(fmt);
    setFormat(fmt);

    m_renderer = new GLRenderer(this);
    resetCamera();
    connect(m_renderer, &GLRenderer::makeCurrent, this, &RenderWidget::slotMakeCurrent);
    connect(m_renderer, &GLRenderer::doneCurrent, this, &RenderWidget::slotDoneCurrent);
    connect(m_renderer, &GLRenderer::updateNeeded, this, QOverload<>::of(&RenderWidget::update));

    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWidget::setClearColor(const QColor &color)
{
    m_renderer->setClearColor(color);
}

int LDraw::RenderWidget::color() const
{
    return m_renderer->color();
}

void LDraw::RenderWidget::setPartAndColor(Part *part, const QColor &color)
{
    m_renderer->setPartAndColor(part, color);
}

void LDraw::RenderWidget::setPartAndColor(Part *part, int basecolor)
{
    m_renderer->setPartAndColor(part, basecolor);
}

LDraw::Part *LDraw::RenderWidget::part() const
{
    return m_renderer->part();
}

QSize LDraw::RenderWidget::minimumSizeHint() const
{
    return { 50, 50 };
}

QSize LDraw::RenderWidget::sizeHint() const
{
    return { 100, 100 };
}

void LDraw::RenderWidget::mousePressEvent(QMouseEvent *e)
{
    setCursor(Qt::ClosedHandCursor);
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWidget::mouseReleaseEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWidget::wheelEvent(QWheelEvent *e)
{
    m_renderer->handleWheelEvent(e);
}

void LDraw::RenderWidget::slotMakeCurrent()
{
    makeCurrent();
}

void LDraw::RenderWidget::slotDoneCurrent()
{
    doneCurrent();
}

void LDraw::RenderWidget::initializeGL()
{
    m_renderer->initializeGL(context());
}

void LDraw::RenderWidget::resizeGL(int w, int h)
{
    m_renderer->resizeGL(context(), w, h);
}

void LDraw::RenderWidget::paintGL()
{
    m_renderer->paintGL(context());
}

void LDraw::RenderWidget::resetCamera()
{
    stopAnimation();
    m_renderer->resetTransformation();
}

void LDraw::RenderWidget::startAnimation()
{
    m_renderer->startAnimation();
}

void LDraw::RenderWidget::stopAnimation()
{
    m_renderer->stopAnimation();
}

#endif // !QT_NO_OPENGL

#include "moc_renderwidget.cpp"
