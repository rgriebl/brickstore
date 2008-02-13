/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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

#include "cfilteredit.h"


CFilterEdit::CFilterEdit(QWidget *parent)
    : QLineEdit(parent)
{
    m_timer = new QTimer(this);

    w_menu = new QToolButton(this);
    w_menu->setCursor(Qt::ArrowCursor);
//    w_menu->setAutoRaise(true);

    w_clear = new QToolButton(this);
    w_clear->setCursor(Qt::ArrowCursor);
    w_clear->hide();

    w_menu->setStyleSheet("QToolButton { border: none; padding: 0px; }\n"
                          "QToolButton::menu-indicator { subcontrol-origin: border; subcontrol-position: bottom right;"
#if defined( Q_WS_WIN )
                          "position: relative; top: 4px; left: 4px;"
#endif
                          " }");
    w_clear->setStyleSheet("QToolButton { border: none; padding: 0px; }");

    connect(w_clear, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(checkText(const QString&)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTick()));

    doLayout(false);
}

void CFilterEdit::doLayout(bool setpositionsonly)
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

void CFilterEdit::setMenuIcon(const QIcon &icon)
{
    w_menu->setIcon(icon);
    doLayout(false);
}

void CFilterEdit::setClearIcon(const QIcon &icon)
{
    w_clear->setIcon(icon);
    doLayout(false);
}


void CFilterEdit::setMenu(QMenu *menu)
{
    w_menu->setMenu(menu);
    w_menu->setPopupMode(QToolButton::InstantPopup);
}

QMenu *CFilterEdit::menu() const
{
    return w_menu->menu();
}

void CFilterEdit::resizeEvent(QResizeEvent *)
{
    doLayout(true);
}

void CFilterEdit::checkText(const QString &str)
{
    w_clear->setVisible(!str.isEmpty());
    m_timer->start(250);
}

void CFilterEdit::timerTick()
{
    emit filterTextChanged(text());
}

void CFilterEdit::setIdleText(const QString &str)
{
    m_idletext = str;
    update();
}

void CFilterEdit::paintEvent(QPaintEvent *e)
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

void CFilterEdit::focusInEvent(QFocusEvent *e)
{
    if (!m_idletext.isEmpty())
        update();
    QLineEdit::focusInEvent(e);
}

void CFilterEdit::focusOutEvent(QFocusEvent *e)
{
    if (!m_idletext.isEmpty())
        update();
    QLineEdit::focusOutEvent(e);
}
