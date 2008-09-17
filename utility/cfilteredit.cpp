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
#include <QLineEdit>
#include <QIcon>
#include <QStyle>
#include <QTimer>
#include <QPainter>
#include <QStyleOptionFrame>
#include <QMenu>
#include <QtDebug>

#include "cfilteredit.h"

#if defined( Q_WS_MAC )
//#  define MAC_USE_NATIVE_SEARCHFIELD
#else
#  undef MAC_USE_NATIVE_SEARCHFIELD
#endif

#if defined( MAC_USE_NATIVE_SEARCHFIELD )

#include <Carbon/Carbon.h>

// copied and simplified to static functions from private/qcore_mac_p.h
class QCFString {
public:
    static QString toQString(CFStringRef cfstr);
    static CFStringRef toCFStringRef(const QString &str);
};

typedef QWidget CFilterEditPrivateBase;

#else
#  include <QLineEdit>
typedef QLineEdit CFilterEditPrivateBase;

#endif


class CFilterEditPrivate : public CFilterEditPrivateBase {
    Q_OBJECT

    friend class CFilterEdit;

public:
    CFilterEditPrivate(CFilterEdit *parent);
    ~CFilterEditPrivate();

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

    virtual void setMenu(QMenu *menu);
    virtual void setText(const QString &str);
    virtual QString text() const;
    virtual void setIdleText(const QString &str);
    virtual QString idleText() const;

private slots:
    void timerTick();

private:
    CFilterEdit *q;
    QTimer *     m_timer;
    QMenu *      m_menu;

#if defined(MAC_USE_NATIVE_SEARCHFIELD)
private:
    static OSStatus macEventHandler(EventHandlerCallRef handler, EventRef event, void *data);
    HIViewRef    m_hisearch;

#else
protected:
    virtual void resizeEvent(QResizeEvent *);
    virtual void paintEvent(QPaintEvent *e);
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);

private slots:
    void checkText(const QString &);

private:
    void doLayout(bool setpositions);

    QString      m_idletext;
    QToolButton *w_menu;
    QToolButton *w_clear;
#endif
};




CFilterEditPrivate::CFilterEditPrivate(CFilterEdit *parent)
    : CFilterEditPrivateBase(parent), q(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(400);

    m_menu = 0;

    connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTick()));

#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::LineEdit));
    setFocusPolicy(Qt::StrongFocus);
    q->setAttribute(Qt::WA_MacShowFocusRect);

    // Create a native search field and pass its window id to QWidget::create.
    HISearchFieldCreate(NULL/*bounds*/, kHISearchFieldAttributesSearchIcon | kHISearchFieldAttributesCancel, NULL/*menu ref*/, NULL, &m_hisearch);
    create(reinterpret_cast<WId>(m_hisearch));
    //setAttribute(Qt::WA_MacShowFocusRect);
    setAttribute(Qt::WA_MacNormalSize);

    EventTypeSpec mac_events[] = { { kEventClassSearchField, kEventSearchFieldCancelClicked },
                                   { kEventClassTextField, kEventTextDidChange },
                                   { kEventClassTextField, kEventTextAccepted } };

    HIViewInstallEventHandler(m_hisearch, macEventHandler, GetEventTypeCount(mac_events), mac_events, this, NULL);
#else
    w_menu = new QToolButton(this);
    w_menu->setIcon(QIcon(":/images/filter_edit_menu"));
    w_menu->setCursor(Qt::ArrowCursor);
    w_menu->setFocusPolicy(Qt::NoFocus);
//    w_menu->setAutoRaise(true);

    w_clear = new QToolButton(this);
    w_clear->setIcon(QIcon(":/images/filter_edit_clear"));
    w_clear->setCursor(Qt::ArrowCursor);
    w_clear->setFocusPolicy(Qt::NoFocus);
    w_clear->hide();

    w_menu->setStyleSheet("QToolButton { border: none; padding: 0px; }\n"
                          "QToolButton::menu-indicator { left: -30px }");
    w_clear->setStyleSheet("QToolButton { border: none; padding: 0px; }");

    connect(w_clear, SIGNAL(clicked()), q, SLOT(clear()));
    connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(checkText(const QString&)));

    doLayout(false);
#endif
}

CFilterEditPrivate::~CFilterEditPrivate()
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    CFRelease(m_hisearch);
#endif
}


QSize CFilterEditPrivate::sizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    return minimumSizeHint() + QSize(10 * fontMetrics().width(QLatin1Char('x')), 0);
#else
    return QLineEdit::sizeHint() + QSize(0, 0); //TODO: image sizes
#endif
}

QSize CFilterEditPrivate::minimumSizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
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
    return QSize(optimalBounds.size.width + fontMetrics().maxWidth() + 2*optimalBounds.size.height,
                 optimalBounds.size.height-8);
#else
    return QLineEdit::minimumSizeHint() + QSize(0, 0); //TODO: image sizes
#endif
}


void CFilterEditPrivate::timerTick()
{
    emit q->textChanged(text());
}

void CFilterEditPrivate::setText(const QString &txt)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    HIViewSetText(m_hisearch, QCFString::toCFStringRef(txt));
#else
    QLineEdit::setText(txt);
#endif
    m_timer->stop();
    emit q->textChanged(txt);
}

QString CFilterEditPrivate::text() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    CFStringRef ref = HIViewCopyText(m_hisearch);
    QString str = QCFString::toQString(ref);
    CFRelease(ref);
    return str;
#else
    return QLineEdit::text();
#endif
}

void CFilterEditPrivate::setMenu(QMenu *menu)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    HISearchFieldSetSearchMenu(m_hisearch, menu ? menu->macMenu(0) : 0);
#else
    w_menu->setMenu(menu);
    w_menu->setPopupMode(QToolButton::InstantPopup);

#endif
    delete m_menu;
    m_menu = menu;
}

void CFilterEditPrivate::setIdleText(const QString &str)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    CFStringRef ref = QCFString::toCFStringRef(str);
    HISearchFieldSetDescriptiveText(m_hisearch, ref);
    CFRelease(ref);
#else
    m_idletext = str;
    if (QLineEdit::text().isEmpty())
        update();
#endif
}

QString CFilterEditPrivate::idleText() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    CFStringRef ref;
    HISearchFieldCopyDescriptiveText(m_hisearch, &ref);
    QString str = QCFString::toQString(ref);
    CFRelease(ref);
    return str;
#else
    return m_idletext;
#endif
}


// ##########################################################################



CFilterEdit::CFilterEdit(QWidget *parent)
    : QWidget(parent)
{
    d = new CFilterEditPrivate(this);

    setSizePolicy(d->sizePolicy());
    setFocusProxy(d);
}

void CFilterEdit::clear()
{
    d->setText(QString());
}

void CFilterEdit::setText(const QString &str)
{
    d->setText(str);
}

QString CFilterEdit::text() const
{
    return d->text();
}

void CFilterEdit::setMenu(QMenu *menu)
{
    d->setMenu(menu);
}

QMenu *CFilterEdit::menu() const
{
    return d->m_menu;
}

void CFilterEdit::setIdleText(const QString &str)
{
    d->setIdleText(str);
}

QString CFilterEdit::idleText() const
{
    return d->idleText();
}

QSize CFilterEdit::sizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    return d->sizeHint() + QSize(6, 6); //focus border
#else
    return d->sizeHint();
#endif
}

QSize CFilterEdit::minimumSizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    return d->minimumSizeHint() + QSize(6, 6); //focus border
#else
    return d->minimumSizeHint();
#endif
}

void CFilterEdit::resizeEvent(QResizeEvent *)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    d->setGeometry(2, 4, width() - 6, height() - 6);
#else
    d->setGeometry(0, 0, width(), height());
#endif
}


#if defined(MAC_USE_NATIVE_SEARCHFIELD)

OSStatus CFilterEditPrivate::macEventHandler(EventHandlerCallRef, EventRef event, void *data)
{
    CFilterEditPrivate *that = static_cast<CFilterEditPrivate *>(data);

    if (!that)
        return eventNotHandledErr;

    switch (GetEventClass(event)) {
    case kEventClassTextField:
        switch (GetEventKind(event)) {
        case kEventTextDidChange:
            emit that->m_timer->start();
            break;
        case kEventTextAccepted:
            emit that->q->returnPressed();
            break;
        default:
            break;
        }
        break;

    case kEventClassSearchField:
        switch (GetEventKind(event)) {
        case kEventSearchFieldCancelClicked:
            that->q->clear();
            break;
        case kEventSearchFieldSearchClicked:
            emit that->q->returnPressed();
            break;
        default:
            break;
        }
        break;
    }
    return noErr;
}

#else // MAC_USE_NATIVE_SEARCHFIELD

void CFilterEditPrivate::checkText(const QString &str)
{
    w_clear->setVisible(!str.isEmpty());
    m_timer->start();
}

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

#endif // MAC_USE_NATIVE_SEARCHFIELD

#include "cfilteredit.moc"

