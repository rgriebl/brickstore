/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

#include <QAbstractButton>
#include <QLineEdit>
#include <QIcon>
#include <QStyle>
#include <QPainter>
#include <QStyleOptionFrame>
#include <QMouseEvent>
#include <QMenu>
#include <QtDebug>

#include "filteredit.h"

#if defined(Q_OS_MAC)
#include <QProxyStyle>
#include <Carbon/Carbon.h>
#include <QtGui/private/qcoregraphics_p.h>

#if (MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_5)
enum {
    kHIThemeFrameTextFieldRound = 1000,
    kHIThemeFrameTextFieldRoundSmall = 1001,
    kHIThemeFrameTextFieldRoundMini = 1002
};
#endif

class MacSearchFieldProxyStyle : public QProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p, const QWidget *w) const
    {
        if (pe == PE_PanelLineEdit) {
            if (const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(opt)) {
                if (frame->state & State_Sunken) {
                    QColor baseColor(frame->palette.window().color());
                    HIThemeFrameDrawInfo fdi;
                    fdi.version = 0;
                    ThemeDrawState tds = kThemeStateActive;
                    if (opt->state & QStyle::State_Active) {
                        if (!(opt->state & QStyle::State_Enabled))
                            tds = kThemeStateUnavailable;
                    } else {
                        if (opt->state & QStyle::State_Enabled)
                            tds = kThemeStateInactive;
                        else
                            tds = kThemeStateUnavailableInactive;
                    }
                    fdi.state = tds;
                    SInt32 frame_size;
                    fdi.kind = kHIThemeFrameTextFieldRound;
                    GetThemeMetric(kThemeMetricEditTextFrameOutset, &frame_size);
                    if ((frame->state & State_ReadOnly) || !(frame->state & State_Enabled))
                        fdi.state = kThemeStateInactive;
                    fdi.isFocused = (frame->state & State_HasFocus);
                    HIRect hirect = qt_hirectForQRect(frame->rect,
                                                      QRect(frame_size, frame_size,
                                                      frame_size * 2, frame_size * 2));
                    QPixmap pix(frame->rect.size());
                    pix.fill(Qt::transparent);
                    QMacCGContext context(&pix); //CGContextRef context = qt_mac_cg_context(&pix);
                    HIThemeDrawFrame(&hirect, &fdi, context, kHIThemeOrientationNormal);
                   // CGContextRelease(context);
                    p->drawPixmap(frame->rect.topLeft(), pix);
                    return;
                }
            }
        }
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
private:
    static inline HIRect qt_hirectForQRect(const QRect &convertRect, const QRect &rect = QRect())
    {
        return CGRectMake(convertRect.x() + rect.x(), convertRect.y() + rect.y(),
                          convertRect.width() - rect.width(), convertRect.height() - rect.height());
    }

};
#endif


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
                m_menu->popup(QPoint(r.x() + height() / 2, r.y()));
            }
            e->accept();
        }
        QAbstractButton::mousePressEvent(e);
    }

private:
    QMenu *m_menu;
    bool m_hover;
};


FilterEdit::FilterEdit(QWidget *parent)
    : QLineEdit(parent)
{
    w_menu = new FilterEditButton(QIcon(":/images/filter_edit_menu.png"), this);
    w_clear = new FilterEditButton(QIcon(":/images/filter_edit_clear.png"), this);
    w_clear->hide();

    connect(w_clear, &QAbstractButton::clicked, this, &QLineEdit::clear);
    connect(this, &QLineEdit::textChanged, this, &FilterEdit::checkText);

    getTextMargins(&m_left, &m_top, &m_right, &m_bottom);
    doLayout();

#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setStyle(new MacSearchFieldProxyStyle());
#endif
}

void FilterEdit::setMenu(QMenu *menu)
{
    w_menu->setMenu(menu);
}

QMenu *FilterEdit::menu() const
{
    return w_menu->menu();
}


void FilterEdit::checkText(const QString &)
{
    doLayout();
}

void FilterEdit::doLayout()
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

    setTextMargins(m_left + ms.width() + fw, m_top,
                   m_right + (w_clear->isVisible() ? (cs.width() + fw) : 0), m_bottom);
}


void FilterEdit::resizeEvent(QResizeEvent *)
{
    doLayout();
}

#if (QT_VERSION < 0x407000) && !defined(Q_WS_MAEMO_5)

void FilterEdit::setPlaceholderText(const QString &str)
{
    m_placeholdertext = str;
    if (QLineEdit::text().isEmpty())
        update();
}

QString FilterEdit::placeholderText() const
{
    return m_placeholdertext;
}

void FilterEdit::paintEvent(QPaintEvent *e)
{
    QLineEdit::paintEvent(e);

    if (!hasFocus() && !m_placeholdertext.isEmpty() && text().isEmpty()) {
        QStyleOptionFrame opt;
        initStyleOption(&opt);
        QRect cr = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
        int l, r, t, b;
        getTextMargins(&l, &t, &r, &b);
        cr.adjust(l, t, -r, -b);

        QPainter p(this);
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        p.drawText(cr, Qt::AlignLeft|Qt::AlignVCenter, m_placeholdertext);
    }
}

void FilterEdit::focusInEvent(QFocusEvent *e)
{
    if (!m_placeholdertext.isEmpty())
        update();
    QLineEdit::focusInEvent(e);
}

void FilterEdit::focusOutEvent(QFocusEvent *e)
{
    if (!m_placeholdertext.isEmpty())
        update();
    QLineEdit::focusOutEvent(e);
}
#endif // (QT_VERSION < 0x407000) && !defined(Q_WS_MAEMO_5)
