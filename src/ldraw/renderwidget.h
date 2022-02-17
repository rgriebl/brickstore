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
#pragma once

#include <qtguiglobal.h>

#if !defined(QT_NO_OPENGL)

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QVector3D>
#include <QScopedPointer>

#include <vector>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

#ifdef MessageBox
#undef MessageBox
#endif

QT_FORWARD_DECLARE_CLASS(QOpenGLFramebufferObject)

namespace LDraw {

class GLRenderer;
class Part;


class RenderWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = nullptr);
    ~RenderWidget() override;

    void setClearColor(const QColor &color);

    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, const QColor &color);
    void setPartAndColor(Part *part, int basecolor);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    bool isAnimationActive() const;

public slots:
    void resetCamera();
    void startAnimation();
    void stopAnimation();

protected slots:
    void slotMakeCurrent();
    void slotDoneCurrent();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    GLRenderer *m_renderer;
};

}

#endif //!QT_NO_OPENGL

