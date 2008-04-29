#ifndef __LDRAW_RENDERWIDGET_GL_H__
#define __LDRAW_RENDERWIDGET_GL_H__

#include <QGLWidget>

namespace LDraw {

class Part;

class RenderWidget : public QGLWidget {
    Q_OBJECT
public:
    RenderWidget(QWidget *parent = 0);
    virtual ~RenderWidget();
    
    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;
    
    Part *part() const;
    int color() const;
    void setPartAndColor(Part *part, int basecolor);
    
    void setXRotation(qreal r);
    void setYRotation(qreal r);
    void setZRotation(qreal r);
    
    void setZoom(qreal r);
    
protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);    

private:
    void create_display_list();
    void render_complete_visit(Part *part, int ldraw_basecolor);
    void render_dynamic_visit(Part *part, int ldraw_basecolor);
    void render_static_visit(Part *part, int ldraw_basecolor);
    
    Part *m_part;
    qreal m_rx, m_ry, m_rz;
    int m_color;
    GLuint m_dl_part;
    QPoint m_last_pos;
    bool m_initialized;
    qreal m_zoom;
};

}

#endif
