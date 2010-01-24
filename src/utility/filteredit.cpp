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
#include <QMouseEvent>
#include <QMenu>
#include <QtDebug>

#include "filteredit.h"

//#if 0
#if defined( Q_WS_MAC )
#  define MAC_USE_NATIVE_SEARCHFIELD
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

typedef QWidget FilterEditPrivateBase;

#else
#  include <QLineEdit>
typedef QLineEdit FilterEditPrivateBase;

class FilterEditButton : public QAbstractButton {
public:
    FilterEditButton(const QIcon &icon, QWidget *parent)
        : QAbstractButton(parent), m_menu(0), m_hover(false)
    {
        setIcon(icon);
        setCursor(Qt::ArrowCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(QSize(22, 22));
        resize(22, 22);
    }

    void setMenu(QMenu *menu)
    {
        m_menu = menu;
    }

    QMenu *menu() const
    {
        return m_menu;
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        icon().paint(&p, rect(), Qt::AlignCenter, isEnabled() ? (m_hover ? QIcon::Active : QIcon::Normal) : QIcon::Disabled, isDown() ? QIcon::On : QIcon::Off);
    }

    void enterEvent(QEvent *)
    {
        m_hover = true;
        update();
    }

    void leaveEvent(QEvent *)
    {
        m_hover = false;
        update();
    }

    void mousePressEvent(QMouseEvent *e)
    {
        if (m_menu && e->button() == Qt::LeftButton) {
            QWidget *p = parentWidget();
            if (p) {
                QPoint r = p->mapToGlobal(QPoint(0, p->height()));
                m_menu->exec(QPoint(r.x() + height() / 2, r.y()));
            }
            e->accept();
        }
        QAbstractButton::mousePressEvent(e);
    }

private:
    QMenu *m_menu;
    bool m_hover;
};
#endif


class FilterEditPrivate : public FilterEditPrivateBase {
    Q_OBJECT

    friend class FilterEdit;

public:
    FilterEditPrivate(FilterEdit *parent);
    ~FilterEditPrivate();

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
    FilterEdit *q;
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
    void doLayout();

    QString           m_idletext;
    FilterEditButton *w_menu;
    FilterEditButton *w_clear;
    int               m_left;
    int               m_top;
    int               m_right;
    int               m_bottom;
#endif
};




FilterEditPrivate::FilterEditPrivate(FilterEdit *parent)
    : FilterEditPrivateBase(parent), q(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(250);

    m_menu = 0;

    connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTick()));

#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::LineEdit));
    setFocusPolicy(Qt::StrongFocus);
    q->setAttribute(Qt::WA_MacShowFocusRect);

    // Create a native search field and pass its window id to QWidget::create.
    HISearchFieldCreate(NULL/*bounds*/, kHISearchFieldAttributesSearchIcon, NULL/*menu ref*/, NULL, &m_hisearch);
    create(reinterpret_cast<WId>(m_hisearch));
    //setAttribute(Qt::WA_MacShowFocusRect);
    setAttribute(Qt::WA_MacNormalSize);

    EventTypeSpec mac_events[] = { { kEventClassSearchField, kEventSearchFieldCancelClicked },
                                   { kEventClassTextField, kEventTextDidChange },
                                   { kEventClassTextField, kEventTextAccepted } };

    HIViewInstallEventHandler(m_hisearch, macEventHandler, GetEventTypeCount(mac_events), mac_events, this, NULL);
#else
    w_menu = new FilterEditButton(QIcon(":/images/filter_edit_menu"), this);
    w_clear = new FilterEditButton(QIcon(":/images/filter_edit_clear"), this);
    w_clear->hide();

    connect(w_clear, SIGNAL(clicked()), q, SLOT(clear()));
    connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(checkText(const QString&)));

    getTextMargins(&m_left, &m_top, &m_right, &m_bottom);
    doLayout();
#endif
}

FilterEditPrivate::~FilterEditPrivate()
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    CFRelease(m_hisearch);
#endif
}


QSize FilterEditPrivate::sizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    return minimumSizeHint() + QSize(10 * fontMetrics().width(QLatin1Char('x')), 0);
#else
    return QLineEdit::sizeHint() + QSize(0, 0); //TODO: image sizes
#endif
}

QSize FilterEditPrivate::minimumSizeHint() const
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


void FilterEditPrivate::timerTick()
{
    qDebug("** FILTER TEXT CHANGED **");
    emit q->textChanged(text());
}

void FilterEditPrivate::setText(const QString &txt)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    HIViewSetText(m_hisearch, QCFString::toCFStringRef(txt));
    OptionBits c = kHISearchFieldAttributesCancel;
    bool empty = txt.isEmpty();
    HISearchFieldChangeAttributes(m_hisearch, empty ? 0 : c, empty ? c : 0);
#else
    QLineEdit::setText(txt);
    doLayout();
#endif
    m_timer->stop();
    emit q->textChanged(txt);
}

QString FilterEditPrivate::text() const
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

void FilterEditPrivate::setMenu(QMenu *menu)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    HISearchFieldSetSearchMenu(m_hisearch, menu ? static_cast<OpaqueMenuRef *>(menu->macMenu(0)) : 0);
#else
    w_menu->setMenu(menu);

#endif
    delete m_menu;
    m_menu = menu;
}

void FilterEditPrivate::setIdleText(const QString &str)
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

QString FilterEditPrivate::idleText() const
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



FilterEdit::FilterEdit(QWidget *parent)
    : QWidget(parent)
{
    d = new FilterEditPrivate(this);

    setSizePolicy(d->sizePolicy());
    setFocusProxy(d);
}

void FilterEdit::clear()
{
    d->setText(QString());
}

void FilterEdit::setText(const QString &str)
{
    d->setText(str);
}

QString FilterEdit::text() const
{
    return d->text();
}

void FilterEdit::setMenu(QMenu *menu)
{
    d->setMenu(menu);
}

QMenu *FilterEdit::menu() const
{
    return d->m_menu;
}

void FilterEdit::setIdleText(const QString &str)
{
    d->setIdleText(str);
}

QString FilterEdit::idleText() const
{
    return d->idleText();
}

QSize FilterEdit::sizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    return d->sizeHint() + QSize(6, 6); //focus border
#else
    return d->sizeHint();
#endif
}

QSize FilterEdit::minimumSizeHint() const
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    return d->minimumSizeHint() + QSize(6, 6); //focus border
#else
    return d->minimumSizeHint();
#endif
}

void FilterEdit::resizeEvent(QResizeEvent *)
{
#if defined(MAC_USE_NATIVE_SEARCHFIELD)
    d->setGeometry(2, 4, width() - 6, height() - 6);
#else
    d->setGeometry(0, 0, width(), height());
#endif
}


#if defined(MAC_USE_NATIVE_SEARCHFIELD)

OSStatus FilterEditPrivate::macEventHandler(EventHandlerCallRef, EventRef event, void *data)
{
    FilterEditPrivate *that = static_cast<FilterEditPrivate *>(data);

    if (!that)
        return eventNotHandledErr;

    switch (GetEventClass(event)) {
    case kEventClassTextField:
        switch (GetEventKind(event)) {
        case kEventTextDidChange: {
            that->m_timer->start();
            OptionBits c = kHISearchFieldAttributesCancel;
            bool empty = that->text().isEmpty();
            HISearchFieldChangeAttributes(that->m_hisearch, empty ? 0 : c, empty ? c : 0);
            break;
        }
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

void FilterEditPrivate::checkText(const QString &)
{
    doLayout();
    m_timer->start();
}

void FilterEditPrivate::doLayout()
{
//    QSize ms = w_menu->sizeHint();
//    QSize cs = w_clear->sizeHint();
    QSize ms = w_menu->size();
    QSize cs = w_clear->size();
    int fw = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

    w_menu->move(rect().left() + fw, (rect().bottom() + 1 - ms.height())/2);
    w_clear->setVisible(!text().isEmpty());
    if (w_clear->isVisible())
        w_clear->move(rect().right() - fw - cs.width(), (rect().bottom() + 1 - cs.height())/2);

    setTextMargins(m_left + ms.width() + fw, m_top, m_right + w_clear->isVisible() ? (cs.width() + fw) : 0, m_bottom);
}


void FilterEditPrivate::resizeEvent(QResizeEvent *)
{
    doLayout();
}

void FilterEditPrivate::paintEvent(QPaintEvent *e)
{
    QLineEdit::paintEvent(e);

    if (!hasFocus() && !m_idletext.isEmpty() && text().isEmpty()) {
        QStyleOptionFrameV2 opt;
        initStyleOption(&opt);
        QRect cr = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
        int l, r, t, b;
        getTextMargins(&l, &t, &r, &b);
        cr.adjust(l, t, -r, -b);

        QPainter p(this);
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        p.drawText(cr, Qt::AlignLeft|Qt::AlignVCenter, m_idletext);
    }
}

void FilterEditPrivate::focusInEvent(QFocusEvent *e)
{
    if (!m_idletext.isEmpty())
        update();
    QLineEdit::focusInEvent(e);
}

void FilterEditPrivate::focusOutEvent(QFocusEvent *e)
{
    if (!m_idletext.isEmpty())
        update();
    QLineEdit::focusOutEvent(e);
}

#endif // MAC_USE_NATIVE_SEARCHFIELD

#include "filteredit.moc"

