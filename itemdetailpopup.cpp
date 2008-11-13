
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTableView>

#include "window.h"
#include "itemdetailpopup.h"


ItemDetailPopup::ItemDetailPopup(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint), m_item(0), m_part(0), m_pic(0), m_connected(0)
{
    //QSize mins(640, 4int(parent->width() * .8), int(parent->height() * .8));
    //QSize mins(int(parent->width() * .8), int(parent->height() * .8));
    //setAttribute(Qt::WA_NoSystemBackground);

    //m_stack = new QStackedWidget(this);
    m_close = new QToolButton(this);
    m_close->setText("X"); //TODO: pixmap

    //m_blpic = new QLabel(m_stack);
    //m_blpic->setFixedSize(640, 480);
    m_ldraw = new LDraw::RenderOffscreenWidget(this);
    //m_stack->addWidget(m_blpic);
    //m_stack->addWidget(m_ldraw);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(m_close);
    lay->addWidget(m_ldraw);

    connect(m_close, SIGNAL(clicked()), this, SLOT(close()));
}

ItemDetailPopup::~ItemDetailPopup()
{
    if (m_pic)
        m_pic->release();
    if (m_part)
        delete m_part;
}

void ItemDetailPopup::setItem(Document::Item *item)
{
    m_item = item;

    if (m_pic)
        m_pic->release();
    m_pic = 0;
    if (m_part)
        delete m_part;
    m_part = 0;

    if (m_item) {
        m_pic = BrickLink::core()->picture(m_item->item(), m_item->color(), true);
        m_pic->addRef();
        m_part = LDraw::core()->itemModel(m_item->item()->id());

        //m_blpic->setText(QString());

        if (m_part && m_part->root() && m_item->color()->ldrawId() >= 0) {
            m_ldraw->setPartAndColor(m_part->root(), m_item->color()->ldrawId());
            //m_stack->setCurrentWidget(m_ldraw);
            //m_blpic->setText(QString());
        } else {
            //redraw();
            //m_stack->setCurrentWidget(m_blpic);
            m_ldraw->setPartAndColor(0, -1);
        }
    } else {
        //m_blpic->setText(QString());
        m_ldraw->setPartAndColor(0, -1);
    }

    if (!m_connected && item)
        m_connected = connect(BrickLink::core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(gotUpdate(BrickLink::Picture *)));

}

void ItemDetailPopup::gotUpdate(BrickLink::Picture *pic)
{
    if (pic == m_pic) {
        redraw();
    }
}

void ItemDetailPopup::redraw()
{
    if (m_pic) {
        //setWindowTitle(QString(d->m_pic->item()->id()) + " " + d->m_pic->item()->name());
/*
        if (m_pic->updateStatus() == BrickLink::Updating)
            m_blpic->setText(QLatin1String("<center><i>") +
                    tr("Please wait ...updating") +
                    QLatin1String("</i></center>"));
        else if (m_pic->valid())
            m_blpic->setPixmap(m_pic->pixmap());
        else
            m_blpic->setText(QString::null);*/
    }
}

void ItemDetailPopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(255, 255, 255, 180));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 20, 20);
}

void ItemDetailPopup::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Space)
        close();
    else
        e->ignore();
}

void ItemDetailPopup::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_movepos = e->globalPos();
}

void ItemDetailPopup::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() == Qt::LeftButton) {
       move(pos() + e->globalPos() - m_movepos);
       m_movepos = e->globalPos();
    }
}

