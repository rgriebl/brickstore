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
#include <QApplication>
#include <QPainter>
#include <QImage>
#include <QDesktopWidget>
#include <QGradient>
#include <QPicture>

#include "application.h"
#include "splash.h"


Splash *Splash::s_inst = 0;
bool Splash::s_dont_show = false;

Splash *Splash::inst()
{
    if (!s_inst && !s_dont_show) {
        s_inst = new Splash();
        s_inst->show();
    }
    return s_inst;
}

Splash::Splash()
    : QSplashScreen(QPixmap(1, 1), Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_NoSystemBackground);

    QFont f = QApplication::font();
    f.setPixelSize(qMax(14 * qApp->desktop()->height() / 1200, 10));
    setFont(f);
    QFontMetrics fm(f);

    QSize s(20 + fm.width("x") * 50, 20 + fm.width("x") * 50);

    QLinearGradient gradient2(0, 0, 0, s.height());
    QRadialGradient gradient1(s.width() / 2, s.height() / 2, 0.4 * qMin(s.width(), s.height()) );
    gradient1.setColorAt(0,    Qt::white);
    gradient1.setColorAt(0.2,  QColor(230, 230, 230));
    gradient1.setColorAt(0.45, QColor(190, 190, 190));
    gradient1.setColorAt(0.5,  QColor(210, 210, 210));
    gradient1.setColorAt(0.55, QColor(190, 190, 190));
    gradient1.setColorAt(0.8,  QColor(230, 230, 230));
    gradient1.setColorAt(1,    Qt::transparent);
    gradient2.setColorAt(0,    Qt::transparent);
    gradient2.setColorAt(0.2,  QColor(230, 230, 230, 128));
    gradient2.setColorAt(0.45, QColor(190, 190, 190,   0));
    gradient2.setColorAt(0.5,  QColor(210, 210, 210,  50));
    gradient2.setColorAt(0.55, QColor(190, 190, 190,   0));
    gradient2.setColorAt(0.8,  QColor(230, 230, 230, 128));
    gradient2.setColorAt(1,    Qt::transparent);

    m_pix = QPixmap(s);
    m_pix.fill(Qt::transparent);
    QPainter p;
    p.begin(&m_pix);
    p.initFrom(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);

    p.setBrush(gradient1);
    p.drawRect(0, 0, s.width(), s.height());

    QPixmap logo(":/images/icon.png");
    QPicture logotext;
    logotext.load(":/images/logo-text.pic");

    QRect ts = logotext.boundingRect();
    QSize ps = logo.size();

    int dx = (s.width() - (ps.width() + 8 + ts.width())) / 2;
    int dy = s.height() / 2;

    p.drawPixmap(dx, dy - ps.height() / 2, logo);
    p.drawPicture(dx + ps.width() + 8, dy - ts.height() / 2 - ts.top(), logotext);
    p.end();

//    m_pix = QPixmap::fromImage(img);
    setPixmap(m_pix);
}

Splash::~Splash()
{
    s_inst = 0;
    s_dont_show = true;
}

void Splash::message(const QString &msg)
{
    QSplashScreen::showMessage(msg, Qt::AlignHCenter | Qt::AlignBottom);
}

void Splash::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawPixmap(0, 0, m_pix);
}
