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
#include <QBitmap>

#include "capplication.h"
#include "csplash.h"


CSplash *CSplash::s_inst = 0;
bool CSplash::s_dont_show = false;

CSplash *CSplash::inst()
{
    if (!s_inst && !s_dont_show) {
        s_inst = new CSplash();
        s_inst->show();
    }
    return s_inst;
}

CSplash::CSplash()
        : QSplashScreen(QPixmap(1, 1))
{
    setAttribute(Qt::WA_DeleteOnClose);

    QFont f = QApplication::font();
    f.setPixelSize(qMax(14 * qApp->desktop()->height() / 1200, 10));
    setFont(f);
    QFontMetrics fm(f);

    QSize s(20 + fm.width("x") * 50, 10 + fm.height() * 8);

    QLinearGradient gradient2(0, 0, 0, s.height());
    QRadialGradient gradient1(s.width() / 2, s.height() / 2, qMax(s.width(), s.height()) / 2);
    gradient1.setColorAt(0,    Qt::white);
    gradient1.setColorAt(0.2,  QColor(230, 230, 230));
    gradient1.setColorAt(0.45, QColor(190, 190, 190));
    gradient1.setColorAt(0.5,  QColor(210, 210, 210));
    gradient1.setColorAt(0.55, QColor(190, 190, 190));
    gradient1.setColorAt(0.8,  QColor(230, 230, 230));
    gradient1.setColorAt(1,    Qt::white);
    gradient2.setColorAt(0,    Qt::white);
    gradient2.setColorAt(0.2,  QColor(230, 230, 230, 128));
    gradient2.setColorAt(0.45, QColor(190, 190, 190,   0));
    gradient2.setColorAt(0.5,  QColor(210, 210, 210,  50));
    gradient2.setColorAt(0.55, QColor(190, 190, 190,   0));
    gradient2.setColorAt(0.8,  QColor(230, 230, 230, 128));
    gradient2.setColorAt(1,    Qt::white);

    QImage img(s, QImage::Format_ARGB32_Premultiplied);
    QPainter p;
    p.begin(&img);
    p.initFrom(this);
    p.setPen(Qt::NoPen);

    p.setBrush(gradient1);
    p.drawRect(0, 0, s.width(), s.height());

    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    p.setBrush(gradient2);
    p.drawRect(0, 0, s.width(), s.height());

    p.setPen(QColor(232, 232, 232));
    for (int i = s.height() / 5; i < (s.height() - s.height() / 5 + 5); i += s.height() / 20)
        p.drawLine(0, i, s.width() - 1, i);
    p.setPen(QColor(0, 0, 0));

    QFont f2 = f;
    f2.setPixelSize(f2.pixelSize() * 2);
    f2.setBold(true);
    p.setFont(f2);

    QPixmap logo(":/images/icon.png");
    QString logo_text = cApp->appName();

    QSize ts = p.boundingRect(geometry(), Qt::AlignCenter, logo_text).size();
    QSize ps = logo.size();

    int dx = (s.width() - (ps.width() + 8 + ts.width())) / 2;
    int dy = s.height() / 2;

    p.drawPixmap(dx, dy - ps.height() / 2, logo);
    p.drawText(dx + ps.width() + 8, dy - ts.height() / 2, ts.width(), ts.height(), Qt::AlignCenter, logo_text);
    p.end();

    setPixmap(QPixmap::fromImage(img));
}

CSplash::~CSplash()
{
    s_inst = 0;
    s_dont_show = true;
}

void CSplash::message(const QString &msg)
{
    QSplashScreen::showMessage(msg, Qt::AlignHCenter | Qt::AlignBottom);
}
