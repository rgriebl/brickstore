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


class FilterEditButton : public QAbstractButton
{
public:
    FilterEditButton(const QIcon &icon, QWidget *parent)
        : QAbstractButton(parent), m_menu(nullptr), m_hover(false)
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
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    QMenu *m_menu;
    bool m_hover;
};

void FilterEditButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    icon().paint(&p, rect(), Qt::AlignCenter, isEnabled() ? (m_hover ? QIcon::Active : QIcon::Normal)
                                                          : QIcon::Disabled,
                 isDown() ? QIcon::On : QIcon::Off);
}

void FilterEditButton::enterEvent(QEvent *)
{
    m_hover = true;
    update();
}

void FilterEditButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void FilterEditButton::mousePressEvent(QMouseEvent *e)
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
