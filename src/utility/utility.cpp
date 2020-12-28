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

#include <cmath>

#include <QColor>
#include <QDesktopWidget>
#include <QApplication>
#include <QDir>
#include <QWidget>

#if defined(Q_OS_WINDOWS)
#  if defined(Q_CC_MINGW)
#    define _WIN32_WINNT 0x0500
#  endif
#  include <windows.h>
#elif defined(Q_OS_BSD4)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#else
#  include <QFile>
#  include <QByteArray>
#endif

#include "utility.h"


static int naturalCompareNumbers(const QChar *&n1, const QChar *&n2)
{
    int result = 0;

    while (true) {
        const auto d1 = (n1++)->digitValue();
        const auto d2 = (n2++)->digitValue();

        if (d1 == -1 && d2 == -1)
            return result;
        else if (d1 == -1)
            return -1;
        else if (d2 == -1)
            return 1;
        else if (d1 != d2 && !result)
            result = d1 - d2;
    }
}

int Utility::naturalCompare(const QString &name1, const QString &name2)
{
    bool empty1 = name1.isEmpty();
    bool empty2 = name2.isEmpty();

    if (empty1 && empty2)
        return 0;
    else if (empty1)
        return -1;
    else if (empty2)
        return 1;

    bool special = false;
    const QChar *n1 = name1.constData();
    const QChar *n1e = n1 + name1.size();
    const QChar *n2 = name2.constData();
    const QChar *n2e = n2 + name2.size();

    while (true) {
        // 1) skip white space
        while ((n1 < n1e) && n1->isSpace()) {
            n1++;
            special = true;
        }
        while ((n2 < n2e) && n2->isSpace()) {
            n2++;
            special = true;
        }

        // 2) check for numbers
        if ((n1 < n1e) && n1->isDigit() && (n2 < n2e) && n2->isDigit()) {
            int d = naturalCompareNumbers(n1, n2);
            if (d)
                return d;
            special = true;
        }


        // 4) naturally the same -> let the unicode order decide
        if ((n1 >= n1e) && (n2 >= n2e))
            return special ? name1.localeAwareCompare(name2) : 0;

        // 5) found a difference
        if (n1 >= n1e)
            return -1;
        else if (n2 >= n2e)
            return 1;
        else if (*n1 != *n2)
            return n1->unicode() - n2->unicode();

        n1++; n2++;
    }
}

qreal Utility::colorDifference(const QColor &c1, const QColor &c2)
{
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    return (qAbs(r1-r2) + qAbs(g1-g2) + qAbs(b1-b2)) / qreal(3);
}

QColor Utility::gradientColor(const QColor &c1, const QColor &c2, qreal f)
{
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    f = qBound(qreal(0), f, qreal(1));
    qreal e = qreal(1) - f;

    return QColor::fromRgbF(r1 * e + r2 * f, g1 * e + g2 * f, b1 * e + b2 * f, a1 * e + a2 * f);
}

QColor Utility::contrastColor(const QColor &c, qreal f)
{
    qreal h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);

    f = qBound(qreal(0), f, qreal(1));

    v += f * ((v < qreal(0.5)) ? qreal(1) : qreal(-1));
    v = qBound(qreal(0), v, qreal(1));

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
                    error = qApp->translate("Utility", "Could not rename %1 to %2.").arg(newpath, basepath);
            }
            else
                error = qApp->translate("Utility", "Could not backup %1 to %2.").arg(basepath, oldpath);
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

#if defined(Q_OS_BSD4)
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

    if (sysctl(sctl, 2, &ram, &ramsize, nullptr, 0) == 0)
        physmem = quint64(ram);

#elif defined(Q_OS_WINDOWS)
    if ((QSysInfo::WindowsVersion & QSysInfo::WV_NT_based) >= QSysInfo::WV_2000) {
        MEMORYSTATUSEX memstatex = { sizeof(memstatex) };
        GlobalMemoryStatusEx(&memstatex);
        physmem = memstatex.ullTotalPhys;
    }
    else {
        MEMORYSTATUS memstat = { sizeof(memstat) };
        GlobalMemoryStatus(&memstat);
        physmem = memstat.dwTotalPhys;
    }

#elif defined(Q_OS_LINUX)
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
#  warning "BrickStore doesn't know how to get the physical memory size on this platform!"
#endif

    return physmem;
}


QString Utility::weightToString(double w, QLocale::MeasurementSystem ms, bool optimize, bool show_unit)
{
    static QLocale loc(QLocale::system());

    int prec = 0;
    const char *unit = nullptr;

    if (ms != QLocale::MetricSystem) {
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
        s.append(QLatin1Char(' '));
        s.append(QLatin1String(unit));
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

QString Utility::localForInternationalCurrencySymbol(const QString &international_symbol)
{
    const QList<QLocale> allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                                               QLocale::AnyCountry);

    for (const auto &locale : allLocales) {
        if (locale.currencySymbol(QLocale::CurrencyIsoCode) == international_symbol)
            return locale.currencySymbol(QLocale::CurrencySymbol);
    }
    return international_symbol;
}

