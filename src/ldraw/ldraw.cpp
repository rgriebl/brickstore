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
#include <cfloat>

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QStringBuilder>
#include <QDateTime>
#include <QDebug>
#include <QStringBuilder>
#include <QtConcurrent>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  include <tchar.h>
#  include <shlobj.h>
#endif

#include "qcoro/core/qcorofuture.h"
#include "utility/utility.h"
#include "utility/exception.h"
#include "utility/stopwatch.h"
#include "utility/transfer.h"
#include "minizip/minizip.h"
#include "ldraw.h"
#include "part.h"


Q_LOGGING_CATEGORY(LogLDraw, "ldraw")


namespace LDraw {

Part *Library::findPart(const QString &_filename, const QString &_parentdir)
{
    QString filename = _filename;
    filename.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString parentdir = _parentdir;
    if (!parentdir.isEmpty() && !parentdir.startsWith("!ZIP!"_l1))
        parentdir = QDir(parentdir).canonicalPath();

    bool inZip = false;
    bool found = false;

    if (QFileInfo(filename).isRelative()) {
        // search order is parentdir => p => parts => models

        QStringList searchpath = m_searchpath;
        if (!parentdir.isEmpty() && !searchpath.contains(parentdir))
            searchpath.prepend(parentdir);

        for (const QString &sp : qAsConst(searchpath)) {

            if (sp.startsWith("!ZIP!"_l1)) {
                filename = filename.toLower();
                QString testname = sp.mid(5) % u'/' % filename;
                if (m_zip->contains(testname)) {
                    QFileInfo fi(filename);
                    parentdir = sp;
                    if (fi.path() != "."_l1)
                        parentdir = parentdir % u'/' % fi.path();
                    filename = testname;
                    inZip = true;
                    found = true;
                    break;
                }
            } else {
                QString testname = sp % u'/' % filename;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
                if (!QFile::exists(testname))
                    testname = testname.toLower();
#endif
                if (QFile::exists(testname)) {
                    filename = testname;
                    parentdir = QFileInfo(testname).path();
                    found = true;
                    break;
                }
            }
        }
    } else {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (!QFile::exists(filename))
            filename = filename.toLower();
#endif
        if (QFile::exists(filename)) {
            parentdir = QFileInfo(filename).path();
            found = true;
        }
    }

    if (!found)
        return nullptr;
    if (!inZip)
        filename = QFileInfo(filename).canonicalFilePath();

    Part *p = m_cache[filename];
    if (!p) {
        QByteArray data;

        if (inZip) {
            try {
                data = m_zip->readFile(filename);
            } catch (const Exception &e) {
                qCWarning(LogLDraw) << "Failed to read from LDraw ZIP:" << e.error();
            }
        } else {
            QFile f(filename);

            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qCWarning(LogLDraw) << "Failed to open file" << filename << ":" << f.errorString();
            } else {
                data = f.readAll();
                if (f.error() != QFile::NoError)
                    qCWarning(LogLDraw) << "Failed to read file" << filename << ":" << f.errorString();
                f.close();
            }
        }
        if (!data.isEmpty()) {
            p = Part::parse(data, parentdir);
            if (p) {
                if (!m_cache.insert(filename, p, p->cost())) {
                    qCWarning(LogLDraw) << "Unable to cache file" << filename;
                    p = nullptr;
                }

                //qCInfo(LogLDraw) << "Cache at" << m_cache.totalCost() << "/" <<  m_cache.maxCost() << "with" << m_cache.size() << "parts";
            }
        }
    }
    return p;
}


bool Library::isValidLDrawDir(const QString &ldir)
{
    QFileInfo fi(ldir);

    if (fi.exists() && fi.isDir() && fi.isReadable()) {
        QDir dir(ldir);

        if (dir.cd("p"_l1) || dir.cd("P"_l1)) {
            if (dir.exists("stud.dat"_l1) || dir.exists("STUD.DAT"_l1)) {
                if (dir.cd("../parts"_l1) || dir.cd("../PARTS"_l1)) {
                    if (dir.exists("3001.dat"_l1) || dir.exists("3001.DAT"_l1)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

QStringList Library::potentialDrawDirs()
{
    QStringList dirs;
    dirs << QString::fromLocal8Bit(qgetenv("LDRAWDIR"));

#if defined(Q_OS_WINDOWS)
    {
        wchar_t inidir [MAX_PATH];

        DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
        if (l >= 0) {
            inidir [l] = 0;
            dirs << QString::fromUtf16(reinterpret_cast<ushort *>(inidir));
        }
    }

    {
        HKEY skey, lkey;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
            if (RegOpenKeyExW(skey, L"LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                wchar_t regdir [MAX_PATH + 1];
                DWORD regdirsize = MAX_PATH * sizeof(wchar_t);

                if (RegQueryValueExW(lkey, L"InstallDir", nullptr, nullptr, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                    regdir [regdirsize / sizeof(WCHAR)] = 0;
                    dirs << QString::fromUtf16(reinterpret_cast<ushort *>(regdir));
                }
                RegCloseKey(lkey);
            }
            RegCloseKey(skey);
        }
    }
#elif defined(Q_OS_UNIX)
    {
        QStringList unixdirs;
        unixdirs << "~/ldraw"_l1;

#  if defined(Q_OS_MACOS)
        unixdirs << "~/Library/ldraw"_l1
                 << "~/Library/LDRAW"_l1
                 << "/Library/LDRAW"_l1
                 << "/Library/ldraw"_l1;
#  else
        unixdirs << "/usr/share/ldraw"_l1;
        unixdirs << "/var/lib/ldraw"_l1;
        unixdirs << "/var/ldraw"_l1;
#  endif

        QString homepath = QDir::homePath();

        foreach (QString d, unixdirs) {
            d.replace("~"_l1, homepath);
            dirs << d;
        }
    }
#endif
    dirs.removeAll(QString()); // remove empty strings
    return dirs;
}










Library *Library::s_inst = nullptr;

//Library *Library::create()
//{
//    if (!s_inst) {
//        QString error;
//        QString ldrawdir = datadir;

//        if (ldrawdir.isEmpty()) {
//            const auto ldawDirs = potentialDrawDirs();
//            for (auto &ld : ldawDirs) {
//                if (isValidLDrawDir(ld)) {
//                    ldrawdir = ld;
//                    break;
//                }
//            }
//        }

//        if (!ldrawdir.isEmpty()) {
//            s_inst = new Library(ldrawdir);

//            if (!s_inst->m_zip || s_inst->m_zip->open()) {
//                if (s_inst->parseLDconfig("LDConfig.ldr")) {
//                    s_inst->parseLDconfig("LDConfig_missing.ldr");
//                    qInfo().noquote() << "Found LDraw at" << ldrawdir << "\n  Last updated:"
//                                      << s_inst->lastUpdated().toString(Qt::RFC2822Date);
//                } else {
//                    error = qApp->translate("LDraw", "LDraws's ldcondig.ldr is not readable.");
//                }
//            } else {
//                error = qApp->translate("LDraw", "The LDraw installation at \'%1\' is not usable.").arg(ldrawdir);
//            }
//        } else {
//            error = qApp->translate("LDraw", "No usable LDraw installation available.");
//        }

//        if (!error.isEmpty()) {
//            delete s_inst;
//            s_inst = nullptr;

//            if (errstring)
//                *errstring = error;
//        }
//    }

//    return s_inst;
//}

Library::Library(QObject *parent)
    : QObject(parent)
    , m_transfer(new Transfer(this))
{
    m_cache.setMaxCost(50 * 1024 * 1024); // 50MB
}

Library *Library::inst()
{
    if (!s_inst)
        s_inst = new Library();
    return s_inst;
}

Library::~Library()
{
    // the parts in cache are referencing each other, so a plain clear will not work
    m_cache.clearRecursive();
}

Part *Library::partFromFile(const QString &file)
{
    return findPart(file, QFileInfo(file).path());
}

Part *Library::partFromId(const QByteArray &id)
{
    QString filename = QLatin1String(id) % u".dat";
    return findPart(filename);
}


QString Library::path() const
{
    return m_path;
}

QCoro::Task<bool> Library::setPath(const QString &path)
{
    if (m_path == path)
        co_return false;

    emit libraryAboutToBeReset();

    m_cache.clearRecursive();
    if (!m_cache.isEmpty())
        co_return false;

    m_zip.reset();
    m_searchpath.clear();
    m_path = path;
    m_isZip = QFileInfo(path).isFile() && path.endsWith(".zip"_l1);

    bool caseSensitive =
#if defined(Q_OS_LINUX)
            true;
#else
            false;
#endif

    bool valid = !path.isEmpty();

    if (valid && m_isZip) {
        caseSensitive = false;
        QScopedPointer zip { new MiniZip(path) };

        if (co_await QtConcurrent::run([&zip]() { return zip->open(); }))
            m_zip.reset(zip.take());
        else
            valid = false;
    }
    if (valid && parseLDconfig("LDConfig.ldr"_l1))
        parseLDconfig("LDConfig_missing.ldr"_l1);
    else
        valid = false;

    if (valid) {
        static const char *subdirs[] = { "p/48", "p", "parts", "models" };

        for (auto subdir : subdirs) {
            if (m_zip) {
                m_searchpath << QString(u"!ZIP!ldraw/" % QLatin1String(subdir));
            } else {
                QDir sdir(m_path);
                QString s = QLatin1String(subdir);

                if (sdir.cd(s))
                    m_searchpath << sdir.canonicalPath();
                else if (caseSensitive && sdir.cd(s.toLower()))
                    m_searchpath << sdir.canonicalPath();
            }
        }
    }

    if (valid) {
        qInfo().noquote() << "Found LDraw at" << m_path << "\n  Last updated:"
                          << m_lastUpdated.toString(Qt::RFC2822Date);
    } else if (path.isEmpty()) {
        qInfo().noquote() << "No LDraw installation specified";
    } else {
        qInfo().noquote() << "LDraw installation at" << m_path << "is not useable";
    }

    emit libraryReset();
    if (m_valid != valid) {
        m_valid = valid;
        emit validChanged(valid);
    }
    if (valid)
        emit lastUpdatedChanged(m_lastUpdated);
    co_return true;
}

bool Library::startUpdate()
{
    if (!m_isZip)
        return false;
    return false;
}

void Library::cancelUpdate()
{
}


QByteArray Library::readLDrawFile(const QString &filename)
{
    QByteArray data;
    if (m_zip) {
        QString zipFilename = "ldraw/"_l1 % filename;
        if (m_zip->contains(zipFilename))
            data = m_zip->readFile(zipFilename);
    } else {
        QFile f(path() % u'/' % filename);

        if (!f.open(QIODevice::ReadOnly))
            throw Exception(&f, "Failed to read file");

        data = f.readAll();
        if (f.error() != QFileDevice::NoError)
            throw Exception(&f, "Failed to read file");
    }
    return data;
}

bool Library::parseLDconfig(const QString &filename)
{
    //stopwatch sw("LDraw: parseLDconfig");

    QByteArray data;
    try {
        data = readLDrawFile(filename);
    } catch (const Exception &) {
        //qCWarning(LogLDraw) << "LDraw ZIP:" << e.error();
        return false;
    }
    QTextStream ts(data);

    QString line;
    int lineno = 0;
    while (!ts.atEnd()) {
        line = ts.readLine();
        lineno++;

        QStringList sl = line.simplified().split(' '_l1);

        if (sl.count() >= 9 &&
                sl[0].toInt() == 0 &&
                sl[1] == "!COLOUR"_l1 &&
                sl[3] == "CODE"_l1 &&
                sl[5] == "VALUE"_l1 &&
                sl[7] == "EDGE"_l1) {
            // 0 !COLOUR name CODE x VALUE v EDGE e [ALPHA a] [LUMINANCE l] [ CHROME | PEARLESCENT | RUBBER | MATTE_METALLIC | METAL | MATERIAL <params> ]</params>

            Color c;

            c.id = sl[4].toInt();
            c.name = sl[2];
            c.color = sl[6];
            c.edgeColor = sl[8];

            for (int idx = 9; idx < sl.count(); ++idx) {
                if (sl[idx] == "ALPHA"_l1) {
                    int alpha = sl[++idx].toInt();
                    c.color.setAlpha(alpha);
                    //c.edgeColor.setAlpha(alpha);
                } else if (sl[idx] == "LUMINANCE"_l1) {
                    c.luminance = sl[++idx].toInt();
                } else if (sl[idx] == "CHROME"_l1) {
                    c.chrome = true;
                } else if (sl[idx] == "PEARLESCENT"_l1) {
                    c.pearlescent = true;
                } else if (sl[idx] == "RUBBER"_l1) {
                    c.rubber = true;
                } else if (sl[idx] == "MATTE_METALLIC"_l1) {
                    c.matteMetallic = true;
                } else if (sl[idx] == "METAL"_l1) {
                    c.metal = true;
                }
            }
            if (c.color.isValid() && c.edgeColor.isValid()) {
                m_colors.insert(c.id, c);
            } else {
                qCWarning(LogLDraw) << "Got invalid color " << c.id << " : " << c.color << " // " << c.edgeColor;
            }
        } else if (sl.count() >= 5 &&
                   sl[0].toInt() == 0 &&
                   sl[1] == "!LDRAW_ORG"_l1 &&
                   sl[2] == "Configuration"_l1 &&
                   sl[3] == "UPDATE"_l1) {
            // 0 !LDRAW_ORG Configuration UPDATE yyyy-MM-dd
            m_lastUpdated = QDate::fromString(sl[4], "yyyy-MM-dd"_l1);
        }
    }

    return true;
}


QColor Library::edgeColor(int id) const
{
    if (m_colors.contains(id)) {
        return m_colors.value(id).edgeColor;
    } else if (id >= 0 && id <= 15) {
        const int legacyMapping[] = {
            8, 9, 10, 11, 12, 13, 0, 8,
            0, 1, 2, 3, 4, 5, 8, 8
        };
        return color(legacyMapping[id]);
    } else if (id > 256) {
        // edge color of dithered colors is derived from color 1
        return edgeColor((id - 256) & 0x0f);
    } else {
        // calculate a contrasting color

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        qreal h, s, v, a;
#else
        float h, s, v, a;
#endif
        color(id).getHsvF(&h, &s, &v, &a);

        v += 0.5f * ((v < 0.5f) ? 1 : -1);
        v = qBound<decltype(v)>(0, v, 1);

        return QColor::fromHsvF(h, s, v, a);
    }
}


QColor Library::color(int id, int baseid) const
{
    if (baseid < 0 && (id == 16 || id == 24)) {
        qCWarning(LogLDraw) << "Called color() with meta color id" << id << "without specifying the base color";
        return {};
    } else if (id == 16) {
        return color(baseid);
    } else if (id == 24) {
        return edgeColor(baseid);
    } else if (m_colors.contains(id)) {
        return m_colors.value(id).color;
    } else if (id >= 256) {
        // dithered color (do a normal blend - DOS times are over by now)

        QColor c1 = color((id - 256) & 0x0f);
        QColor c2 = color(((id - 256) >> 4) & 0x0f);

        int r1, g1, b1, a1, r2, g2, b2, a2;
        c1.getRgb(&r1, &g1, &b1, &a1);
        c2.getRgb(&r2, &g2, &b2, &a2);

        return QColor::fromRgb((r1+r2) >> 1, (g1+g2) >> 1, (b1+b2) >> 1, (a1+a2) >> 1);
    } else {
        qCWarning(LogLDraw) << "Called color() with an invalid color id:" << id;
        return Qt::yellow;
    }
}

} // namespace LDraw

#include "moc_ldraw.cpp"
