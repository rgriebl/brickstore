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
#  if defined( Q_CC_MINGW )
#    define _WIN32_WINNT 0x0500
#  endif
#  include <windows.h>
#elif defined( Q_OS_BSD4 )
#  include <sys/types.h>
#  include <sys/sysctl.h>
#else
#  include <QFile>
#  include <QByteArray>
#endif

#if defined( Q_WS_X11 )
#  include <QMainWindow>
#endif

#include "utility.h"


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
        MEMORYSTATUSEX memstatex = { sizeof(memstatex) };
        GlobalMemoryStatusEx(&memstatex);
        physmem = memstatex.ullTotalPhys;
    }
    else {
        MEMORYSTATUS memstat = { sizeof(memstat) };
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
        s.append(QLatin1Char(' '));
        s.append(QLatin1String(unit));;
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


static const struct CurrencyData {
    QLocale::Country country : 16;
    char             symbol_int[4];
    QChar            symbol[5];
} currency_data[] = {
    { QLocale::Albania,            { 'A', 'L', 'L', 0 }, { 3, 0x4c, 0x65, 0x6b } },
    { QLocale::Afghanistan,        { 'A', 'F', 'N', 0 }, { 1, 0x60b } },
    { QLocale::Argentina,          { 'A', 'R', 'S', 0 }, { 1, 0x24 } },
    { QLocale::Aruba,              { 'A', 'W', 'G', 0 }, { 1, 0x192 } },
    { QLocale::Australia,          { 'A', 'U', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Austria,            { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Azerbaijan,         { 'A', 'Z', 'N', 0 }, { 3, 0x43c, 0x430, 0x43d } },
    { QLocale::Bahamas,            { 'B', 'S', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Barbados,           { 'B', 'B', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Belarus,            { 'B', 'Y', 'R', 0 }, { 2, 0x70, 0x2e } },
    { QLocale::Belgium,            { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Belize,             { 'B', 'Z', 'D', 0 }, { 3, 0x42, 0x5a, 0x24 } },
    { QLocale::Bermuda,            { 'B', 'M', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Bolivia,            { 'B', 'O', 'B', 0 }, { 2, 0x24, 0x62 } },
    { QLocale::BosniaAndHerzegowina,{'B', 'A', 'M', 0 }, { 2, 0x4b, 0x4d } },
    { QLocale::Botswana,           { 'B', 'W', 'P', 0 }, { 1, 0x50 } },
    { QLocale::Bulgaria,           { 'B', 'G', 'N', 0 }, { 2, 0x43b, 0x432 } },
    { QLocale::Brazil,             { 'B', 'R', 'L', 0 }, { 2, 0x52, 0x24 } },
    { QLocale::BruneiDarussalam,   { 'B', 'N', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Cambodia,           { 'K', 'H', 'R', 0 }, { 1, 0x17db } },
    { QLocale::Canada,             { 'C', 'A', 'D', 0 }, { 1, 0x24 } },
    { QLocale::CaymanIslands,      { 'K', 'Y', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Chile,              { 'C', 'L', 'P', 0 }, { 1, 0x24 } },
    { QLocale::China,              { 'C', 'N', 'Y', 0 }, { 1, 0x5143 } },
    { QLocale::Colombia,           { 'C', 'O', 'P', 0 }, { 1, 0x24 } },
    { QLocale::CostaRica,          { 'C', 'R', 'C', 0 }, { 1, 0x20a1 } },
    { QLocale::Croatia,            { 'H', 'R', 'K', 0 }, { 2, 0x6b, 0x6e } },
    { QLocale::Cuba,               { 'C', 'U', 'P', 0 }, { 1, 0x20b1 } },
    { QLocale::Cyprus,             { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::CzechRepublic,      { 'C', 'Z', 'K', 0 }, { 2, 0x4b, 0x10d } },
    { QLocale::Denmark,            { 'D', 'K', 'K', 0 }, { 2, 0x6b, 0x72} },
    { QLocale::DominicanRepublic,  { 'D', 'O', 'P', 0 }, { 3, 0x52, 0x44, 0x24 } },
    { QLocale::Egypt,              { 'E', 'G', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::ElSalvador,         { 'S', 'V', 'C', 0 }, { 1, 0x24 } },
    { QLocale::Estonia,            { 'E', 'E', 'K', 0 }, { 2, 0x6b, 0x72 } },
    { QLocale::FalklandIslands,    { 'F', 'K', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::FijiCountry,        { 'F', 'J', 'D', 0 }, { 1, 0x24 } },
    { QLocale::France,             { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Germany,            { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Ghana,              { 'G', 'H', 'C', 0 }, { 1, 0xa2 } },
    { QLocale::Gibraltar,          { 'G', 'I', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::Greece,             { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Guatemala,          { 'G', 'T', 'Q', 0 }, { 1, 0x51 } },
    { QLocale::Guyana,             { 'G', 'Y', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Netherlands,        { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Honduras,           { 'H', 'N', 'L', 0 }, { 1, 0x4c } },
    { QLocale::HongKong,           { 'H', 'K', 'D', 0 }, { 3, 0x48, 0x4b, 0x24 } },
    { QLocale::Hungary,            { 'H', 'U', 'F', 0 }, { 2, 0x46, 0x74 } },
    { QLocale::Iceland,            { 'I', 'S', 'K', 0 }, { 2, 0x6b, 0x72 } },
    { QLocale::India,              { 'I', 'N', 'R', 0 }, { 1, 0x20a8 } },
    { QLocale::Indonesia,          { 'I', 'D', 'R', 0 }, { 2, 0x52, 0x70 } },
    { QLocale::Iran,               { 'I', 'R', 'R', 0 }, { 1, 0xfdfc } },
    { QLocale::Ireland,            { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Israel,             { 'I', 'L', 'S', 0 }, { 1, 0x20aa } },
    { QLocale::Italy,              { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Jamaica,            { 'J', 'M', 'D', 0 }, { 2, 0x4a, 0x24 } },
    { QLocale::Japan,              { 'J', 'P', 'Y', 0 }, { 1, 0xa5 } },
    { QLocale::Kazakhstan,         { 'K', 'Z', 'T', 0 }, { 2, 0x43b, 0x432 } },
    { QLocale::DemocraticRepublicOfKorea,
                                   { 'K', 'P', 'W', 0 }, { 1, 0x20a9 } },
    { QLocale::RepublicOfKorea,    { 'K', 'R', 'W', 0 }, { 1, 0x20a9 } },
    { QLocale::Kyrgyzstan,         { 'K', 'G', 'S', 0 }, { 2, 0x43b, 0x432 } },
    { QLocale::Lao,                { 'L', 'A', 'K', 0 }, { 1, 0x20ad } },
    { QLocale::Latvia,             { 'L', 'V', 'L', 0 }, { 2, 0x4c, 0x73 } },
    { QLocale::Lebanon,            { 'L', 'B', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::Liberia,            { 'L', 'R', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Liechtenstein,      { 'C', 'H', 'F', 0 }, { 3, 0x43, 0x48, 0x46 } },
    { QLocale::Lithuania,          { 'L', 'T', 'L', 0 }, { 2, 0x4c, 0x74 } },
    { QLocale::Luxembourg,         { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Macedonia,          { 'M', 'K', 'D', 0 }, { 3, 0x434, 0x435, 0x43d } },
    { QLocale::Malaysia,           { 'M', 'Y', 'R', 0 }, { 2, 0x52, 0x4d } },
    { QLocale::Malta,              { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Mauritius,          { 'M', 'U', 'R', 0 }, { 1, 0x20a8 } },
    { QLocale::Mexico,             { 'M', 'X', 'N', 0 }, { 1, 0x24 } },
    { QLocale::Mongolia,           { 'M', 'N', 'T', 0 }, { 1, 0x20ae } },
    { QLocale::Mozambique,         { 'M', 'Z', 'N', 0 }, { 2, 0x4d, 0x54 } },
    { QLocale::Namibia,            { 'N', 'A', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Nepal,              { 'N', 'P', 'R', 0 }, { 1, 0x20a8 } },
    { QLocale::NetherlandsAntilles,{ 'A', 'N', 'G', 0 }, { 1, 0x192 } },
    { QLocale::Netherlands,        { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::NewZealand,         { 'N', 'Z', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Nicaragua,          { 'N', 'I', 'O', 0 }, { 2, 0x43, 0x24 } },
    { QLocale::Nigeria,            { 'N', 'G', 'N', 0 }, { 1, 0x20a6 } },
    { QLocale::Norway,             { 'N', 'O', 'K', 0 }, { 2, 0x6b, 0x72 } },
    { QLocale::Oman,               { 'O', 'M', 'R', 0 }, { 1, 0xfdfc } },
    { QLocale::Pakistan,           { 'P', 'K', 'R', 0 }, { 1, 0x20a8 } },
    { QLocale::Panama,             { 'P', 'A', 'B', 0 }, { 3, 0x42, 0x2f, 0x2e } },
    { QLocale::Paraguay,           { 'P', 'Y', 'G', 0 }, { 2, 0x47, 0x73 } },
    { QLocale::Peru,               { 'P', 'E', 'N', 0 }, { 3, 0x53, 0x2f, 0x2e } },
    { QLocale::Philippines,        { 'P', 'H', 'P', 0 }, { 3, 0x50, 0x68, 0x70 } },
    { QLocale::Poland,             { 'P', 'L', 'N', 0 }, { 2, 0x7a, 0x142 } },
    { QLocale::Portugal,           { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Qatar,              { 'Q', 'A', 'R', 0 }, { 1, 0xfdfc } },
    { QLocale::Romania,            { 'R', 'O', 'N', 0 }, { 3, 0x6c, 0x65, 0x69 } },
    { QLocale::RussianFederation,  { 'R', 'U', 'B', 0 }, { 3, 0x440, 0x443, 0x431 } },
    { QLocale::StHelena,           { 'S', 'H', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::SaudiArabia,        { 'S', 'A', 'R', 0 }, { 1, 0xfdfc } },
    { QLocale::SerbiaAndMontenegro,{ 'R', 'S', 'D', 0 }, { 4, 0x414, 0x438, 0x43d, 0x2e } },
    { QLocale::Seychelles,         { 'S', 'C', 'R', 0 }, { 1, 0x20a8 } },
    { QLocale::Singapore,          { 'S', 'G', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Slovenia,           { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::SolomonIslands,     { 'S', 'B', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Somalia,            { 'S', 'O', 'S', 0 }, { 1, 0x53 } },
    { QLocale::SouthAfrica,        { 'Z', 'A', 'R', 0 }, { 1, 0x52 } },
    { QLocale::Spain,              { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::SriLanka,           { 'L', 'K', 'R', 0 }, { 1, 0x20a8 } },
    { QLocale::Sweden,             { 'S', 'E', 'K', 0 }, { 2, 0x6b, 0x72 } },
    { QLocale::Switzerland,        { 'C', 'H', 'F', 0 }, { 3, 0x43, 0x48, 0x46 } },
    { QLocale::Suriname,           { 'S', 'R', 'D', 0 }, { 1, 0x24 } },
    { QLocale::SyrianArabRepublic, { 'S', 'Y', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::Taiwan,             { 'T', 'W', 'D', 0 }, { 3, 0x4e, 0x54, 0x24 } },
    { QLocale::Thailand,           { 'T', 'H', 'B', 0 }, { 1, 0xe3f } },
    { QLocale::TrinidadAndTobago,  { 'T', 'T', 'D', 0 }, { 3, 0x54, 0x54, 0x24 } },
    { QLocale::Turkey,             { 'T', 'R', 'Y', 0 }, { 3, 0x59, 0x54, 0x4c } },
    { QLocale::Tuvalu,             { 'T', 'V', 'D', 0 }, { 1, 0x24 } },
    { QLocale::Ukraine,            { 'U', 'A', 'H', 0 }, { 1, 0x20b4 } },
    { QLocale::UnitedKingdom,      { 'G', 'B', 'P', 0 }, { 1, 0xa3 } },
    { QLocale::UnitedStates,       { 'U', 'S', 'D', 0 }, { 1, 0x24  } },
    { QLocale::Uruguay,            { 'U', 'Y', 'U', 0 }, { 2, 0x24, 0x55 } },
    { QLocale::Uzbekistan,         { 'U', 'Z', 'S', 0 }, { 2, 0x43b, 0x432 } },
    { QLocale::VaticanCityState,   { 'E', 'U', 'R', 0 }, { 1, 0x20ac } },
    { QLocale::Venezuela,          { 'V', 'E', 'F', 0 }, { 2, 0x42, 0x73 } },
    { QLocale::VietNam,            { 'V', 'N', 'D', 0 }, { 2, 0x20ab } },
    { QLocale::Yemen,              { 'Y', 'E', 'R', 0 }, { 2, 0xfdfc } },
    { QLocale::Zimbabwe,           { 'Z', 'W', 'D', 0 }, { 2, 0x5a, 0x24 } },

    { QLocale::AnyCountry,         { 0, 0, 0, 0 },       { 0 } }
};

QPair<QString, QString> Utility::currencySymbolsForCountry(QLocale::Country c)
{
    QPair<QString, QString> result;

    for (const CurrencyData *p = currency_data; p->symbol_int[0]; ++p) {
        if (p->country == c) {
            result.first  = QLatin1String(p->symbol_int);
            result.second = QString::fromRawData(p->symbol+1, *((qint16 *)p->symbol));
            break;
        }
    }
    return result;
}

QLocale::Country Utility::countryForCurrencySymbol(const QString &international_symbol)
{
    for (const CurrencyData *p = currency_data; p->symbol_int[0]; ++p) {
        if (QLatin1String(p->symbol_int) == international_symbol) {
            return p->country;
        }
    }
    return QLocale::AnyCountry;
}
