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

#include <cmath>

#include <QColor>
#include <QDesktopWidget>
#include <QApplication>
#include <QDir>
#include <QWidget>

#if defined( Q_OS_WIN )
#include <windows.h>

#elif defined( Q_OS_BSD4 )
#include <sys/types.h>
#include <sys/sysctl.h>

#else
#include <QFile>
#include <QByteArray>

#endif

#if defined( Q_WS_X11 )
#include <QMainWindow>
#endif

#include "utility.h"


qreal Utility::colorDifference(const QColor &c1, const QColor &c2)
{
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    return (qAbs(r1-r2) + qAbs(g1-g2) + qAbs(b1-b2)) / 3.f;
}

QColor Utility::gradientColor(const QColor &c1, const QColor &c2, qreal f)
{
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    f = qMin(qMax(f, 0.0), 1.0);
    qreal e = 1.0 - f;

    return QColor::fromRgbF(r1 * e + r2 * f, g1 * e + g2 * f, b1 * e + b2 * f, a1 * e + a2 * f);
}

QColor Utility::contrastColor(const QColor &c, qreal f)
{
    qreal h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);

    f = qMin(qMax(f, 0.0), 1.0);

    v += f * ((v < 0.5) ? 1.0 : -1.0);
    v = qMin(qMax(v, 0.0), 1.0);

    return QColor::fromHsvF(h, s, v, a);
}

void Utility::setPopupPos(QWidget *w, const QRect &pos)
{
    QSize sh = w->sizeHint();
    QSize desktop = qApp->desktop()->size();

    int x = pos.x() + (pos.width() - sh.width()) / 2;
    int y = pos.y() + pos.height();

    if ((y + sh.height()) > desktop.height()) {
        int d = w->frameSize().height() - w->size().height();
#if defined( Q_WS_X11 )
        if (d <= 0) {
            foreach (QWidget *tlw, qApp->topLevelWidgets()) {
                if (qobject_cast<QMainWindow *>(tlw)) {
                    d = tlw->frameSize().height() - tlw->size().height();
                    break;
                }
            }
        }     
#endif
        y = pos.y() - sh.height() - d;
    }
    if (y < 0)
        y = 0;

    if ((x + sh.width()) > desktop.width())
        x = desktop.width() - sh.width();
    if (x < 0)
        x = 0;

    w->move(x, y);
    w->resize(sh);
}



QString Utility::safeRename(const QString &basepath)
{
    QString error;
    QString newpath = basepath + ".new";
    QString oldpath = basepath + ".old";

    QDir cwd;

    if (cwd.exists(newpath)) {
        if (!cwd.exists(oldpath) || cwd.remove(oldpath)) {
            if (!cwd.exists(basepath) || cwd.rename(basepath, oldpath)) {
                if (cwd.rename(newpath, basepath))
                    error = QString();
                else
                    error = qApp->translate("Utility", "Could not rename %1 to %2.").arg(newpath).arg(basepath);
            }
            else
                error = qApp->translate("Utility", "Could not backup %1 to %2.").arg(basepath).arg(oldpath);
        }
        else
            error = qApp->translate("Utility", "Could not delete %1.").arg(oldpath);
    }
    else
        error = qApp->translate("Utility", "Could not find %1.").arg(newpath);

    return error;
}

quint64 Utility::physicalMemory()
{
    quint64 physmem = 0;

#if defined( Q_OS_BSD4 )
#if defined( HW_MEMSIZE )      // Darwin
    const int sctl2 = HW_MEMSIZE;
    uint64_t ram;
#elif defined( HW_PHYSMEM64 )  // NetBSD, OpenBSD
    const int sctl2 = HW_PHYSMEM64;
    uint64_t ram;
#else                          // legacy, 32bit only
    const int sctl2 = HW_PHYSMEM;
    unsigned int ram;
#endif
     int sctl[] = { CTL_HW, sctl2 };
     size_t ramsize = sizeof(ram);

    if (sysctl(sctl, 2, &ram, &ramsize, 0, 0) == 0)
        physmem = quint64(ram);

#elif defined( Q_OS_WIN )
    if ((QSysInfo::WindowsVersion & QSysInfo::WV_NT_based) >= QSysInfo::WV_2000) {
        MEMORYSTATUSEX memstatex;
        GlobalMemoryStatusEx(&memstatex);
        physmem = memstatex.ullTotalPhys;
    }
    else {
        MEMORYSTATUS memstat;
        GlobalMemoryStatus(&memstat);
        physmem = memstat.dwTotalPhys;
    }

#elif defined( Q_OS_LINUX )
    QFile f(QLatin1String("/proc/meminfo"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray line;
        while (!(line = f.readLine()).isNull()) {
            if (line.startsWith("MemTotal:")) {
                line.chop(3); // strip "kB\n" at the end
                physmem = line.mid(9).trimmed().toULongLong() * 1024LL;
                break;
            }
        }
        f.close();
    }

#else
#warning "BrickStore doesn't know how to get the physical memory size on this platform!"
#endif

    return physmem;
}


QString Utility::weightToString(double w, QLocale::MeasurementSystem ms, bool optimize, bool show_unit)
{
    static QLocale loc(QLocale::system());

    int prec = 0;
    const char *unit = 0;

    if (ms == QLocale::ImperialSystem) {
        w *= 0.035273961949580412915675808215204;

        prec = 4;
        unit = "oz";

        if (optimize && (w >= 32)) {
            unit = "lb";
            w /= 16.;
        }
    }
    else {
        prec = 3;
        unit = "g";

        if (optimize && (w >= 1000.)) {
            unit = "kg";
            w /= 1000.;

            if (w >= 1000.) {
                unit = "t";
                w /= 1000.;
            }
        }
    }
    QString s = loc.toString(w, 'f', prec);

    if (show_unit) {
        s += " ";
        s += unit;
    }
    return s;
}

double Utility::stringToWeight(const QString &s, QLocale::MeasurementSystem ms)
{
    static QLocale loc(QLocale::system());

    double w = loc.toDouble(s);

    if (ms == QLocale::ImperialSystem)
        w *= 28.349523125000000000000000000164;

    return w;
}

