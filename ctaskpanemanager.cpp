/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <math.h>

#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QPixmapCache>
#include <QStyle>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QCursor>
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QDockWidget>
#include <QMouseEvent>
#include <QStyleOptionFocusRect>

#include <QTimeLine>
#include "ctaskpanemanager.h"
#include "utility.h"

#if defined( Q_WS_WIN )
#include <windows.h>
#endif

#define ANIM_DURATION   300
#define ANIM_FPS        33


class CTaskPane;

class CTaskPaneManagerPrivate {
public:
    CTaskPaneManager::Mode  m_mode;
    QMainWindow *m_mainwindow;
    QDockWidget *m_panedock;
    CTaskPane   *m_taskpane;

    struct Item {
        QDockWidget *m_itemdock;
        QWidget *    m_widget;
        QString      m_text;
        bool         m_visible;

        // only in modern mode
        QIcon        m_icon;
        int          m_orig_framestyle;
        QRect        m_header_rect;

        Item()
                : m_itemdock(0), m_widget(0), m_visible(true),
                m_orig_framestyle(QFrame::NoFrame)
        { }

        Item(const Item &copy)
                : m_itemdock(copy.m_itemdock), m_widget(copy.m_widget),
                m_text(copy.m_text), m_visible(copy.m_visible), m_icon(copy.m_icon),
                m_orig_framestyle(copy.m_orig_framestyle), m_header_rect(copy.m_header_rect)
        { }
    };

    int findItem(QWidget *w);
    QList<Item>  m_items;
    int          m_hot_item;

    QLinearGradient    m_bgrad, m_tgrad;
    QColor             m_lcol, m_rcol, m_tcol, m_fcol;

    QRect m_margins;
    QFont m_bold_font;
    int m_xh, m_xw;
    int m_vspace, m_hspace;
    int m_arrow;
    QSize m_size_hint;

    QTimeLine m_anim_timeline;
    QImage    m_anim_src;
    QImage    m_anim_img;
    int       m_anim_item;
};



class CTaskPane : public QWidget {
    Q_OBJECT

public:
    CTaskPane(CTaskPaneManager *manager, CTaskPaneManagerPrivate *priv);

    QRegion recalcLayout();
    virtual QSize sizeHint() const;

protected:
    virtual void changeEvent(QEvent *e);
    virtual void leaveEvent(QEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void paintEvent(QPaintEvent *e);
    virtual void resizeEvent(QResizeEvent *e);

    void updateFont();
    void updateBackground();
    int findItemHeader(const QPoint &p);

private slots:
    void animateExpand(qreal value);


private:
    CTaskPaneManager        *q;
    CTaskPaneManagerPrivate *d;
};

CTaskPane::CTaskPane(CTaskPaneManager *manager, CTaskPaneManagerPrivate *priv)
    : QWidget(priv->m_panedock), q(manager), d(priv)
{
    hide();
    updateFont();
    updateBackground();

    setMouseTracking(true);

    connect(&d->m_anim_timeline, SIGNAL(valueChanged(qreal)), this, SLOT(animateExpand(qreal)));
}

void CTaskPane::animateExpand(qreal value)
{
    int frame = d->m_anim_timeline.currentFrame();
    int start = d->m_anim_timeline.startFrame();
    int end = d->m_anim_timeline.endFrame();

    if (d->m_anim_timeline.direction() == QTimeLine::Backward)
        qSwap(start, end);

    if (d->m_anim_item > -1 && frame == start) {
        QWidget *w = d->m_items[d->m_anim_item].m_widget;

        d->m_anim_src = QPixmap::grabWidget(w, QRect(0, 0, w->width(), w->height())).toImage();
    }
    else if (d->m_anim_item > -1 && frame == end) {
        d->m_anim_item = -1;
        recalcLayout();
        update(); // could be smaller
    }

    if (d->m_anim_item > -1) {
        int h = qMin(d->m_anim_src.height(), int(d->m_anim_src.height() * value));
#if 0
        // SCALE
        d->m_anim_img = d->m_anim_src.scaled(d->m_anim_src.width(), h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
#elif 1
        // SCROLL
        d->m_anim_img = d->m_anim_src.copy(0, d->m_anim_src.height() - h, d->m_anim_src.width(), h);
#else
        // PERSPECTIVE
        const qreal pi = 3.14159265;
        QTransform tf;
        tf.translate(d->m_anim_src.width() / 2, 0);
        tf.rotate(-180.0 * acos(qreal(h) / qreal(d->m_anim_src.height())) / pi, Qt::XAxis);
        tf.translate(-d->m_anim_src.width() / 2, 0);
        d->m_anim_img = d->m_anim_src.transformed(tf, Qt::FastTransformation);
#endif
        QImage alpha(d->m_anim_img.size(), QImage::Format_Indexed8);
        alpha.fill(127 + char(value * 128));
        d->m_anim_img.setAlphaChannel(alpha);

        update(recalcLayout());
    }
}


void CTaskPane::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange)
        updateFont();
    else if (e->type() == QEvent::PaletteChange)
        updateBackground();
    QWidget::changeEvent(e);
}

void CTaskPane::resizeEvent(QResizeEvent *e)
{
    updateBackground();
    update(recalcLayout());
    QWidget::resizeEvent(e);
}

void CTaskPane::updateFont()
{
    d->m_bold_font = font();
    d->m_bold_font.setBold(true);

    QFontMetrics fm(d->m_bold_font);

    d->m_xw = fm.width("x");
    d->m_xh = fm.height();

    d->m_arrow = (d->m_xh / 2) | 1;
    d->m_hspace = d->m_xw;
    d->m_vspace = d->m_xh / 6;

    recalcLayout();
    update();
}


#ifdef Q_WS_WIN
static inline QRgb colorref2qrgb(COLORREF col)
{
    return qRgb(GetRValue(col), GetGValue(col), GetBValue(col));
}
#endif

void CTaskPane::updateBackground()
{
#if defined( Q_WS_WIN )
    QColor text (colorref2qrgb(GetSysColor(COLOR_CAPTIONTEXT)));
    QColor cap1 (colorref2qrgb(GetSysColor(COLOR_ACTIVECAPTION)));
    QColor cap2;
    if (QSysInfo::WindowsVersion == QSysInfo::WV_95 || QSysInfo::WindowsVersion == QSysInfo::WV_NT)
        cap2 = Utility::contrastColor(cap2, 0.2f);
    else
        cap2.setRgb(colorref2qrgb(GetSysColor(COLOR_GRADIENTACTIVECAPTION)));

    if (Utility::colorDifference(cap1, cap2) < 0.1f)
        cap2 = Utility::contrastColor(cap2, 0.2f);

    d->m_lcol = cap1;
    d->m_rcol = cap2;
    d->m_tcol = text;

#else // if defined( Q_WS_MACX ) || defined( Q_WS_X11 )
    // leopard title bar gradient
    QColor cap1(192, 192, 192);
    QColor cap2(150, 150, 150);

    d->m_lcol = QColor(220, 220, 220);
    d->m_rcol = QColor(64, 64, 64);
    d->m_tcol = Qt::black;

#endif

    d->m_bgrad = QLinearGradient(1, 0, width()-2, 0);
    d->m_bgrad.setColorAt(0, cap1);
    d->m_bgrad.setColorAt(1, cap2);

    QColor cap1a = cap1;
    cap1a.setAlphaF(0.f);
    QColor cap2a = cap2;
    cap2a.setAlphaF(0.f);

    d->m_tgrad = QLinearGradient(0, 0, width() - d->m_margins.left() - d->m_margins.right(), 0);
    d->m_tgrad.setColorAt(0,   cap2a);
    d->m_tgrad.setColorAt(0.1, cap2);
    d->m_tgrad.setColorAt(0.5, cap2);
    d->m_tgrad.setColorAt(1,   cap1a);

    d->m_fcol = Utility::gradientColor(Utility::gradientColor(cap1, cap2), palette().color(QPalette::Base), .5f);
}


int CTaskPane::findItemHeader(const QPoint &p)
{
    int idx = 0;
    foreach (const CTaskPaneManagerPrivate::Item &item, d->m_items) {
        if (item.m_header_rect.contains(p))
            return idx;
        idx++;
    }
    return -1;
}

void CTaskPane::mouseMoveEvent(QMouseEvent *e)
{
    int idx = findItemHeader(e->pos());

    if (idx != d->m_hot_item) {
        if (d->m_hot_item > -1)
            update(d->m_items[d->m_hot_item].m_header_rect);

        d->m_hot_item = idx;

        if (d->m_hot_item > -1) {
            update(d->m_items[d->m_hot_item].m_header_rect);
            setCursor(Qt::PointingHandCursor);
        }
        else
            unsetCursor();
    }

    QWidget::mouseMoveEvent(e);
}

void CTaskPane::leaveEvent(QEvent *e)
{
    if (d->m_hot_item > -1) {
        unsetCursor();
        update(d->m_items[d->m_hot_item].m_header_rect);
        d->m_hot_item = -1;
    }
    QWidget::leaveEvent(e);
}

void CTaskPane::mouseReleaseEvent(QMouseEvent *e)
{
    int idx = findItemHeader(e->pos());
    if ((idx > -1) && (e->button() == Qt::LeftButton)) {
        QWidget *w = d->m_items[idx].m_widget;
        q->setItemVisible(w, !q->isItemVisible(w));
        e->accept();
    }
}

void CTaskPane::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    p.setClipRect(e->rect());
    int w = width();
    int h = height();

    p.fillRect(1, 0, w-2, h, d->m_bgrad);
    p.setPen(d->m_lcol);
    p.drawLine(0, 0, 0, h-1);
    p.setPen(d->m_rcol);
    p.drawLine(w-1, 0, w-1, h-1);
    p.setPen(d->m_fcol);

    QRect r;
    int idx = 0;
    foreach (const CTaskPaneManagerPrivate::Item &item, d->m_items) {
        bool hot = (idx == d->m_hot_item);
        bool anim = (idx == d->m_anim_item);

        r = item.m_header_rect;
        int dy = r.height() - (2 * d->m_vspace + d->m_xh);
        p.setBrushOrigin(r.x(), 0);
        p.fillRect(r.adjusted(0, dy, 0, 0), d->m_tgrad);
        p.setBrushOrigin(0, 0);

        int offx = d->m_hspace;

        if (!item.m_icon.isNull()) {
            item.m_icon.paint(&p, QRect(r.x() + offx, r.top(), 22, 22), Qt::AlignCenter, hot ? QIcon::Active : QIcon::Normal, QIcon::On);

            offx += (22 + d->m_hspace);
        }

        QRect tr = r.adjusted(offx, dy, -d->m_hspace, 0);

        p.setFont(d->m_bold_font);
        p.setPen(Utility::contrastColor(hot ? Utility::contrastColor(d->m_tcol, 0.1f) : d->m_tcol, 0.5f));
        p.drawText(tr.translated(1,1), Qt::AlignLeft | Qt::AlignVCenter, item.m_text);

        p.setPen(hot ? Utility::contrastColor(d->m_tcol,0.1f) : d->m_tcol);
        p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, item.m_text);

        QRect ar(tr.right() - d->m_arrow, tr.top() + (tr.height() - d->m_arrow) / 2, d->m_arrow, d->m_arrow);

        for (int i = 0; i < d->m_arrow; i++) {
            int off = i / 2;

            if (item.m_visible)
                p.drawLine(ar.left() + off, ar.top() + i, ar.right() - off, ar.top() + i);
            else
                p.drawLine(ar.left() + i, ar.top() + off, ar.left() + i, ar.bottom() - off);
        }

        QRect wrect = item.m_widget->geometry();

        if (anim) {
            wrect.moveTo(r.left() + 2, r.bottom() + 3);
            wrect.setHeight(d->m_anim_img.height());
            p.drawImage(wrect, d->m_anim_img);
        }

        p.setPen(d->m_fcol);
        if (item.m_visible || anim)
            p.drawRect(wrect.adjusted(-2, -2, 1, 1));
        else
            p.drawLine(r.x(), r.bottom() + 1, r.right(), r.bottom() + 1);

        idx++;
    }
}

QSize CTaskPane::sizeHint() const
{
    if (!d->m_size_hint.isValid())
        const_cast<CTaskPane *>(this)->recalcLayout();
    return d->m_size_hint;
}

QRegion CTaskPane::recalcLayout()
{
    //updateGeometry();

    int offx = d->m_margins.left();
    int offy = d->m_margins.top();
    int w = width() - offx - d->m_margins.right();
    int h = height() - offy - d->m_margins.bottom();
    int maxw = 0;

    if (w <= 0 || h <= 0)
        return QRegion();

    bool first = true;
    int headerh = qMax(22, 2 * d->m_vspace + d->m_xh + 1);

    QRegion dirty;

    for (int idx = 0; idx < d->m_items.size(); idx++) {
        CTaskPaneManagerPrivate::Item &item = d->m_items[idx];

        if (!first)
            offy += d->m_xh / 2;
        first = false;

        QRect headerr(offx, offy, w, headerh);

        if (headerr != item.m_header_rect) {
            dirty |= item.m_header_rect.adjusted(0, 0, 0, 1);
            dirty |= headerr.adjusted(0, 0, 0, 1);
            item.m_header_rect = headerr;
        }

        offy += (headerh + 1);

        bool anim = (idx == d->m_anim_item);
        QWidget *wid = item.m_widget;
        QSize ws = wid->minimumSizeHint();

        if (ws.isEmpty())
            ws = wid->minimumSize();

        if (item.m_visible) {
            int wh = ws.height();

            QRect widgetr(offx + 2, offy + 1, w - 4, wh);

            if (wid->geometry() != widgetr) {
                dirty |= wid->geometry().adjusted(-2, -2, 2, 2);
                dirty |= widgetr.adjusted(-2, -2, 2, 2);
                wid->setGeometry(widgetr);
            }

            if (anim) {
                wid->hide();
                dirty |= QRect(offx, offy - 1, w, d->m_anim_img.height() + 4);
            }
            else if (isVisible())
                wid->show();

            offy += (anim ? d->m_anim_img.height() + 2 : wh + 2);
        }
        else {
            wid->hide();
            wid->setGeometry(0, 0, w - 4, ws.height());

            if (anim)
                dirty |= QRect(offx, offy - 1, w, ws.height() + 4);

            offy += (anim ? d->m_anim_img.height() + 2 : 0);
        }

        // calculate sizeHint
        QFontMetrics fm(d->m_bold_font);
        int iw = item.m_icon.isNull() ? 0 : 22;
        int tw = fm.width(item.m_text) + 1;

        maxw = qMax(maxw, d->m_hspace + iw + (iw ? d->m_hspace : 0) + tw + (tw ? d->m_hspace : 0) + d->m_arrow + d->m_hspace);
        maxw = qMax(maxw, ws.width() + 4);
    }
    d->m_size_hint = QSize(offx + maxw + d->m_margins.right(), offy + d->m_margins.bottom());

    return dirty;
}




































CTaskPaneManager::CTaskPaneManager(QMainWindow *parent)
        : QObject(parent)
{
    d = new CTaskPaneManagerPrivate;
    d->m_mainwindow = parent;
    d->m_panedock = 0;
    d->m_taskpane = 0;
    d->m_mode = Classic;
    d->m_hot_item = -1;
    d->m_anim_item = -1;
    d->m_margins.setRect(6, 6, 1, 1);

    d->m_anim_timeline.setDuration(ANIM_DURATION);
    d->m_anim_timeline.setFrameRange(0, ANIM_DURATION * ANIM_FPS / 1000 - 1);
    d->m_anim_timeline.setUpdateInterval(1000 / ANIM_FPS);
    d->m_anim_timeline.setCurveShape(QTimeLine::EaseInCurve);
    d->m_anim_timeline.setDirection(QTimeLine::Forward);

    setMode(Modern);
}

CTaskPaneManager::~CTaskPaneManager()
{
    delete d;
}

int CTaskPaneManagerPrivate::findItem(QWidget *w)
{
    int idx = 0;
    foreach(const Item &item, m_items) {
        if (item.m_widget == w)
            return idx;
        idx++;
    }
    return -1;
}

void CTaskPaneManager::addItem(QWidget *w, const QIcon &is, const QString &text)
{
    if (w && (d->findItem(w) == -1)) {
        CTaskPaneManagerPrivate::Item item;
        item.m_itemdock   = 0;
        item.m_widget     = w;
        item.m_icon       = is;
        item.m_text       = text;
        item.m_orig_framestyle  = QFrame::NoFrame;

        kill();
        d->m_items.append(item);
        create();
    }
}

void CTaskPaneManager::removeItem(QWidget *w, bool delete_widget)
{
    int idx = d->findItem(w);

    if (idx > -1) {
        kill();

        d->m_items.removeAt(idx);
        if (delete_widget)
            delete w;

        create();
    }
}

void CTaskPaneManager::setItemText(QWidget *w, const QString &txt)
{
    int idx = d->findItem(w);

    if (idx > -1) {
        d->m_items[idx].m_text = txt;

        kill();
        create();
    }
}


QString CTaskPaneManager::itemText(QWidget *w) const
{
    int idx = d->findItem(w);
    return idx == -1 ? QString() : d->m_items[idx].m_text;
}


bool CTaskPaneManager::isItemVisible(QWidget *w) const
{
    int idx = d->findItem(w);
    return idx == -1 ? false : d->m_items[idx].m_visible;
}

void CTaskPaneManager::setItemVisible(QWidget *w, bool visible)
{
    int idx = d->findItem(w);

    if (idx > -1) {
        CTaskPaneManagerPrivate::Item &item = d->m_items[idx];
        if (visible != item.m_visible) {
            item.m_visible = visible;

            if (d->m_mode == Modern) {
                d->m_anim_item = idx;
                if (d->m_anim_timeline.state() == QTimeLine::Running) {
                    d->m_anim_timeline.setCurrentTime(d->m_anim_timeline.duration());
                    d->m_anim_timeline.stop();
                    d->m_taskpane->recalcLayout();
                }
                d->m_anim_timeline.setDirection(visible ? QTimeLine::Forward : QTimeLine::Backward);
                d->m_anim_timeline.start();
            }
            else
                item.m_itemdock->setShown(visible);
        }
    }
}

CTaskPaneManager::Mode CTaskPaneManager::mode() const
{
    return d->m_mode;
}

void CTaskPaneManager::setMode(Mode m)
{
    if (m == d->m_mode)
        return;

    kill();
    d->m_mode = m;
    create();
}

void CTaskPaneManager::kill()
{
    // kill existing window
    if (d->m_mode == Modern) {
        for (int idx = 0; idx < d->m_items.size(); idx++) {
            CTaskPaneManagerPrivate::Item &item = d->m_items[idx];

            item.m_widget->hide();
            item.m_widget->setParent(0);

            if (qobject_cast<QFrame *>(item.m_widget)) {
                QFrame *f = static_cast <QFrame *>(item.m_widget);
                f->setFrameStyle(item.m_orig_framestyle);
            }
        }
        delete d->m_panedock;
        d->m_panedock = 0;
        d->m_taskpane = 0;
    }
    else {
        for (int idx = 0; idx < d->m_items.size(); idx++) {
            CTaskPaneManagerPrivate::Item &item = d->m_items[idx];

            item.m_widget->hide();
            item.m_widget->setParent(0);

            disconnect(item.m_itemdock, SIGNAL(visibilityChanged(bool)), this, SLOT(dockVisibilityChanged(bool)));

            delete item.m_itemdock;
            item.m_itemdock = 0;
        }
    }
}

void CTaskPaneManager::create()
{
    // create new window
    if (d->m_mode == Modern) {
        d->m_panedock = new QDockWidget(d->m_mainwindow);
        d->m_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, d->m_panedock);

        d->m_panedock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        d->m_panedock->setTitleBarWidget(new QWidget());

        d->m_taskpane = new CTaskPane(this, d);
        d->m_panedock->setWidget(d->m_taskpane);

        for (int idx = 0; idx < d->m_items.size(); idx++) {
            CTaskPaneManagerPrivate::Item &item = d->m_items[idx];

            if (qobject_cast <QFrame *> (item.m_widget)) {
                QFrame *f = static_cast <QFrame *>(item.m_widget);

                item.m_orig_framestyle = f->frameStyle();
                f->setFrameStyle(QFrame::NoFrame);
            }
            item.m_widget->setParent(d->m_taskpane);
            item.m_widget->show();
        }
        d->m_taskpane->recalcLayout();
        d->m_taskpane->setFixedWidth(d->m_taskpane->sizeHint().width());
        d->m_taskpane->show();
        d->m_panedock->show();
    }
    else {
        for (int idx = 0; idx < d->m_items.size(); idx++) {
            CTaskPaneManagerPrivate::Item &item = d->m_items[idx];

            item.m_itemdock = new QDockWidget(item.m_text, d->m_mainwindow);
            d->m_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, item.m_itemdock);

            item.m_itemdock->setFeatures(QDockWidget::AllDockWidgetFeatures);
            item.m_itemdock->setWidget(item.m_widget);

            item.m_widget->show();

            if (item.m_visible)
                item.m_itemdock->show();
            else
                item.m_itemdock->hide();

            connect(item.m_itemdock, SIGNAL(visibilityChanged(bool)), this, SLOT(dockVisibilityChanged(bool)));
        }
    }
}

void CTaskPaneManager::dockVisibilityChanged(bool b)
{
    QDockWidget *dock = qobject_cast<QDockWidget *>(sender());

    for (int idx = 0; idx < d->m_items.size(); idx++) {
        if (d->m_items[idx].m_itemdock == dock) {
            d->m_items[idx].m_visible = b;
            break;
        }
    }
}


QMenu *CTaskPaneManager::createItemVisibilityMenu() const
{
    QMenu *m = new QMenu(d->m_mainwindow);
    connect(m, SIGNAL(aboutToShow()), this, SLOT(itemMenuAboutToShow()));
    connect(m, SIGNAL(triggered(QAction *)), this, SLOT(itemMenuTriggered(QAction *)));
    return m;
}

void CTaskPaneManager::itemMenuAboutToShow()
{
    QMenu *m = qobject_cast<QMenu *>(sender());

    if (m) {
        m->clear();

        bool allon = true;
        QAction *a;

        int idx = 0;
        foreach (const CTaskPaneManagerPrivate::Item &item, d->m_items) {
            a = m->addAction(item.m_text);
            a->setData(idx);
            a->setCheckable(true);

            bool vis = isItemVisible(item.m_widget);
            if (d->m_mode == Modern) {
                bool pdvis = d->m_panedock->isVisible();
                vis &= pdvis;
                a->setEnabled(pdvis);
            }
            a->setChecked(vis);
            allon &= vis;
            idx++;
        }
        m->addSeparator();

        a = m->addAction(tr("All"));

        if (d->m_mode == Modern)
            allon = d->m_panedock->isVisible();

        a->setCheckable(true);
        a->setChecked(allon);
        a->setData(allon ? -2 : -1);

        m->addSeparator();

        a = m->addAction(tr("Modern (fixed)"));
        a->setCheckable(true);
        a->setChecked(d->m_mode == Modern);
        a->setData(-3);

        a = m->addAction(tr("Classic (movable)"));
        a->setCheckable(true);
        a->setChecked(d->m_mode != Modern);
        a->setData(-4);
    }
}

void CTaskPaneManager::itemMenuTriggered(QAction *a)
{
    if (!a)
        return;

    int id = qvariant_cast<int>(a->data());

    if (id == -4) {
        setMode(Classic);
    }
    else if (id == -3) {
        setMode(Modern);
    }
    else if ((id < 0) && (d->m_mode == Modern)) {
        d->m_panedock->setShown(id == -1 ? true : false);
    }
    else {
        int index = 0;
        for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it, index++) {
            CTaskPaneManagerPrivate::Item &item = *it;


            if ((id == index) || (id < 0)) {
                bool onoff = !isItemVisible(item.m_widget);

                if (d->m_mode == Classic) {
                    if (id == -1)
                        onoff = true;
                    else if (id == -2)
                        onoff = false;
                }
                setItemVisible(item.m_widget, onoff);
            }
        }
    }
}


class TaskPaneAction : public QAction {
public:
    TaskPaneAction(CTaskPaneManager *tpm, QObject *parent, const char *name = 0)
        : QAction(parent)
    {
        setObjectName(name);
        setMenu(tpm->createItemVisibilityMenu());
    }
};

QAction *CTaskPaneManager::createItemVisibilityAction(QObject *parent, const char *name) const
{
    return new TaskPaneAction(const_cast<CTaskPaneManager *>(this), parent, name);
}



#if 0

CTaskPane::CTaskPane(QWidget *parent)
    : QWidget(parent), m_expanding(0)
{
    m_margins = QRect(6, 6, 1, 1);
    m_groups = new QList <CTaskGroup *>;
    //m_groups->setAutoDelete ( true );

    updateFont();
    updatePalette();

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);


#endif


#include "ctaskpanemanager.moc"
