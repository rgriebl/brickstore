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

#include "ldraw.h"
#include "glrenderer.h"
#include "renderwindow.h"


LDraw::RenderWindow::RenderWindow()
    : QOpenGLWindow()
{
    QSurfaceFormat fmt = format();
    GLRenderer::adjustSurfaceFormat(fmt);
    setFormat(fmt);

    m_renderer = new GLRenderer(this);
    resetCamera();
    connect(m_renderer, &GLRenderer::makeCurrent, this, &RenderWindow::slotMakeCurrent);
    connect(m_renderer, &GLRenderer::doneCurrent, this, &RenderWindow::slotDoneCurrent);
    connect(m_renderer, &GLRenderer::updateNeeded, this, QOverload<>::of(&RenderWindow::update));

    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWindow::setClearColor(const QColor &color)
{
    m_renderer->setClearColor(color);
}

int LDraw::RenderWindow::color() const
{
    return m_renderer->color();
}

void LDraw::RenderWindow::setPartAndColor(Part *part, const QColor &color)
{
    m_renderer->setPartAndColor(part, color);
}

void LDraw::RenderWindow::setPartAndColor(Part *part, int basecolor)
{
    m_renderer->setPartAndColor(part, basecolor);
}

LDraw::Part *LDraw::RenderWindow::part() const
{
    return m_renderer->part();
}

void LDraw::RenderWindow::mousePressEvent(QMouseEvent *e)
{
    setCursor(Qt::ClosedHandCursor);
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWindow::mouseReleaseEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
    setCursor(Qt::OpenHandCursor);
}

void LDraw::RenderWindow::mouseMoveEvent(QMouseEvent *e)
{
    m_renderer->handleMouseEvent(e);
}

void LDraw::RenderWindow::wheelEvent(QWheelEvent *e)
{
    m_renderer->handleWheelEvent(e);
}

bool LDraw::RenderWindow::event(QEvent *e)
{
    if (e->type() == QEvent::NativeGesture) {
        const auto *nge = static_cast<QNativeGestureEvent *>(e);
        if (nge->gestureType() == Qt::ZoomNativeGesture) {
            float d = 1.0f + float(nge->value());
            m_renderer->setZoom(m_renderer->zoom() * d);
            e->accept();
            return true;
        } else if (nge->gestureType() == Qt::SmartZoomNativeGesture) {
            m_renderer->resetTransformation();
            e->accept();
            return true;
        } else if (nge->gestureType() == Qt::RotateNativeGesture) {
            auto q = m_renderer->rotation();
            float p, y, r;
            q.getEulerAngles(&p, &y, &r);
            r += float(nge->value());
            m_renderer->setRotation(QQuaternion::fromEulerAngles(p, y, r));
            e->accept();
            return true;
        }
    }
    return QOpenGLWindow::event(e);
}

void LDraw::RenderWindow::slotMakeCurrent()
{
    makeCurrent();
}

void LDraw::RenderWindow::slotDoneCurrent()
{
    doneCurrent();
}

void LDraw::RenderWindow::initializeGL()
{
    m_renderer->initializeGL(context());
}

void LDraw::RenderWindow::resizeGL(int w, int h)
{
    m_renderer->resizeGL(context(), w, h);
}

void LDraw::RenderWindow::paintGL()
{
    m_renderer->paintGL(context());
}

void LDraw::RenderWindow::resetCamera()
{
    stopAnimation();
    m_renderer->resetTransformation();
}

void LDraw::RenderWindow::startAnimation()
{
    m_renderer->startAnimation();
}

void LDraw::RenderWindow::stopAnimation()
{
    m_renderer->stopAnimation();
}

#endif // !QT_NO_OPENGL

#include "moc_renderwindow.cpp"

