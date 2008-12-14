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
    : QSplashScreen(QPixmap(1, 1))
{
    setAttribute(Qt::WA_DeleteOnClose);

    QFont f = QApplication::font();
    f.setPixelSize(qMax(14 * qApp->desktop()->height() / 1200, 10));
    setFont(f);
    QFontMetrics fm(f);

    QSize s(20 + fm.width("x") * 50, 10 + fm.height() * 8);

    QLinearGradient fgrad(0, 0, s.width(), s.height());
    fgrad.setColorAt(0,    QColor(  0,   0 ,  0));
    fgrad.setColorAt(0.1,  QColor( 20,  20,  20));
    fgrad.setColorAt(0.15, QColor( 40,  40,  40));
    fgrad.setColorAt(0.2,  QColor( 20,  20,  20));
    fgrad.setColorAt(0.5,  QColor(  0,   0,   0));
    fgrad.setColorAt(0.8,  QColor( 20,  20,  20));
    fgrad.setColorAt(0.85, QColor( 40,  40,  40));
    fgrad.setColorAt(0.9,  QColor( 20,  20,  20));
    fgrad.setColorAt(1,    QColor(  0,   0,   0));

    QLinearGradient hgrad(0, 0, s.width(), 0);
    hgrad.setColorAt(0,    QColor(  0,   0,   0));
    hgrad.setColorAt(0.25, QColor(180, 180, 180));
    hgrad.setColorAt(0.5,  QColor(255, 255, 255));
    hgrad.setColorAt(0.75, QColor(180, 180, 180));
    hgrad.setColorAt(1,    QColor(  0,   0,   0));

    QLinearGradient hgrad2(s.width()*.25, 0, s.width()*.75, 0);
    hgrad2.setColorAt(0,    QColor(  0,   0,   0,   0));
    hgrad2.setColorAt(0.25, QColor(180, 180, 180, 180));
    hgrad2.setColorAt(0.5,  QColor(255, 255, 255, 255));
    hgrad2.setColorAt(0.75, QColor(180, 180, 180, 180));
    hgrad2.setColorAt(1,    QColor(  0,   0,   0,   0));

    QLinearGradient vgrad(0, 0, 0, s.height());
    vgrad.setColorAt(0,    QColor(  0,   0,   0));
    vgrad.setColorAt(0.25, QColor(180, 180, 180));
    vgrad.setColorAt(0.5,  QColor(255, 255, 255));
    vgrad.setColorAt(0.75, QColor(180, 180, 180));
    vgrad.setColorAt(1,    QColor(  0,   0,   0));

    QPixmap pix(s);
    pix.fill(Qt::white);

    QPainter p;
    p.begin(&pix);
    p.initFrom(this);

    p.fillRect(pix.rect(), fgrad);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(hgrad,1));
    p.drawLine(1, 1, s.width()-2, 1);
    p.drawLine(1, s.height()-2, s.width()-2, s.height()-2);
    p.setPen(QPen(hgrad2,1));
    p.drawLine(s.width()*.25, s.height()*.7, s.width()*.75, s.height()*.7);

    p.setPen(QPen(vgrad,1));
    p.drawLine(1, 1, 1, s.height()-2);
    p.drawLine(s.width()-2, 1, s.width()-2, s.height()-2);

    QPixmap logo(":/images/icon.png");
    QPicture logotext;
    logotext.load(":/images/logo-text.pic");

    QRect ts = logotext.boundingRect();
    QSize ps = logo.size();

    int dx = (s.width() - (ps.width() + 8 + ts.width())) / 2;
    int dy = s.height() / 2;

    p.drawPixmap(dx, dy - ps.height() / 2, logo);

    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setPen(Qt::white);
    p.setBrush(QColor(240,240,240));
    p.drawPicture(dx + ps.width() + 8, dy - ts.height() / 2 - ts.top(), logotext);

    QString version = Application::inst()->applicationVersion();
    QRect vr = fm.boundingRect(version);

    p.setPen(QColor(192, 192, 192));
    p.setBrush(Qt::NoBrush);
    p.drawText(pix.rect().adjusted(0, 10, -10, 0), Qt::AlignTop | Qt::AlignRight, version);

    p.end();

    setPixmap(pix);
}

Splash::~Splash()
{
    s_inst = 0;
    s_dont_show = true;
}

void Splash::message(const QString &msg)
{
    QSplashScreen::showMessage(msg + QLatin1String("\n"), Qt::AlignHCenter | Qt::AlignBottom, Qt::white);
}
