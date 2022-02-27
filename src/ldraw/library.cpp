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

    if (updateStatus() == UpdateStatus::Updating)
        return false;

    QScopedValueRollback locker(m_locked, true);

    QString remotefile = "https://www.ldraw.org/library/updates/complete.zip"_l1;
    QString localfile = m_path;

    QDateTime dt;
    if (!force && QFile::exists(localfile))
        dt = m_lastUpdated.startOfDay().addDays(7);

    auto file = new QSaveFile(localfile);

    TransferJob *job = nullptr;
    if (file->open(QIODevice::WriteOnly)) {
        job = TransferJob::getIfNewer(QUrl(remotefile), dt, file);
        // job = TransferJob::get(QUrl(remotefile), file); // for testing only
        m_transfer->retrieve(job);
    }
    if (!job) {
        delete file;
        return false;
    }

    setUpdateStatus(UpdateStatus::Updating);

    locker.commit();

    connect(m_transfer, &Transfer::started,
            this, [this, job](TransferJob *j) {
        if (j != job)
            return;
        emit updateStarted();
    });
    connect(m_transfer, &Transfer::progress,
            this, [this, job](TransferJob *j, int done, int total) {
        if (j != job)
            return;
        emit updateProgress(done, total);
    });
    connect(m_transfer, &Transfer::finished,
            this, [this, job, file](TransferJob *j) -> QCoro::Task<> {
        if (j != job)
            co_return;

        Q_ASSERT(m_locked);
        m_locked = false;

        try {
            if (!job->isFailed() && job->wasNotModifiedSince()) {
                emit updateFinished(true, tr("Already up-to-date."));
                setUpdateStatus(UpdateStatus::Ok);
            } else if (job->isFailed()) {
                throw Exception(tr("download failed") % u": " % job->errorString());
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

std::tuple<QHash<int, Library::Color>, QDate> Library::parseLDconfig(const QByteArray &contents)
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
                colors.insert(c.id, c);
            } else {
                qCWarning(LogLDraw) << "Got invalid color " << c.id << " : " << c.color << " // " << c.edgeColor;
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


QColor Library::edgeColor(int id) const
{
    if (m_locked)
        return { };

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
    if (m_locked)
        return { };

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
        MiniZip::unzip(ldir, &buffer, "ldraw/LDConfig.ldr");
        auto [colors, date] = parseLDconfig(data); { }
        if (!colors.isEmpty()) {
            ok = true;
            generated = date;
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

} // namespace LDraw

#include "moc_library.cpp"
