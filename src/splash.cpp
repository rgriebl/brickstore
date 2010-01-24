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
#include <QPalette>
#if defined(Q_WS_X11)
#  include <QX11Info>
#endif

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

    QFont f = QApplication::font();
    f.setPixelSize(qMax(14 * qApp->desktop()->height() / 1200, 10));
    f.setBold(true);
    setFont(f);
    QFontMetrics fm(f);

    QPixmap pix(fm.width("x") * 35, fm.width("x") * 35);

    bool transparent =
#if defined(Q_WS_X11)
        QX11Info::isCompositingManagerRunning();
#elif defined(Q_WS_MAC) || defined(Q_WS_WIN)
        true;
#else
        false;
#endif

    if (transparent) {
        // QSplash would ignore the translucency otherwise
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::transparent);
        setPalette(pal);

        setAttribute(Qt::WA_TranslucentBackground);

        pix.fill(Qt::transparent);
    } else {
        QWidget *desktop = QApplication::desktop();
        pix = QPixmap::grabWindow(desktop->winId(), (desktop->width() - pix.width()) / 2, (desktop->height() - pix.height()) / 2, pix.width(), pix.height());
    }

    QPainter p;
    p.begin(&pix);
    p.initFrom(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    QPoint c = pix.rect().center();

    QRadialGradient gradient1(c.x(), c.y(), 0.8 * qMin(c.x(), c.y()) );
    gradient1.setColorAt(0,    Qt::white);
    gradient1.setColorAt(0.2,  QColor(230, 230, 230));
    gradient1.setColorAt(0.45, QColor(190, 190, 190));
    gradient1.setColorAt(0.5,  QColor(210, 210, 210));
    gradient1.setColorAt(0.55, QColor(190, 190, 190));
    gradient1.setColorAt(0.8,  QColor(230, 230, 230));
    gradient1.setColorAt(1,    Qt::transparent);
    p.fillRect(pix.rect(), gradient1);

    QPicture text;
    text.load(":/images/logo-text.pic");
    QRect textr = text.boundingRect();

    QLinearGradient gradient2(textr.topLeft(), textr.bottomRight());
    gradient2.setColorAt(0,    Qt::black);
    gradient2.setColorAt(0.3,  Qt::darkGray);
    gradient2.setColorAt(0.5,  Qt::white);
    gradient2.setColorAt(0.7,  Qt::darkGray);
    gradient2.setColorAt(1,    Qt::black);
    p.setBrush(gradient2);
    p.setPen(Qt::black);
    p.drawPicture(c - textr.center(), text);

    QPixmap logo(":/images/icon.png");
    p.drawPixmap(c - logo.rect().center() - QPoint(0, 8 + logo.height() / 2 + textr.height() / 2), logo);

    QString version = Application::inst()->applicationVersion();
    QRect versionr = fm.boundingRect(version);

    p.setPen(QColor(80, 80, 80));
    p.setBrush(Qt::NoBrush);
    versionr.moveCenter(c);
    versionr.translate(0, 8 + versionr.height() / 2 + textr.height() / 2);
    p.drawText(versionr, Qt::AlignCenter, version);

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
    QSplashScreen::showMessage(msg + QLatin1String("\n\n\n\n"), Qt::AlignHCenter | Qt::AlignBottom, Qt::black);
}

void Splash::paintEvent(QPaintEvent *e)
{
    if (testAttribute(Qt::WA_TranslucentBackground)) {
        QPainter p(this);
        p.fillRect(rect(), palette().background());
    }
}
