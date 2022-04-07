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
#include "library.h"
#include "part.h"


Q_LOGGING_CATEGORY(LogLDraw, "ldraw")


namespace LDraw {

Library *Library::s_inst = nullptr;

Library::Library(QObject *parent)
    : QObject(parent)
    , m_transfer(new Transfer(this))
{
    m_cache.setMaxCost(50 * 1024 * 1024); // 50MB

    connect(m_transfer, &Transfer::started,
            this, [this](TransferJob *j) {
        if (j != m_job)
            return;
        qWarning() << "LDRAW UPDATE STARTED";
        emit updateStarted();
    });
    connect(m_transfer, &Transfer::progress,
            this, [this](TransferJob *j, int done, int total) {
        if (j != m_job)
            return;
        qWarning() << "LDRAW UPDATE Progress" << done << total;
        emit updateProgress(done, total);
    });
    connect(m_transfer, &Transfer::finished,
            this, [this](TransferJob *j) -> QCoro::Task<> {
        if (j != m_job)
            co_return;

        Q_ASSERT(m_locked);
        m_locked = false;

        auto *file = qobject_cast<QSaveFile *>(j->file());
        Q_ASSERT(file);

        m_job = nullptr;

        try {
            if (!j->isFailed() && j->wasNotModifiedSince()) {
                emit updateFinished(true, tr("Already up-to-date."));
                setUpdateStatus(UpdateStatus::Ok);
            } else if (j->isFailed()) {
                throw Exception(tr("download failed") % u": " % j->errorString());
            } else {
                if (m_zip)
                    m_zip->close();
                if (!file->commit()) {
                    setPath(m_path, true); // at least try to reload the old library
                    throw Exception(tr("saving failed") % u": " % file->errorString());
                }
                if (!co_await setPath(file->fileName(), true))
                    throw Exception(tr("reloading failed - please restart the application."));

                emit updateFinished(true, { });
                setUpdateStatus(UpdateStatus::Ok);
            }

        } catch (const Exception &e) {
            emit updateFinished(false, tr("Could not load the new parts library") % u": \n\n" % e.error());
            setUpdateStatus(UpdateStatus::UpdateFailed);
        }
        emit libraryReset();
    });

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
    if (m_locked)
        return nullptr;

    QString filename = QLatin1String(id) % u".dat";
    return findPart(filename);
}


QString Library::path() const
{
    return m_path;
}

QCoro::Task<bool> Library::setPath(const QString &path, bool forceReload)
{
    if ((m_path == path) && !forceReload)
        co_return false;

    emit libraryAboutToBeReset();

    m_cache.clearRecursive();
    if (!m_cache.isEmpty()) {
        emit libraryReset();
        co_return false;
    }

    bool caseSensitive =
#if defined(Q_OS_LINUX)
            true;
#else
            false;
#endif
    bool valid = !path.isEmpty();
    m_path = path;
    m_isZip = (QFileInfo(path).suffix() == "zip"_l1);

    m_zip.reset();
    m_searchpath.clear();

    if (valid && m_isZip) {
        QFileInfo(path).dir().mkpath("."_l1);

        caseSensitive = false;
        auto zip = std::make_unique<MiniZip>(path);

        if (co_await QtConcurrent::run([&zip]() { return zip->open(); }))
            m_zip.reset(zip.release());
        else
            valid = false;
    }
    if (valid) {
        try {
            auto ldConfig = readLDrawFile("LDConfig.ldr"_l1);
            std::tie(m_colors, m_lastUpdated) = Library::parseLDconfig(ldConfig);
            if (m_colors.isEmpty())
                valid = false;
        } catch (const Exception &) {
            valid = false;
        }
    }

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
    return startUpdate(false);
}

bool Library::startUpdate(bool force)
{
    if (!m_isZip)
        return false;

    if (m_job || (updateStatus() == UpdateStatus::Updating))
        return false;

    QScopedValueRollback locker(m_locked, true);

    QString remotefile = "https://www.ldraw.org/library/updates/complete.zip"_l1;
    QString localfile = m_path;

    QDateTime dt;
    if (!force && QFile::exists(localfile)) {
        // m_lastUpdate, aka the date in LDConfig.ldr ist _just_ for the config itself,
        // not for the whole library
        dt = QFileInfo(localfile).lastModified();
    }

    auto file = new QSaveFile(localfile);

    if (file->open(QIODevice::WriteOnly)) {
        m_job = TransferJob::getIfNewer(QUrl(remotefile), dt, file);
        // m_job = TransferJob::get(QUrl(remotefile), file); // for testing only
        m_transfer->retrieve(m_job);
    }
    if (!m_job) {
        delete file;
        return false;
    }

    setUpdateStatus(UpdateStatus::Updating);

    locker.commit();

    return true;
}

void Library::cancelUpdate()
{
    if (m_updateStatus == UpdateStatus::Updating)
        m_transfer->abortAllJobs();
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

void Library::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}

std::tuple<QHash<int, Color>, QDate> Library::parseLDconfig(const QByteArray &contents)
{
    QHash<int, Color> colors;
    QDate date;

    QTextStream ts(contents);

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
            // 0 !COLOUR name CODE x VALUE v EDGE e [ALPHA a] [LUMINANCE l] [ CHROME | PEARLESCENT | RUBBER | MATTE_METALLIC | METAL | MATERIAL <params> ]

            Color c;

            c.m_id = sl[4].toInt();
            c.m_name = sl[2];
            c.m_color = sl[6];
            c.m_edgeColor = sl[8];

            for (int idx = 9; idx < sl.count(); ++idx) {
                if (sl[idx] == "ALPHA"_l1) {
                    int alpha = sl[++idx].toInt();
                    c.m_color.setAlpha(alpha);
                    //c.edgeColor.setAlpha(alpha);
                } else if (sl[idx] == "LUMINANCE"_l1) {
                    c.m_luminance = float(sl[++idx].toInt()) / 255;
                } else if (sl[idx] == "CHROME"_l1) {
                    c.m_chrome = true;
                } else if (sl[idx] == "PEARLESCENT"_l1) {
                    c.m_pearlescent = true;
                } else if (sl[idx] == "RUBBER"_l1) {
                    c.m_rubber = true;
                } else if (sl[idx] == "MATTE_METALLIC"_l1) {
                    c.m_matteMetallic = true;
                } else if (sl[idx] == "METAL"_l1) {
                    c.m_metal = true;
                }
            }
            if (c.m_color.isValid() && c.m_edgeColor.isValid()) {
                colors.insert(c.m_id, c);
            } else {
                qCWarning(LogLDraw) << "Got invalid color " << c.m_id << " : " << c.m_color << " // " << c.m_edgeColor;
            }
        } else if (sl.count() >= 5 &&
                   sl[0].toInt() == 0 &&
                   sl[1] == "!LDRAW_ORG"_l1 &&
                   sl[2] == "Configuration"_l1 &&
                   sl[3] == "UPDATE"_l1) {
            // 0 !LDRAW_ORG Configuration UPDATE yyyy-MM-dd
            date = QDate::fromString(sl[4], "yyyy-MM-dd"_l1);
        }
    }

    return { colors, date };
}


Color Library::color(int id, int baseId) const
{
    if (m_locked)
        return { };

    if ((id == 16) || (id == 24)) {
        if (baseId < 0) {
            qCWarning(LogLDraw) << "Called color() with meta color id" << id << "without specifying the base color";
            return { };
        }
        return color(baseId, -1);

    } else if (m_colors.contains(id)) {
        return m_colors.value(id);

    } else if (id >= 256) {
        Color c = m_colors.value(0); // black

        // dithered color (do a normal blend - DOS times are over by now)

        QColor c1 = color((id - 256) & 0x0f).m_color;
        QColor c2 = color(((id - 256) >> 4) & 0x0f).m_color;

        int r1, g1, b1, a1, r2, g2, b2, a2;
        c1.getRgb(&r1, &g1, &b1, &a1);
        c2.getRgb(&r2, &g2, &b2, &a2);

        c.m_name = "Dithered Color "_l1 + QString::number(id);
        c.m_id = id;
        c.m_color = QColor::fromRgb((r1+r2) >> 1, (g1+g2) >> 1, (b1+b2) >> 1, (a1+a2) >> 1);
        // edge color of dithered colors is derived from color 1
        c.m_edgeColor = color((id - 256) & 0x0f).m_edgeColor;
        return c;
    } else {
        qCWarning(LogLDraw) << "Called color() with an invalid color id:" << id;
        return { };
    }

#if 0
    // calculate a contrasting edge color

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qreal h, s, v, a;
#else
    float h, s, v, a;
#endif
    color(id).getHsvF(&h, &s, &v, &a);

    v += 0.5f * ((v < 0.5f) ? 1 : -1);
    v = qBound<decltype(v)>(0, v, 1);

    return QColor::fromHsvF(h, s, v, a);
#endif
}

Part *Library::findPart(const QString &_filename, const QString &_parentdir)
{
    QString filename = _filename;
    filename.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString parentdir = _parentdir;
    if (!parentdir.isEmpty() && !parentdir.startsWith("!ZIP!"_l1))
        parentdir = QDir(parentdir).canonicalPath();

    bool inZip = false;
    bool found = false;

    // add the logo on studs
    if (filename == "stud.dat"_l1)
        filename = "stud-logo4.dat"_l1;
    else if (filename == "stud2.dat"_l1)
        filename = "stud2-logo4.dat"_l1;

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


std::tuple<bool, QDate> Library::checkLDrawDir(const QString &ldir)
{
    bool ok = false;
    QDate generated;

    QFileInfo fi(ldir);

    if (fi.exists() && fi.isDir() && fi.isReadable()) {
        QDir dir(ldir);

        if (dir.cd("p"_l1) || dir.cd("P"_l1)) {
            if (dir.exists("stud.dat"_l1) || dir.exists("STUD.DAT"_l1)) {
                if (dir.cd("../parts"_l1) || dir.cd("../PARTS"_l1)) {
                    if (dir.exists("3001.dat"_l1) || dir.exists("3001.DAT"_l1)) {
                        QFile f(ldir % "/LDConfig.ldr"_l1);
                        if (f.open(QIODevice::ReadOnly)) {
                            auto [colors, date] = parseLDconfig(f.readAll()); { }
                            if (!colors.isEmpty()) {
                                ok = true;
                                generated = date;
                            }
                        }
                    }
                }
            }
        }
    } else if (fi.exists() && fi.isFile() && fi.suffix() == "zip"_l1) {
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        try {
            MiniZip::unzip(ldir, &buffer, "ldraw/LDConfig.ldr");
            auto [colors, date] = parseLDconfig(data); { }
            if (!colors.isEmpty()) {
                ok = true;
                generated = date;
            }
        } catch (const Exception &) {
        }
    }
    return { ok, generated };
}

QVector<std::tuple<QString, QDate>> Library::potentialLDrawDirs()
{
    QStringList dirs;
    dirs << QString::fromLocal8Bit(qgetenv("LDRAWDIR"));

#if defined(Q_OS_WINDOWS)
    {
        wchar_t inidir [MAX_PATH];

        DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
        if (l >= 0) {
            inidir [l] = 0;
            dirs << QString::fromWCharArray(inidir);
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
                    dirs << QString::fromWCharArray(regdir);
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

    QVector<std::tuple<QString, QDate>> result;

    for (auto dir = dirs.begin(); dir != dirs.end(); ++dir) {
        if (!dir->isEmpty()) {
            auto [ok, date] = checkLDrawDir(*dir); { }
            if (ok) {
                result.append({ *dir, date });
            }
        }
    }

    return result;
}

void registerQmlTypes()
{
    qRegisterMetaType<Color>("LDrawColor");
    qmlRegisterUncreatableType<Color>("BrickStore", 1, 0, "LDrawColor",
                                      "Cannot create objects of type %1"_l1.arg("LDrawColor"_l1));
}

} // namespace LDraw

#include "moc_library.cpp"
