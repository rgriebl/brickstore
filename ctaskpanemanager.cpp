/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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

#include "ctaskpanemanager.h"
#include "cutility.h"

#if defined( Q_WS_WIN )
#include <windows.h>
#endif

class CTaskPaneManagerPrivate {
public:
    CTaskPaneManager::Mode  m_mode;
    QMainWindow *m_mainwindow;
    QDockWidget *m_panedock;
    CTaskPane   *m_taskpane;

    struct Item {
        QDockWidget *m_itemdock;
        uint         m_taskpaneid;
        QWidget *    m_widget;
        QIcon        m_iconset;
        QString      m_label;
        int          m_orig_framestyle;
        bool         m_expandible  : 1;
        bool         m_visible     : 1;

        Item()
                : m_itemdock(0), m_taskpaneid(0),
                m_widget(0), m_orig_framestyle(QFrame::NoFrame), m_expandible(false),
                m_visible(true)
        { }

        Item(const Item &copy)
                : m_itemdock(copy.m_itemdock), m_taskpaneid(copy.m_taskpaneid),
                m_widget(copy.m_widget), m_iconset(copy.m_iconset), m_label(copy.m_label),
                m_orig_framestyle(copy.m_orig_framestyle), m_expandible(copy.m_expandible),
                m_visible(copy.m_visible)
        { }
    };

    QList<Item>::iterator findItem(QWidget *w);
    QList<Item>::iterator noItem();

    QList<Item> m_items;
};



class CTaskGroup;


class CTaskPane : public QWidget {
    Q_OBJECT

public:
    CTaskPane(QWidget *parent);
    virtual ~CTaskPane();

    void addItem(QWidget *w, const QIcon &is, const QString &txt, bool expandible = true);
    void removeItem(QWidget *w, bool delete_widget = true);

    bool isExpanded(QWidget *w) const;
    void setExpanded(QWidget *w, bool exp);

    virtual QSize sizeHint() const;

signals:
    void itemVisibilityChanged(QWidget *, bool);

protected:
    friend class CTaskGroup;

    void drawHeaderBackground(QPainter *p, CTaskGroup *g, bool /*hot*/);
    void drawHeaderContents(QPainter *p, CTaskGroup *g, bool hot);
    void recalcLayout();
    QSize sizeForGroup(const CTaskGroup *g) const;

    virtual void resizeEvent(QResizeEvent *re);
    virtual void paintEvent(QPaintEvent *pe);
    virtual void changeEvent(QEvent *e);

    void updateFont();
    void updatePalette();
    void updateBackground();

private:
    QLinearGradient    m_bgrad, m_tgrad;
    QColor             m_lcol, m_rcol, m_tcol, m_fcol;

    QRect m_margins;
    QFont m_font;
    int m_xh, m_xw;
    int m_vspace, m_hspace;
    int m_arrow;
    QList <CTaskGroup *> *m_groups;
};



CTaskPaneManager::CTaskPaneManager(QMainWindow *parent)
        : QObject(parent)
{
    d = new CTaskPaneManagerPrivate;
    d->m_mainwindow = parent;
    d->m_panedock = 0;
    d->m_taskpane = 0;
    d->m_mode = Classic;
    setMode(Modern);
}

CTaskPaneManager::~CTaskPaneManager()
{
    delete d;
}

QList<CTaskPaneManagerPrivate::Item>::iterator CTaskPaneManagerPrivate::findItem(QWidget *w)
{
    QList<Item>::iterator it = m_items.begin();

    while (it != m_items.end()) {
        if ((*it).m_widget == w)
            break;
        ++it;
    }
    return it;
}

QList<CTaskPaneManagerPrivate::Item>::iterator CTaskPaneManagerPrivate::noItem()
{
    return m_items.end();
}

void CTaskPaneManager::addItem(QWidget *w, const QIcon &is, const QString &text)
{
    if (w && (d->findItem(w) == d->noItem())) {
        CTaskPaneManagerPrivate::Item item;
        item.m_itemdock   = 0;
        item.m_widget     = w;
        item.m_iconset    = is;
        item.m_label      = text;
        item.m_expandible = true;
        item.m_orig_framestyle  = QFrame::NoFrame;

        kill();
        d->m_items.append(item);
        create();
    }
}

void CTaskPaneManager::removeItem(QWidget *w, bool delete_widget)
{
    QList<CTaskPaneManagerPrivate::Item>::iterator it = d->findItem(w);

    if (it != d->noItem()) {
        kill();

        d->m_items.erase(it);
        if (delete_widget)
            delete w;

        create();
    }
}

void CTaskPaneManager::setItemText(QWidget *w, const QString &txt)
{
    QList<CTaskPaneManagerPrivate::Item>::iterator it = d->findItem(w);

    if (it != d->noItem()) {
        (*it).m_label = txt;

        kill();
        create();
    }
}


QString CTaskPaneManager::itemText(QWidget *w) const
{
    QString txt;
    QList<CTaskPaneManagerPrivate::Item>::iterator it = d->findItem(w);

    if (it != d->noItem())
        txt = (*it).m_label;

    return txt;
}


bool CTaskPaneManager::isItemVisible(QWidget *w) const
{
    QList<CTaskPaneManagerPrivate::Item>::iterator it = d->findItem(w);

    if (it != d->noItem())
        return (*it).m_visible;

    return false;
}

void CTaskPaneManager::setItemVisible(QWidget *w, bool visible)
{
    QList<CTaskPaneManagerPrivate::Item>::iterator it = d->findItem(w);

    if (it != d->noItem()) {
        if (visible != (*it).m_visible) {
            (*it).m_visible = visible;

            if (d->m_mode == Modern)
                d->m_taskpane->setExpanded(w, visible);
            else
                (*it).m_itemdock->setShown(visible);
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
        for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it) {
            CTaskPaneManagerPrivate::Item &item = *it;

            if (qobject_cast <QFrame *> (item.m_widget)) {
                QFrame *f = static_cast <QFrame *>(item.m_widget);
                f->setFrameStyle(item.m_orig_framestyle);
            }

            d->m_taskpane->removeItem(item.m_widget, false);
        }
        delete d->m_panedock;
        d->m_panedock = 0;
        d->m_taskpane = 0;
    }
    else {
        for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it) {
            CTaskPaneManagerPrivate::Item &item = *it;

            disconnect(item.m_itemdock, SIGNAL(visibilityChanged(bool)), this, SLOT(dockVisibilityChanged(bool)));

            item.m_widget->hide();
            item.m_widget->setParent(0);

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

        d->m_taskpane = new CTaskPane(0);
        d->m_panedock->setWidget(d->m_taskpane);

        for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it) {
            CTaskPaneManagerPrivate::Item &item = *it;

            if (qobject_cast <QFrame *> (item.m_widget)) {
                QFrame *f = static_cast <QFrame *>(item.m_widget);

                item.m_orig_framestyle = f->frameStyle();
                f->setFrameStyle(QFrame::NoFrame);
            }

            d->m_taskpane->addItem(item.m_widget, item.m_iconset, item.m_label, item.m_expandible);
            item.m_widget->show();
            d->m_taskpane->setExpanded(item.m_widget, item.m_visible);
        }
        d->m_taskpane->setFixedWidth(d->m_taskpane->sizeHint().width());
        d->m_taskpane->show();
        d->m_panedock->show();

        connect(d->m_taskpane, SIGNAL(itemVisibilityChanged(QWidget *, bool)), this, SLOT(itemVisibilityChanged(QWidget *, bool)));
    }
    else {
        for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it) {
            CTaskPaneManagerPrivate::Item &item = *it;

            item.m_itemdock = new QDockWidget(item.m_label, d->m_mainwindow);
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
    QDockWidget *dock = qobject_cast <QDockWidget *> (sender());

    for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it) {
        CTaskPaneManagerPrivate::Item &item = *it;

        if (item.m_itemdock == dock) {
            item.m_visible = b;
            break;
        }
    }
}

void CTaskPaneManager::itemVisibilityChanged(QWidget *w, bool b)
{
    for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it) {
        CTaskPaneManagerPrivate::Item &item = *it;

        if (item.m_widget == w) {
            item.m_visible = b;
            break;
        }
    }
}

QMenu *CTaskPaneManager::createItemVisibilityMenu() const
{
    QMenu *m = new QMenu(d->m_mainwindow);
// m->setCheckable ( true );
    connect(m, SIGNAL(aboutToShow()), this, SLOT(itemMenuAboutToShow()));
    return m;
}

void CTaskPaneManager::itemMenuAboutToShow()
{
#if 0
    QPopupMenu *m = ::qt_cast <QPopupMenu *> (sender());

    if (m) {
        m->clear();

        bool allon = true;

        int index = 0;
        for (QList<CTaskPaneManagerPrivate::Item>::iterator it = d->m_items.begin(); it != d->m_items.end(); ++it, index++) {
            int id = m->insertItem((*it).m_label, this, SLOT(itemMenuActivated(int)));
            m->setItemParameter(id, index);

            bool vis = isItemVisible((*it).m_widget);
            if (d->m_mode == Modern) {
                bool pdvis = d->m_panedock->isVisible();
                vis &= pdvis;
                m->setItemEnabled(id, pdvis);
            }
            m->setItemChecked(id, vis);
            allon &= vis;
        }
        m->insertSeparator();
        int allid = m->insertItem(tr("All"), this, SLOT(itemMenuActivated(int)));

        if (d->m_mode == Modern)
            allon = d->m_panedock->isVisible();

        m->setItemChecked(allid, allon);
        m->setItemParameter(allid, allon ? -2 : -1);

        m->insertSeparator();

        int modernid = m->insertItem(tr("Modern (fixed)"), this, SLOT(itemMenuActivated(int)));
        m->setItemChecked(modernid, d->m_mode == Modern);
        m->setItemParameter(modernid, -3);

        int classicid = m->insertItem(tr("Classic (moveable)"), this, SLOT(itemMenuActivated(int)));
        m->setItemChecked(classicid, d->m_mode != Modern);
        m->setItemParameter(classicid, -4);
    }
#endif
}

void CTaskPaneManager::itemMenuActivated(int id)
{
#if 0
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
#endif
}


class TaskPaneAction : public QAction {
public:
    TaskPaneAction(CTaskPaneManager *tpm, QObject *parent, const char *name = 0)
            : QAction(parent), m_tpm(tpm)
    {
        setObjectName(name);
    }
    /*
     virtual bool addTo ( QWidget *w )
     {
      if ( w->inherits ( "QPopupMenu" )) {
       int id = static_cast <QPopupMenu *> ( w )->insertItem ( menuText ( ), m_tpm->createItemVisibilityMenu ( ));
       m_update_menutexts.insert ( static_cast <QPopupMenu *> ( w ), id );
       return true;
      }
      return false;
     }
     virtual void setText ( const QString &txt )
     {
      QActionGroup::setText ( txt );

      for ( QMap <QPopupMenu *, int>::const_iterator it = m_update_menutexts.begin ( ); it != m_update_menutexts.end ( ); ++it )
       it.key ( )->changeItem ( it.data ( ), menuText ( ));
     }
    */
private:
    CTaskPaneManager *m_tpm;
// QMap <QPopupMenu *, int> m_update_menutexts;
};

QAction *CTaskPaneManager::createItemVisibilityAction(QObject *parent, const char *name) const
{
    return new TaskPaneAction(const_cast<CTaskPaneManager *>(this), parent, name);
}

// vvv PRIVATE vvv

class CTaskGroup : public QWidget {
private:
    friend class CTaskPane;

    CTaskGroup(CTaskPane *parent)
            : QWidget(parent)
    {
        m_bar = parent;
        m_expanded = true;
        m_expandible = true;
        m_hot = false;
        m_widget = 0;

        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
    }

public:
    void setWidget(QWidget *w)
    {
        m_widget = w;
        w->setParent(m_bar);
        m_bar->recalcLayout();
    }

    QWidget *widget() const
    {
        return m_widget;
    }

    void setText(const QString &txt)
    {
        if (txt != m_text) {
            m_text = txt;
            m_bar->recalcLayout();
            update();
        }
    }

    QString text() const
    {
        return m_text;
    }

    void setIcon(const QIcon &is)
    {
        m_is = is;
        m_bar->recalcLayout();
        update();
    }

    QIcon icon() const
    {
        return m_is;
    }

    void setExpandible(bool b)
    {
        if (m_expandible != b) {
            m_expandible = b;
            m_bar->recalcLayout();
            update();
        }
    }

    bool isExpandible() const
    {
        return m_expandible;
    }

    void setExpanded(bool b)
    {
        if (m_expanded != b) {
            m_expanded = b;
            m_bar->recalcLayout();
            update();
            m_bar->update();
            emit m_bar->itemVisibilityChanged(m_widget, b);
        }
    }

    bool isExpanded() const
    {
        return m_expanded;
    }

    void paintEvent(QPaintEvent * /*e*/)
    {
        QPainter p(this);

        m_bar->drawHeaderBackground(&p, this, m_hot);
        m_bar->drawHeaderContents(&p, this, m_hot);
    }

    virtual QSize sizeHint() const
    {
        return m_bar->sizeForGroup(this);
    }

protected:
    virtual void enterEvent(QEvent *)
    {
        m_hot = true;
        update();
    }

    virtual void leaveEvent(QEvent *)
    {
        m_hot = false;
        update();
    }

    void mouseReleaseEvent(QMouseEvent *e)
    {
        if (e->button() == Qt::LeftButton) {
            setExpanded(!isExpanded());
            e->accept();
        }
    }

protected:
    bool m_expandible  : 1;
    bool m_expanded    : 1;
    bool m_hot         : 1;

    int m_leadin;
    int m_leadout;

    QWidget *  m_widget;
    QIcon      m_is;
    QString    m_text;

    CTaskPane *m_bar;
};



//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################


CTaskPane::CTaskPane(QWidget *parent)
        : QWidget(parent)
{
    m_margins = QRect(6, 6, 1, 1);
    m_groups = new QList <CTaskGroup *>;
    //m_groups->setAutoDelete ( true );

    updateFont();
    updatePalette();

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

CTaskPane::~CTaskPane()
{
}

void CTaskPane::addItem(QWidget *w, const QIcon &is, const QString &txt, bool expandible)
{
    CTaskGroup *g = new CTaskGroup(this);
    g->setIcon(is);
    g->setText(txt);
    g->setExpandible(expandible);
    g->setWidget(w);
    g->show();

    m_groups->append(g);
    recalcLayout();
}

void CTaskPane::removeItem(QWidget *w, bool delete_widget)
{
    for (int i = 0; i < m_groups->count(); i++) {
        if (w == m_groups->at(i)->widget()) {
            m_groups->removeAt(i);

            if (delete_widget)
                delete w;
            else
                w->setParent(0);
        }
    }
}

bool CTaskPane::isExpanded(QWidget *w) const
{
    bool exp = false;

    foreach(CTaskGroup *g, *m_groups) {
        if (w == g->widget()) {
            exp = g->isExpanded();
            break;
        }
    }
    return exp;
}

void CTaskPane::setExpanded(QWidget *w, bool exp)
{
    foreach(CTaskGroup *g, *m_groups) {
        if (w == g->widget()) {
            g->setExpanded(exp);
            break;
        }
    }
}

void CTaskPane::drawHeaderBackground(QPainter *p, CTaskGroup *g, bool /*hot*/)
{
    const QPalette pal = palette();
    QRect r = g->rect();

    int offy = r.height() - (m_vspace + m_xh + m_vspace);

    p->fillRect(r.x(), r.y() + offy, r.width(), r.height() - offy, m_tgrad);
}

void CTaskPane::drawHeaderContents(QPainter *p, CTaskGroup *g, bool hot)
{
    const QPalette pal = palette();

    int x, y, w, h;
    g->rect().getRect(&x, &y, &w, &h);

    int offx = m_hspace;

    if (!g->icon().isNull()) {
        g->icon().paint(p, QRect(x + offx, y, 22, 22), Qt::AlignCenter, hot ? QIcon::Active : QIcon::Normal, QIcon::On);
      //  QPixmap pix = g->icon().pixmap(22, y + (h > pix.height() ? (h - pix.height()) / 2 : 0), );

        offx += (22 + m_hspace);
    }

    QRect tr(x + offx, y + h - (2 * m_vspace + m_xh), w - (offx + m_hspace), 2 * m_vspace + m_xh);

    p->setFont(m_font);
    p->setPen(CUtility::contrastColor(hot ? CUtility::contrastColor(m_tcol, 0.1f) : m_tcol, 0.5f));
    p->drawText(tr.translated(1,1), Qt::AlignLeft | Qt::AlignVCenter, g->text());

    p->setPen(hot ? CUtility::contrastColor(m_tcol,0.1f) : m_tcol);
    p->drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, g->text());

    if (g->isExpandible()) {
        QRect ar(tr.right() - m_arrow, tr.top() + (tr.height() - m_arrow) / 2, m_arrow, m_arrow);

        for (int i = 0; i < m_arrow; i++) {
            int off = i / 2;

            if (g->isExpanded())
                p->drawLine(ar.left() + off, ar.top() + i, ar.right() - off, ar.top() + i);
            else
                p->drawLine(ar.left() + i, ar.top() + off, ar.left() + i, ar.bottom() - off);
        }
    }
    if (hasFocus()) {
        QStyleOptionFocusRect opt;
        opt.initFrom(this);
        opt.rect = tr;

        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &opt, p, this);
    }
}

QSize CTaskPane::sizeHint() const
{
    int w = 0, h = 0;

    w += (m_margins.left() + m_margins.right());
    h += (m_margins.top() + m_margins.bottom());

    int iw = 0;

    bool first = true;

    foreach(CTaskGroup *g, *m_groups) {
        if (!first)
            h += m_xh / 2;
        first = false;

        QSize gs = g->sizeHint();

        if (gs.width() > iw)
            iw = gs.width();

        h += gs.height();
        h++;

        QWidget *content = g->widget();

        if (content /*&& g->isExpanded()*/) {
            QSize cs = content->minimumSizeHint();

            if (cs.isEmpty())
                cs = content->minimumSize();

            if ((cs.width() + 4) > iw)
                iw = cs.width() + 4;

            h += cs.height();
            h++;
        }
    }
    w += iw;

    return QSize(w, h);
}

void CTaskPane::recalcLayout()
{
    updateGeometry();

    int offx = m_margins.left();
    int offy = m_margins.top();
    int w = width() - offx - m_margins.right();
    int h = height() - offy - m_margins.bottom();

    if (w <= 0 || h <= 0)
        return;

    bool first = true;

    foreach(CTaskGroup *g, *m_groups) {
        if (!first)
            offy += m_xh / 2;
        first = false;

        int gh = g->sizeHint().height();

        g->setGeometry(offx, offy, w, gh);
        offy += gh;
        offy++;

        QWidget *content = g->widget();

        if (content) {
            if (g->isExpandible() && g->isExpanded()) {
                QSize cs = content->minimumSizeHint();

                if (cs.isEmpty())
                    cs = content->minimumSize();

                int ch = cs.height();

                content->setGeometry(offx + 2, offy + 1, w - 4, ch);
                offy += ch;
                offy+=2;
                content->show();
            }
            else
                content->hide();
        }
    }
}

QSize CTaskPane::sizeForGroup(const CTaskGroup *g) const
{
    QSize s;
    QFontMetrics fm(m_font);

    QSize icons(0, 0);
    if (!g->icon().isNull())
        icons = g->icon().pixmap(32).size();
    int tw = fm.width(g->text()) + 1;
    int th = 2 * m_vspace + m_xh + 1;

    s.setWidth(m_hspace + icons.width() + (icons.width() ? m_hspace : 0) + tw + (tw ? m_hspace : 0) + (g->isExpandible() ? m_arrow + m_hspace : 0));
    s.setHeight(qMax(icons.height(), th));

    return s;
}

void CTaskPane::resizeEvent(QResizeEvent *re)
{
    QWidget::resizeEvent(re);
    updateBackground();
    recalcLayout();
}

void CTaskPane::updateFont()
{
    m_font = font();
    m_font.setBold(true);

    QFontMetrics fm(m_font);

    m_xw = fm.width("x");
    m_xh = fm.height();

    m_arrow = (m_xh / 2) | 1;
    m_hspace = m_xw;
    m_vspace = m_xh / 6;

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
        cap2 = CUtility::contrastColor(cap2, 0.2f);
    else
        cap2.setRgb(colorref2qrgb(GetSysColor(COLOR_GRADIENTACTIVECAPTION)));

    if (CUtility::colorDifference(cap1, cap2) < 0.1f)
        cap2 = CUtility::contrastColor(cap2, 0.2f);

    m_bgrad = QLinearGradient(1, 0, width()-2, 0);
    m_bgrad.setColorAt(0, cap1);
    m_bgrad.setColorAt(1, cap2);

    QColor cap1a = cap1;
    cap1a.setAlphaF(0.f);

    m_tgrad = QLinearGradient(0, 0, width() - m_margins.left() - m_margins.right(), m_xh);
    m_tgrad.setColorAt(0,   cap2);
    m_tgrad.setColorAt(0.5, cap2);
    m_tgrad.setColorAt(1,   cap1a);

    m_lcol = cap1;
    m_rcol = cap2;
    m_tcol = text;
    m_fcol = CUtility::gradientColor(CUtility::gradientColor(cap1, cap2), palette().color(QPalette::Base), .5f);

#elif defined( Q_WS_MACX )
    // leopard title bar gradient
    QColor cap1(192, 192, 192);
    QColor cap2(150, 150, 150);
    m_bgrad = QLinearGradient(1, 0, width()-2, 0);
    m_bgrad.setColorAt(0, cap1);
    m_bgrad.setColorAt(1, cap2);

    QColor cap1a = cap1;
    cap1a.setAlphaF(0.f);

    m_tgrad = QLinearGradient(1, 0, width()-2, 0);
    m_tgrad.setColorAt(0, cap2);
    m_tgrad.setColorAt(0.5, cap2);
    m_tgrad.setColorAt(1, cap1a);

    m_lcol = QColor(220, 220, 220);
    m_rcol = QColor(64, 64, 64);
    m_tcol = Qt::black;
    m_fcol = CUtility::gradientColor(CUtility::gradientColor(cap1, cap2), palette().color(QPalette::Base), .5f);
#else
    // used the same as Mac Os X leopard title bar gradient
    m_bgrad = QLinearGradient(1, 0, width()-2, height());
    m_bgrad.setColorAt(0, QColor(192,192,192));
    m_bgrad.setColorAt(1, QColor(150,150,150));
    m_lcol = QColor(220, 220, 220);
    m_rcol = QColor(64, 64, 64);
#endif
}

void CTaskPane::updatePalette()
{
    updateBackground();
    update();
}


void CTaskPane::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange) {
        updateFont();
    }
    else if (e->type() == QEvent::PaletteChange) {
        updatePalette();
    }
    else {
        QWidget::changeEvent(e);
    }
}

void CTaskPane::paintEvent(QPaintEvent *)
{
    QPalette pal = palette();
    QPainter p(this);

    int w = width();
    int h = height();

    p.fillRect(1, 0, w-2, h, m_bgrad);
    p.setPen(m_lcol);
    p.drawLine(0, 0, 0, h-1);
    p.setPen(m_rcol);
    p.drawLine(w-1, 0, w-1, h-1);
    p.setPen(m_fcol);

    foreach(CTaskGroup *g, *m_groups) {
        if (g->widget() && g->isExpanded()) {
            p.drawRect(g->widget()->geometry().adjusted(-2, -2, 1, 1));
        }
        else {
            QRect gr = g->geometry();
            p.drawLine(gr.x(), gr.bottom() + 1, gr.right(), gr.bottom() + 1);
        }
    }

    p.end();
}


#include "ctaskpanemanager.moc"
