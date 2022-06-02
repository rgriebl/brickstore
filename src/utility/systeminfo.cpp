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
#  endif
#  if defined(Q_CC_MINGW)
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0500
#  endif
#  include <windows.h>
#  include <qpa/qplatformnativeinterface.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#elif defined(Q_OS_LINUX)
#  include <sys/sysinfo.h>
#endif

#include "qcoro/core/qcorofuture.h"
#include "utility.h"
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
    m_map["os.type"_l1] = QSysInfo::productType();
    m_map["os.version"_l1] = QSysInfo::productVersion();
    m_map["os.name"_l1] = QSysInfo::prettyProductName();
    m_map["os.arch"_l1] = QSysInfo::currentCpuArchitecture();
    m_map["qt.version"_l1] = QString::fromLatin1(qVersion());
    m_map["qt.debug"_l1] = QLibraryInfo::isDebugBuild();
    m_map["build.compiler"_l1] = QLatin1String(BRICKSTORE_COMPILER_VERSION);
    m_map["build.arch"_l1] = QSysInfo::buildCpuArchitecture();
    m_map["build.host"_l1] = QLatin1String(BRICKSTORE_BUILD_HOST);
    m_map["build.qt.version"_l1] = QString::fromLatin1(QT_VERSION_STR);
    m_map["build.date"_l1] = QLocale::c().toDateTime(QString::fromLatin1(__DATE__ " " __TIME__)
                                                     .simplified(), "MMM d yyyy HH:mm:ss"_l1);
    m_map["build.number"_l1] = QLatin1String(BRICKSTORE_BUILD_NUMBER);
    m_map["brickstore.locale"_l1] = QLocale().name().left(2);
    m_map["brickstore.version"_l1] = QCoreApplication::applicationVersion();

    m_map["hw.cpu.arch"_l1] = QSysInfo::currentCpuArchitecture();
    // set below - may be delayed due to external processes involved
    // m_map["hw.cpu"_l1] = "e.g. Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz"
    // m_map["hw.gpu"_l1] = "e.g. NVIDIA GeForce GTX 1650";
    // m_map["hw.gpu.arch"_l1] = "intel|amd|nvidia";

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

    m_map["hw.memory"_l1] = physmem;
    m_map["hw.memory.gb"_l1] = QString::number(double(physmem / 1024 / 1024) / 1024, 'f', 1);


    // we cannot use co-routines in a constructor
    QMetaObject::invokeMethod(this, &SystemInfo::init, Qt::QueuedConnection);
}

QCoro::Task<> SystemInfo::init()
{
    // -- CPU ------------------------------------------------

    auto checkCpu = []() -> QString {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        QProcess p;
        p.start("sh"_l1, { "-c"_l1, R"(grep -m 1 '^model name' /proc/cpuinfo | sed -e 's/^.*: //g')"_l1 },
                QIODevice::ReadOnly);
        p.waitForFinished(1000);
        return QString::fromUtf8(p.readAllStandardOutput()).simplified();

#elif defined(Q_OS_WIN) && !defined(BS_BACKEND)
        QProcess p;
        p.start("wmic"_l1, { "/locale:ms_409"_l1, "cpu"_l1, "get"_l1, "name"_l1, "/value"_l1 },
                QIODevice::ReadOnly);
        p.waitForFinished(1000);
        return QString::fromUtf8(p.readAllStandardOutput()).simplified().mid(5);

#elif defined(Q_OS_MACOS)
        char brand[1024];
        size_t brandSize = sizeof(brand) - 1;

        if (sysctlbyname("machdep.cpu.brand_string", brand, &brandSize, nullptr, 0) == 0)
            return QString::fromLocal8Bit(brand, int(brandSize) - 1);
        else
            return "?"_l1;
#else
        return "?"_l1;
#endif
    };

    // -- GPU ------------------------------------------------

    auto checkGpu = []() -> QPair<QString, QString> {
        QPair<QString, QString> result;
        uint vendor = 0;

#if defined(Q_OS_WIN)
        // On Windows, this will provide additional GPU info similar to the output of dxdiag.
        if (auto *ni = qApp->nativeInterface<QNativeInterface::Private::QWindowsApplication>()) {
            const QVariantList gpuInfos = ni->gpuList().toList();
            if (gpuInfos.size() > 0) {

                const auto gpuMap = gpuInfos.constFirst().toMap();
                result.first = gpuMap.value("description"_l1).toString();
                vendor = gpuMap.value("vendorId"_l1).toUInt();
            }
        }
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        QProcess p;
        p.start("lspci"_l1, { "-d"_l1, "::0300"_l1, "-vmm"_l1, "-nn"_l1 }, QIODevice::ReadOnly);
        p.waitForFinished(1000);

        QStringList lspci = QString::fromUtf8(p.readAllStandardOutput()).split('\n'_l1);
        for (const auto &line : lspci) {
            if (line.startsWith("SDevice:"_l1))
                result.first = line.mid(8).simplified().chopped(7);
            else if (line.startsWith("Device:"_l1) && result.first.isEmpty())
                result.first = line.mid(7).simplified().chopped(7);
            else if (line.startsWith("Vendor:"_l1))
                vendor = line.trimmed().right(5).left(4).toUInt(nullptr, 16);
        }

#elif defined(Q_OS_MACOS)
       QProcess p;
       p.start("system_profiler"_l1, { "-json"_l1, "SPDisplaysDataType"_l1 }, QIODevice::ReadOnly);
       p.waitForFinished(3000);
       auto json = QJsonDocument::fromJson(p.readAllStandardOutput());
       auto o = json.object().value("SPDisplaysDataType"_l1).toArray().first().toObject();
       result.first = o.value("sppci_model"_l1).toString();
       result.second = o.value("spdisplays_vendor"_l1).toString().toLower();

#endif
        if (vendor) {
            switch (vendor) {
            case 0x10de: result.second = "nvidia"_l1; break;
            case 0x8086: result.second = "intel"_l1; break;
            case 0x1002: result.second = "amd"_l1; break;
            case 0x15ad: result.second = "vmware"_l1; break;
            }
        }
        return result;
    };

    m_map["hw.cpu"_l1] = co_await QtConcurrent::run(checkCpu);
    auto gpuResultPair = co_await QtConcurrent::run(checkGpu);
    m_map["hw.gpu"_l1] = gpuResultPair.first;
    m_map["hw.gpu.arch"_l1] = gpuResultPair.second;

    emit initialized();
}

QVariantMap SystemInfo::asMap() const
{
    return m_map;
}

quint64 SystemInfo::physicalMemory() const
{
    return m_map["hw.memory"_l1].toULongLong();
}

#include "moc_systeminfo.cpp"
