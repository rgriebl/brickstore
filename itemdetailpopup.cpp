
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTableView>
#include <QBitmap>

#include "window.h"
#include "itemdetailpopup.h"

class GlassButton : public QToolButton {
public:
    enum Type {
        Close
    };

    GlassButton(Type t, QWidget *parent)
        : QToolButton(parent), m_type(t), m_hovered(false)
    { }

protected:
    void enterEvent(QEvent *)
    {
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent *)
    {
        m_hovered = false;
        update();
    }

    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int side = qMin(width(), height());
        QRect r = QRect((width() - side)/2, (height() - side)/2, side, side);

        QPainterPath iconpath;

        switch(m_type) {
        case Close: {
            qreal s3  = side/3;
            qreal s18 = side/18;
            iconpath.setFillRule(Qt::WindingFill);
            iconpath.addRect(-s3, -s18, 2*s3, 2*s18);
            iconpath.addRect(-s18, -s3, 2*s18, 2*s3);
            QTransform rot45;
            rot45.translate(side/2, side/2).rotate(45, Qt::ZAxis);
            iconpath = rot45.map(iconpath);
            break;
        }
        }

        int alpha = isDown() ? 172 : m_hovered ? 128 : 64;
        p.setBrush(QColor(255, 255, 255, alpha));
        p.setPen(Qt::NoPen);

        QPainterPath path;
        path.addEllipse(r);
        p.drawPath(path.subtracted(iconpath));
    }

private:
    Type m_type;
    bool m_hovered : 1;
};


ItemDetailPopup::ItemDetailPopup(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint), m_item(0), m_part(0), m_pic(0), m_pressed(false), m_connected(false)
{
    setAttribute(Qt::WA_NoSystemBackground);

    m_stack = new QStackedWidget(this);
    m_close = new GlassButton(GlassButton::Close, this);
    m_close->setFixedSize(20, 20);

    m_blpic = new QLabel(m_stack);
    m_blpic->setAlignment(Qt::AlignCenter);
    m_blpic->setFixedSize(640, 480);
    m_ldraw = new LDraw::RenderOffscreenWidget(this);
    m_stack->addWidget(m_blpic);
    m_stack->addWidget(m_ldraw);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(m_close);
    lay->addWidget(m_stack);

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
        m_pic = BrickLink::core()->largePicture(m_item->item(), true);
        m_pic->addRef();
        m_part = LDraw::core()->itemModel(m_item->item()->id());

        m_blpic->setText(QString());

        if (m_part && m_part->root() && m_item->color()->ldrawId() >= 0) {
            m_ldraw->setPartAndColor(m_part->root(), m_item->color()->ldrawId());
            m_stack->setCurrentWidget(m_ldraw);
            m_blpic->setText(QString());
        } else {
            redraw();
            m_stack->setCurrentWidget(m_blpic);
            m_ldraw->setPartAndColor(0, -1);
        }
    } else {
        m_blpic->setText(QString());
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

        if (m_pic->updateStatus() == BrickLink::Updating)
            m_blpic->setText(QLatin1String("<center><i>") +
                    tr("Please wait ...updating") +
                    QLatin1String("</i></center>"));
        else if (m_pic->valid()) {
        //    // unfortunately, this looks very crappy for most of BLs pictures
        //    QPixmap pix = m_pic->pixmap();
        //    if (!m_pic->image().hasAlphaChannel())
        //        pix.setMask(QBitmap::fromImage(m_pic->image().createHeuristicMask()));
        //    m_blpic->setPixmap(pix);
            m_blpic->setPixmap(m_pic->pixmap());
        }
        else
            m_blpic->setText(QString::null);
    }
}

void ItemDetailPopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0, 0, 0 , 180));
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
    if (e->button() == Qt::LeftButton) {
        m_movepos = e->globalPos();
        m_pressed = true;
    }
}

void ItemDetailPopup::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_movepos = QPoint();
        m_pressed = false;
    }
}

void ItemDetailPopup::mouseMoveEvent(QMouseEvent *e)
{
    if (m_pressed && e->buttons() == Qt::LeftButton) {
       move(pos() + e->globalPos() - m_movepos);
       m_movepos = e->globalPos();
    }
}

