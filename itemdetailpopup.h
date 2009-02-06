#ifndef ITEMDETAILPOPUP_H
#define ITEMDETAILPOPUP_H

#include <QDialog>

#include "document.h"
#include "ldraw.h"

namespace LDraw {
class Model;
class RenderOffscreenWidget;
}

class QToolButton;
class QStackedWidget;
class QLabel;
class QTableView;


class ItemDetailPopup : public QDialog {
    Q_OBJECT

public:
    ItemDetailPopup(QWidget *parent);
    virtual ~ItemDetailPopup();

public slots:
    void setItem(Document::Item *);

protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);

private slots:
    void gotUpdate(BrickLink::Picture *pic);

private:
    void redraw();

private:
    LDraw::RenderOffscreenWidget *m_ldraw;
    QLabel *m_blpic;

    QStackedWidget *m_stack;
    QToolButton *m_close;
    QToolButton *m_play;
    QToolButton *m_stop;
    QToolButton *m_view;
    Document::Item *m_item;
    QPoint m_movepos;
    LDraw::Model *m_part;
    BrickLink::Picture *m_pic;
    bool m_pressed;

    bool m_connected;
};

#endif

