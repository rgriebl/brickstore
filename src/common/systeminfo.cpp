/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QtConcurrent>
#include <QProcess>
#include <QLibraryInfo>
#include <QCoreApplication>
#include <QLocale>

#if defined(Q_OS_WINDOWS)
#  if !defined(BS_BACKEND)
#    include <private/qguiapplication_p.h>
#    include <qpa/qplatformnativeinterface.h>
#  endif
#  if defined(Q_CC_MINGW)
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0500
#  endif
#  include <windows.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#elif defined(Q_OS_LINUX)
#  include <sys/sysinfo.h>
#endif

#include "qtdiag/qtdiag.h"
#include "qcoro/qcorofuture.h"
#include "version.h"
#include "systeminfo.h"


SystemInfo *SystemInfo::s_inst = nullptr;

SystemInfo *SystemInfo::inst()
{
    if (!s_inst)
        s_inst = new SystemInfo();
    return s_inst;
}

SystemInfo::~SystemInfo()
{
    s_inst = nullptr;
}

SystemInfo::SystemInfo()
    : QObject(QCoreApplication::instance())
{
    m_map[u"os.type"_qs] = QSysInfo::productType();
    m_map[u"os.version"_qs] = QSysInfo::productVersion();
    m_map[u"os.productname"_qs] = QSysInfo::prettyProductName();
    m_map[u"os.arch"_qs] = QSysInfo::currentCpuArchitecture();
    m_map[u"qt.version"_qs] = QString::fromLatin1(qVersion());
    m_map[u"qt.debug"_qs] = QLibraryInfo::isDebugBuild();
    m_map[u"build.compiler"_qs] = QLatin1String(BRICKSTORE_COMPILER_VERSION);
    m_map[u"build.arch"_qs] = QSysInfo::buildCpuArchitecture();
    m_map[u"build.host"_qs] = QLatin1String(BRICKSTORE_BUILD_HOST);
    m_map[u"build.qt.version"_qs] = QString::fromLatin1(QT_VERSION_STR);
    m_map[u"build.date"_qs] = QLocale::c().toDateTime(QString::fromLatin1(__DATE__ " " __TIME__)
                                                      .simplified(), u"MMM d yyyy HH:mm:ss"_qs);
    m_map[u"build.number"_qs] = QLatin1String(BRICKSTORE_BUILD_NUMBER);
    m_map[u"brickstore.locale"_qs] = QLocale().name().left(2);
    m_map[u"brickstore.version"_qs] = QCoreApplication::applicationVersion();

    m_map[u"hw.cpu.arch"_qs] = QSysInfo::currentCpuArchitecture();
    // set below - may be delayed due to external processes involved
    // m_map[u"hw.cpu"_qs] = "e.g. Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz"
    // m_map[u"hw.gpu"_qs] = "e.g. NVIDIA GeForce GTX 1650";
    // m_map[u"hw.gpu.arch"_qs] = "intel|amd|nvidia";

    // -- Memory ---------------------------------------------

    quint64 physmem = 0;

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    uint64_t ram;
    int sctl[] = { CTL_HW, HW_MEMSIZE };
    size_t ramsize = sizeof(ram);

    if (sysctl(sctl, 2, &ram, &ramsize, nullptr, 0) == 0)
        physmem = quint64(ram);

#elif defined(Q_OS_WINDOWS)
    MEMORYSTATUSEX memstatex = { sizeof(memstatex) };
    GlobalMemoryStatusEx(&memstatex);
    physmem = memstatex.ullTotalPhys;

#elif defined(Q_OS_LINUX)
    struct sysinfo si;
    if (sysinfo(&si) == 0)
        physmem = quint64(si.totalram) * si.mem_unit;

#else
#  warning "BrickStore doesn't know how to get the physical memory size on this platform!"
#endif

    m_map[u"hw.memory"_qs] = physmem;
    m_map[u"hw.memory.gb"_qs] = QString::number(double(physmem / 1024 / 1024) / 1024, 'f', 1);

    // -- Platform specific ----------------------------------

#if defined(Q_OS_WINDOWS)
    if (auto wa = qApp->nativeInterface<QNativeInterface::Private::QWindowsApplication>())
        m_map[u"windows.tabletmode"_qs] = wa->isTabletMode();

#endif

    // we cannot use co-routines in a constructor
    QMetaObject::invokeMethod(this, &SystemInfo::init, Qt::QueuedConnection);
}

QCoro::Task<> SystemInfo::init()
{
    // -- CPU ------------------------------------------------

    auto checkCpu = []() -> QString {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        QProcess p;
        p.start(u"sh"_qs, { u"-c"_qs, uR"(grep -m 1 '^model name' /proc/cpuinfo | sed -e 's/^.*: //g')"_qs },
                QIODevice::ReadOnly);
        p.waitForFinished(1000);
        return QString::fromUtf8(p.readAllStandardOutput()).simplified();

#elif defined(Q_OS_WIN) && !defined(BS_BACKEND)
        QProcess p;
        p.start(u"wmic"_qs, { u"/locale:ms_409"_qs, u"cpu"_qs, u"get"_qs, u"name"_qs, u"/value"_qs },
                QIODevice::ReadOnly);
        p.waitForFinished(1000);
        return QString::fromUtf8(p.readAllStandardOutput()).simplified().mid(5);

#elif defined(Q_OS_MACOS)
        char brand[1024];
        size_t brandSize = sizeof(brand) - 1;

        if (sysctlbyname("machdep.cpu.brand_string", brand, &brandSize, nullptr, 0) == 0)
            return QString::fromLocal8Bit(brand, int(brandSize) - 1);
        else
            return u"?"_qs;
#else
        return u"?"_qs;
#endif
    };

    // -- GPU ------------------------------------------------

    auto checkGpu = []() -> QPair<QString, QString> {
        QPair<QString, QString> result;
        uint vendor = 0;

#if !defined(BS_BACKEND)
#  if defined(Q_OS_WIN)
        // On Windows, this will provide additional GPU info similar to the output of dxdiag.
        if (auto *ni = qApp->nativeInterface<QNativeInterface::Private::QWindowsApplication>()) {
            const QVariantList gpuInfos = ni->gpuList().toList();
            if (gpuInfos.size() > 0) {

                const auto gpuMap = gpuInfos.constFirst().toMap();
                result.first = gpuMap.value(u"description"_qs).toString();
                vendor = gpuMap.value(u"vendorId"_qs).toUInt();
            }
        }
#  elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        QProcess p;
        p.start(u"lspci"_qs, { u"-d"_qs, u"::0300"_qs, u"-vmm"_qs, u"-nn"_qs }, QIODevice::ReadOnly);
        p.waitForFinished(1000);

        QStringList lspci = QString::fromUtf8(p.readAllStandardOutput()).split(u'\n');
        for (const auto &line : lspci) {
            if (line.startsWith(u"SDevice:"))
                result.first = line.mid(8).simplified().chopped(7);
            else if (line.startsWith(u"Device:") && result.first.isEmpty())
                result.first = line.mid(7).simplified().chopped(7);
            else if (line.startsWith(u"Vendor:"))
                vendor = line.trimmed().right(5).left(4).toUInt(nullptr, 16);
        }

#  elif defined(Q_OS_MACOS)
       QProcess p;
       p.start(u"system_profiler"_qs, { u"-json"_qs, u"SPDisplaysDataType"_qs }, QIODevice::ReadOnly);
       p.waitForFinished(3000);
       auto json = QJsonDocument::fromJson(p.readAllStandardOutput());
       auto o = json.object().value(u"SPDisplaysDataType"_qs).toArray().first().toObject();
       result.first = o.value(u"sppci_model"_qs).toString();
       auto vendorIdString = o.value(u"spdisplays_vendor-id"_qs).toString();
       if (!vendorIdString.isEmpty())
           vendor = vendorIdString.toUInt(nullptr, 16);
       else
           result.second = o.value(u"spdisplays_vendor"_qs).toString();

#  endif
#endif
        if (vendor) {
            switch (vendor) {
            case 0x10de: result.second = u"nvidia"_qs; break;
            case 0x8086: result.second = u"intel"_qs; break;
            case 0x1002: result.second = u"amd"_qs; break;
            case 0x15ad: result.second = u"vmware"_qs; break;
            }
        }
        return result;
    };

    m_map[u"hw.cpu"_qs] = co_await QtConcurrent::run(checkCpu);
    auto gpuResultPair = co_await QtConcurrent::run(checkGpu);
    m_map[u"hw.gpu"_qs] = gpuResultPair.first;
    m_map[u"hw.gpu.arch"_qs] = gpuResultPair.second;

    emit initialized();
}

QVariantMap SystemInfo::asMap() const
{
    return m_map;
}

QVariant SystemInfo::value(const QString &key) const
{
    return m_map.value(key);
}

quint64 SystemInfo::physicalMemory() const
{
    return m_map[u"hw.memory"_qs].toULongLong();
}

QString SystemInfo::qtDiag() const
{
    return QT_PREPEND_NAMESPACE(qtDiag)(-1);
}

#include "moc_systeminfo.cpp"
