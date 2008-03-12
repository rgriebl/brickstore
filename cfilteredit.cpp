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

#include <QToolButton>
#include <QStyle>
#include <QTimer>
#include <QPainter>
#include <QStyleOptionFrame>
#include <QMenu>

#include <QtDebug>

#include "cfilteredit.h"

#if defined( Q_WS_MAC )
//#define MAC_USE_NATIVE_SEARCHFIELD
#endif

#if defined( MAC_USE_NATIVE_SEARCHFIELD )
#include <Carbon/Carbon.h>
typedef QWidget superclass;
#else
#include <QLineEdit>
typedef QLineEdit superclass;
#endif

class CFilterEditPrivate : public superclass {
    Q_OBJECT
    
    friend class CFilterEdit;
    
public:
    CFilterEditPrivate(CFilterEdit *parent);
    ~CFilterEditPrivate();

    virtual QSize sizeHint() const; 
    virtual QSize minimumSizeHint() const; 

protected slots:
    void checkText(const QString &);
    void timerTick();

private:
    CFilterEdit *q;
    QMenu *      m_menu;
    QTimer *     m_timer;
    QString      m_idletext;

#if defined( MAC_USE_NATIVE_SEARCHFIELD )
private:

    HIViewRef    m_hisearch;

#else

protected:
    virtual void resizeEvent(QResizeEvent *);
    virtual void paintEvent(QPaintEvent *e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

private:
    void doLayout(bool setpositions);

private:
    QLineEdit   *w_edit;
    QToolButton *w_menu;
    QToolButton *w_clear;

#endif
};


CFilterEditPrivate::CFilterEditPrivate(CFilterEdit *parent)
    : superclass(parent), q(parent)
{
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::ComboBox));
    setFocusPolicy(Qt::StrongFocus);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(400);

    m_menu = 0;
    
//    setBackgroundRole(QPalette::Base);
//    setAutoFillBackground(true);

    connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTick()));

#if defined( MAC_USE_NATIVE_SEARCHFIELD )
    // Create a native search field and pass its window id to QWidget::create.
    HISearchFieldCreate(NULL/*bounds*/, kHISearchFieldAttributesSearchIcon | kHISearchFieldAttributesCancel, NULL/*menu ref*/, NULL, &m_hisearch);
    create(reinterpret_cast<WId>(m_hisearch));
    //setAttribute(Qt::WA_MacShowFocusRect);
    setAttribute(Qt::WA_MacNormalSize);
    
#else
    w_menu = new QToolButton(this);
    w_menu->setCursor(Qt::ArrowCursor);
    w_menu->setFocusPolicy(Qt::NoFocus);
//    w_menu->setAutoRaise(true);

    w_clear = new QToolButton(this);
    w_clear->setCursor(Qt::ArrowCursor);
    w_clear->setFocusPolicy(Qt::NoFocus);
    w_clear->hide();

    w_menu->setStyleSheet("QToolButton { border: none; padding: 0px; }\n"
                          "QToolButton::menu-indicator { left: -30px }");
    w_clear->setStyleSheet("QToolButton { border: none; padding: 0px; }");

    connect(w_clear, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(checkText(const QString&)));

    doLayout(false);
#endif
    
}

CFilterEditPrivate::~CFilterEditPrivate()
{
#if defined( MAC_USE_NATIVE_SEARCHFIELD )
    CFRelease(m_hisearch);
#endif
}


QSize CFilterEditPrivate::sizeHint() const
{
#if defined( MAC_USE_NATIVE_SEARCHFIELD )
    EventRef event;
    HIRect optimalBounds;
    CreateEvent(0, kEventClassControl,
        kEventControlGetOptimalBounds,
        GetCurrentEventTime(),
        kEventAttributeUserEvent, &event);

    SendEventToEventTargetWithOptions(event,
        HIObjectGetEventTarget(HIObjectRef(winId())),
        kEventTargetDontPropagate);

    GetEventParameter(event,
        kEventParamControlOptimalBounds, typeHIRect,
        0, sizeof(HIRect), 0, &optimalBounds);

    ReleaseEvent(event);
    return QSize(optimalBounds.size.width + 30 * fontMetrics().width(QLatin1Char('x')), // make it a bit wider.
                 optimalBounds.size.height-8);
#else
    return superclass::sizeHint();
#endif
}

QSize CFilterEditPrivate::minimumSizeHint() const
{
#if defined( MAC_USE_NATIVE_SEARCHFIELD )
    EventRef event;
    HIRect optimalBounds;
    CreateEvent(0, kEventClassControl,
        kEventControlGetOptimalBounds,
        GetCurrentEventTime(),
        kEventAttributeUserEvent, &event);

    SendEventToEventTargetWithOptions(event,
        HIObjectGetEventTarget(HIObjectRef(winId())),
        kEventTargetDontPropagate);

    GetEventParameter(event,
        kEventParamControlOptimalBounds, typeHIRect,
        0, sizeof(HIRect), 0, &optimalBounds);

    ReleaseEvent(event);
    return QSize(optimalBounds.size.width + fontMetrics().maxWidth(),
                 optimalBounds.size.height-8);
#else
    return superclass::minimumSizeHint();
#endif
}


void CFilterEditPrivate::checkText(const QString &str)
{
#if !defined( MAC_USE_NATIVE_SEARCHFIELD )
    w_clear->setVisible(!str.isEmpty());
#else
    Q_UNUSED(str)
#endif
    m_timer->start();
}

void CFilterEditPrivate::timerTick()
{
    QString filtertext;

#if !defined( MAC_USE_NATIVE_SEARCHFIELD )
    filtertext = text();
#else
#endif

    emit q->textChanged(filtertext);
}

QString CFilterEdit::text() const
{
#if !defined( MAC_USE_NATIVE_SEARCHFIELD )
    return d->text();
#else
    return QString();
#endif
}

void CFilterEdit::setText(const QString &txt)
{
#if !defined( MAC_USE_NATIVE_SEARCHFIELD )
    d->setText(txt);
#else
#endif
}

#if !defined( MAC_USE_NATIVE_SEARCHFIELD )

void CFilterEditPrivate::doLayout(bool setpositionsonly)
{
    QSize ms = w_menu->sizeHint();
    QSize cs = w_clear->sizeHint();
    int fw = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

    if (!setpositionsonly) {
        setStyleSheet(QString("QLineEdit { padding-left: %1px; padding-right: %2px; } ")
                      .arg(ms.width() + fw + 1)
                      .arg(cs.width() + fw + 1));

        QSize mins = QLineEdit::minimumSizeHint();
        setMinimumSize(mins.width() + ms.width() + cs.width() + fw * 4 + 4,
                       qMax(mins.height(), qMax(cs.height(), ms.height()) + fw * 2 + 2));
    }
    w_menu->move(rect().left() + fw, (rect().bottom() + 1 - ms.height())/2);
    w_clear->move(rect().right() - fw - cs.width(), (rect().bottom() + 1 - cs.height())/2);
}

void CFilterEditPrivate::resizeEvent(QResizeEvent *)
{
    doLayout(true);
}

void CFilterEditPrivate::paintEvent(QPaintEvent *e)
{
    QLineEdit::paintEvent(e);

    if (!hasFocus() && !m_idletext.isEmpty() && text().isEmpty()) {
        QPainter p(this);
        QPen tmp = p.pen();
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));

        //FIXME: fugly alert!
        // qlineedit uses an internal qstyleoption set to figure this out
        // and then adds a hardcoded 2 pixel interior to that.
        // probably requires fixes to Qt itself to do this cleanly
        QStyleOptionFrame opt;
        opt.init(this);
        opt.rect = contentsRect();
        opt.lineWidth = hasFrame() ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth) : 0;
        opt.midLineWidth = 0;
        opt.state |= QStyle::State_Sunken;
        QRect cr = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
        cr.setLeft(cr.left() + 2);
        cr.setRight(cr.right() - 2);

        p.drawText(cr, Qt::AlignLeft|Qt::AlignVCenter, m_idletext);
        p.setPen(tmp);
    }
}

void CFilterEditPrivate::focusInEvent(QFocusEvent *e)
{
    if (!m_idletext.isEmpty())
        update();
    QLineEdit::focusInEvent(e);
}

void CFilterEditPrivate::focusOutEvent(QFocusEvent *e)
{
    if (!m_idletext.isEmpty())
        update();
    QLineEdit::focusOutEvent(e);
}

#endif


CFilterEdit::CFilterEdit(QWidget *parent)
    : QWidget(parent)
{
    d = new CFilterEditPrivate(this);
    
    setSizePolicy(d->sizePolicy());
    setFocusProxy(d);
}

void CFilterEdit::setMenuIcon(const QIcon &icon)
{
#if !defined( MAC_USE_NATIVE_SEARCHFIELD )
    d->w_menu->setIcon(icon);
    d->doLayout(false);
#endif
}

void CFilterEdit::setClearIcon(const QIcon &icon)
{
#if !defined( MAC_USE_NATIVE_SEARCHFIELD )
    d->w_clear->setIcon(icon);
    d->doLayout(false);
#endif
}


void CFilterEdit::setMenu(QMenu *menu)
{
#if defined( MAC_USE_NATIVE_SEARCHFIELD )
    // Use a Qt menu for the search field menu.
    HISearchFieldSetSearchMenu(d->m_hisearch, menu ? menu->macMenu(0) : 0);
#else
    d->w_menu->setMenu(menu);
    d->w_menu->setPopupMode(QToolButton::InstantPopup);
#endif
    if (d->m_menu)
        delete d->m_menu;
    d->m_menu = menu;
}

QMenu *CFilterEdit::menu() const
{
    return d->m_menu;
}

void CFilterEdit::setIdleText(const QString &str)
{
    d->m_idletext = str;

#if defined( MAC_USE_NATIVE_SEARCHFIELD )
    const CFStringRef cfs = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
            reinterpret_cast<const UniChar *>(str.unicode()), str.length(), kCFAllocatorNull);
    HISearchFieldSetDescriptiveText(m_hisearch, cfs);
    CFRelease(cfs);    
#else
    d->update();
#endif    
}

QSize CFilterEdit::sizeHint() const
{
#if defined( xMAC_USE_NATIVE_SEARCHFIELD )
    return d->sizeHint() + QSize(6, 6);
#else
    return d->sizeHint();
#endif
}

QSize CFilterEdit::minimumSizeHint() const
{
#if defined( xMAC_USE_NATIVE_SEARCHFIELD )
    return d->minimumSizeHint() + QSize(6, 6);
#else
    return d->minimumSizeHint();
#endif
}

void CFilterEdit::resizeEvent(QResizeEvent *)
{
#if defined( xMAC_USE_NATIVE_SEARCHFIELD )
    d->setGeometry(2, 4, width() - 6, height() - 6);
#else
    d->setGeometry(0, 0, width(), height());
#endif
}

#include "cfilteredit.moc"
