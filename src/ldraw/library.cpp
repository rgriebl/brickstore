// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cfloat>
#include <array>

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QtConcurrent>
#include <QCborValue>

#include <QCoro/QCoroFuture>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  include <tchar.h>
#  include <shlobj.h>
#endif

#include "utility/exception.h"
#include "utility/transfer.h"
#include "minizip/minizip.h"
#include "ldraw/library.h"
#include "ldraw/part.h"


Q_LOGGING_CATEGORY(LogLDraw, "ldraw")


namespace LDraw {

class PartLoaderJob
{
public:
    PartLoaderJob(const QString &file, const QString &path, QPromise<Part *> &&promise)
        : m_file(file)
        , m_path(path)
        , m_promise(std::move(promise))
    { }

    QString file() const { return m_file; }
    QString path() const { return m_path; }

    void start();
    void finish(Part *part);

private:
    QString m_file;
    QString m_path;
    QPromise<Part *> m_promise;
    bool m_started = false;
};

void PartLoaderJob::start()
{
    m_promise.start();
    m_started = true;
}

void PartLoaderJob::finish(Part *part)
{
    if (!m_started)
        start();
    if (part)
        part->addRef();
    m_promise.addResult(part);
    m_promise.finish();
    delete this;
}



Library *Library::s_inst = nullptr;

Library::Library(const QString &updateUrl, QObject *parent)
    : QObject(parent)
    , m_updateUrl(updateUrl)
    , m_transfer(new Transfer(this))
{
    m_cache.setMaxCost(50 * 1024 * 1024); // 50MB

    connect(m_transfer, &Transfer::progress,
            this, [this](TransferJob *j, int done, int total) {
        if (j != m_job)
            return;

        // delay updating the state, in case we get 304 (not modified) back
        if (done || total) {
            emitUpdateStartedIfNecessary();
            emit updateProgress(done, total);
        }
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
            if (!j->isFailed() && j->wasNotModified()) {
                // no need to emit updateFinished() here, because we didn't emit updateStarted()
                setUpdateStatus(UpdateStatus::Ok);
            } else if (j->isFailed()) {
                throw Exception(tr("download failed") + u": " + j->errorString());
            } else {
                QString etag = j->lastETag();
                QFile etagf(file->fileName() + u".etag");

                if (m_zip)
                    m_zip->close();
                if (!file->commit()) {
                    QString error = file->errorString(); // file is dead after the co_await
                    co_await setPath(m_path, true); // at least try to reload the old library
                    throw Exception(tr("saving failed") + u": " + error);
                }
                if (!co_await setPath(file->fileName(), true))
                    throw Exception(tr("reloading failed - please restart the application."));

                m_etag = etag;
                if (etagf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    etagf.write(m_etag.toUtf8());
                    etagf.close();
                }

                emitUpdateStartedIfNecessary();
                emit updateFinished(true, { });
                setUpdateStatus(UpdateStatus::Ok);
            }

        } catch (const Exception &e) {
            emitUpdateStartedIfNecessary();
            emit updateFinished(false, tr("Could not load the new parts library") + u": \n\n" + e.errorString());
            setUpdateStatus(UpdateStatus::UpdateFailed);
        }
        emit libraryReset();
    });
}

Library *Library::create(const QString &updateUrl)
{
    if (!s_inst)
        s_inst = new Library(updateUrl);
    return s_inst;
}

Library::~Library()
{
    shutdownPartLoaderThread();
}

QFuture<Part *> Library::partFromId(const QByteArray &id)
{
    QString filename = QLatin1String(id) + u".dat";
    return partFromFile(filename);
}

QFuture<Part *> Library::partFromBrickLinkId(const QByteArray &brickLinkId)
{
    QString ldrawId = QString::fromLatin1(brickLinkId);

    auto it = m_partIdMapping.constFind(ldrawId);
    if (it != m_partIdMapping.cend()) {
        qCDebug(LogLDraw) << "Mapped" << it.key() << "to" << it.value();
        if (it->isEmpty())
            return QtFuture::makeReadyFuture<Part *>(nullptr);
        ldrawId = *it;
    }

    QString filename = ldrawId + u".dat";
    return partFromFile(filename);
}

QFuture<Part *> Library::partFromFile(const QString &file)
{
    QPromise<Part *> promise;
    QFuture<Part *> result = promise.future();

    if (m_locked) {
        promise.start();
        promise.addResult(static_cast<Part *>(nullptr));
        promise.finish();
    } else {
        auto plj = new PartLoaderJob(file, QFileInfo(file).path(), std::move(promise));

        m_partLoaderMutex.lock();
        m_partLoaderJobs.append(plj);
        m_partLoaderCondition.wakeOne();
        m_partLoaderMutex.unlock();
    }
    return result;
}

void Library::partLoaderThread()
{
    while (!m_partLoaderShutdown) {
        m_partLoaderMutex.lock();
        m_partLoaderCondition.wait(&m_partLoaderMutex);

        if (m_partLoaderShutdown) {
            m_partLoaderMutex.unlock();
            continue;
        }

        PartLoaderJob *plj = m_partLoaderJobs.isEmpty() ? nullptr : m_partLoaderJobs.takeFirst();
        m_partLoaderMutex.unlock();

        if (!plj)
            continue;

        plj->start();
        auto *part = m_partLoaderShutdown ? nullptr : findPart(plj->file(), plj->path());
        plj->finish(part);
    }
}

void Library::startPartLoaderThread()
{
    if (!m_partLoaderThread)
        m_partLoaderThread.reset(QThread::create(&Library::partLoaderThread, this));
    m_partLoaderThread->start();
}

void Library::shutdownPartLoaderThread()
{
    if (m_partLoaderThread) {
        if (m_partLoaderThread->isRunning()) {
            m_partLoaderMutex.lock();
            m_partLoaderShutdown = 1;
            m_partLoaderCondition.wakeOne();
            m_partLoaderMutex.unlock();

            m_partLoaderThread->wait();
            m_partLoaderShutdown = 0;
        }
        m_partLoaderThread.reset();
    }
    for (auto *plj : std::as_const(m_partLoaderJobs))
        plj->finish(nullptr);

    m_partLoaderJobs.clear();

    // the parts in cache are referencing each other, so a plain clear will not work
    m_cache.clearRecursive();
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

    shutdownPartLoaderThread();

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
    m_isZip = (QFileInfo(path).suffix() == u"zip");

    m_zip.reset();
    m_searchpath.clear();
    m_partIdMapping.clear();

    if (valid && m_isZip) {
        QFileInfo(path).dir().mkpath(u"."_qs);

        caseSensitive = false;
        auto zip = std::make_unique<MiniZip>(path);

        if (co_await QtConcurrent::run([&zip]() { return zip->open(); }))
            m_zip.reset(zip.release());
        else
            valid = false;
    }
    if (valid) {
        try {
            if (readLDrawFile(u"LDConfig.ldr"_qs).isEmpty())
                valid = false;
        } catch (const Exception &) {
            valid = false;
        }
        try {
            auto cbor = readLDrawFile(u"brickstore-mappings.cbor"_qs);
            if (cbor.isEmpty())
                throw Exception(u"mapping file is empty"_qs);

            QCborParserError parseError;
            auto cborMap = QCborValue::fromCbor(cbor, &parseError);
            if (parseError.error != QCborError::NoError)
                throw Exception(u"CBOR parsing failed: %1"_qs).arg(parseError.errorString());

            const auto map = cborMap.toMap();
            if (map.isEmpty())
                throw Exception(u"CBOR map is empty"_qs);

            for (const auto &pair : map) {
                if (pair.first.isString() && (pair.second.isString() || pair.second.isNull()))
                    m_partIdMapping.insert(pair.first.toString(), pair.second.toString());
            }
            if (m_partIdMapping.size() != map.size()) {
                throw Exception(u"CBOR map parsing failed for %1 of %2 entries"_qs)
                    .arg(map.size() - m_partIdMapping.size()).arg(map.size());
            }
        } catch (const Exception &e) {
            qCWarning(LogLDraw) << "This ldraw library does not have a brickstore-mapping.cbor file:"
                                << e.errorString();
        }
    }

    if (valid) {
        static const std::array subdirs = { "LEGO", "Unofficial/p/48", "Unofficial/p", "p/48", "p",
                                           "Unofficial/parts", "parts", "models" };

        for (auto subdir : subdirs) {
            if (m_zip) {
                m_searchpath << QString(u"!ZIP!ldraw/" + QLatin1String(subdir));
            } else {
                QDir sdir(m_path);
                QString s = QString::fromLatin1(subdir);

                if (sdir.cd(s))
                    m_searchpath << sdir.canonicalPath();
                else if (caseSensitive && sdir.cd(s.toLower()))
                    m_searchpath << sdir.canonicalPath();
            }
        }

        m_etag.clear();
        if (m_zip) {
            QFile f(m_path + u".etag");
            if (f.open(QIODevice::ReadOnly))
                m_etag = QString::fromUtf8(f.readAll());
        }

        m_lastUpdated = m_zip ? QFileInfo(path).lastModified() : QDateTime { };
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
    if (valid) {
        startPartLoaderThread();
        emit lastUpdatedChanged(m_lastUpdated);
    }

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

    QString remotefile = u"https://" + m_updateUrl + u"/complete.zip";
    QString localfile = m_path;

    if (!QFile::exists(localfile))
        force = true;

    auto file = new QSaveFile(localfile);

    if (file->open(QIODevice::WriteOnly)) {
        m_job = TransferJob::getIfDifferent(QUrl(remotefile), force ? QString { } : m_etag, file);
        // m_job = TransferJob::get(QUrl(remotefile), file); // for testing only
        m_transfer->retrieve(m_job);
    }
    if (!m_job) {
        delete file;
        return false;
    }
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
        QString zipFilename = u"ldraw/" + filename;
        if (m_zip->contains(zipFilename))
            data = m_zip->readFile(zipFilename);
    } else {
        QFile f(path() + u'/' + filename);

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

void Library::emitUpdateStartedIfNecessary()
{
    if (updateStatus() != UpdateStatus::Updating) {
        setUpdateStatus(UpdateStatus::Updating);
        emit updateStarted();
    }
}

Part *Library::findPart(const QString &_filename, const QString &_parentdir)
{
    QString filename = _filename;
    filename.replace(u'\\', u'/');
    QString parentdir = _parentdir;
    if (!parentdir.isEmpty() && !parentdir.startsWith(u"!ZIP!"))
        parentdir = QDir(parentdir).canonicalPath();

    bool inZip = false;
    bool found = false;

    // add the logo on studs     //TODO: make this configurable
    if (filename == u"stud.dat")
        filename = u"stud-logo4.dat"_qs;
    else if (filename == u"stud2.dat")
        filename = u"stud2-logo4.dat"_qs;

    if (QFileInfo(filename).isRelative()) {
        // search order is parentdir => p => parts => models

        QStringList searchpath = m_searchpath;
        if (!parentdir.isEmpty() && !searchpath.contains(parentdir))
            searchpath.prepend(parentdir);

        for (const QString &sp : std::as_const(searchpath)) {

            if (sp.startsWith(u"!ZIP!")) {
                filename = filename.toLower();
                QString testname = sp.mid(5) + u'/' + filename;
                if (m_zip->contains(testname)) {
                    QFileInfo fi(filename);
                    parentdir = sp;
                    if (fi.path() != u".")
                        parentdir = parentdir + u'/' + fi.path();
                    filename = testname;
                    inZip = true;
                    found = true;
                    break;
                }
            } else {
                QString testname = sp + u'/' + filename;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)
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
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)
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
                qCWarning(LogLDraw) << "Failed to read from LDraw ZIP:" << e.errorString();
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


bool Library::checkLDrawDir(const QString &ldir)
{
    bool ok = false;
    QFileInfo fi(ldir);

    if (fi.exists() && fi.isDir() && fi.isReadable()) {
        QDir dir(ldir);

        if (dir.cd(u"p"_qs) || dir.cd(u"P"_qs)) {
            if (dir.exists(u"stud.dat"_qs) || dir.exists(u"STUD.DAT"_qs)) {
                if (dir.cd(u"../parts"_qs) || dir.cd(u"../PARTS"_qs)) {
                    if (dir.exists(u"3001.dat"_qs) || dir.exists(u"3001.DAT"_qs)) {
                        QFile f(ldir + u"/LDConfig.ldr");
                        if (f.open(QIODevice::ReadOnly) && f.size()) {
                            ok = true;
                        }
                    }
                }
            }
        }
    } else if (fi.exists() && fi.isFile() && fi.suffix() == u"zip") {
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        try {
            MiniZip::unzip(ldir, &buffer, "ldraw/LDConfig.ldr");
            if (!data.isEmpty()) {
                ok = true;
            }
        } catch (const Exception &) {
        }
    }
    return ok;
}

QPair<int, int> Library::partCacheStats() const
{
    return qMakePair(m_cache.totalCost(), m_cache.maxCost());
}

QStringList Library::potentialLDrawDirs()
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
        unixdirs << u"~/ldraw"_qs;

#  if defined(Q_OS_MACOS)
        unixdirs << u"~/Library/ldraw"_qs
                 << u"~/Library/LDRAW"_qs
                 << u"/Library/LDRAW"_qs
                 << u"/Library/ldraw"_qs;
#  else
        unixdirs << u"/usr/share/ldraw"_qs;
        unixdirs << u"/var/lib/ldraw"_qs;
        unixdirs << u"/var/ldraw"_qs;
#  endif

        QString homepath = QDir::homePath();

        foreach (QString d, unixdirs) {
            d.replace(u"~"_qs, homepath);
            dirs << d;
        }
    }
#endif

    QStringList result;

    for (const auto &dir : std::as_const(dirs)) {
        if (!dir.isEmpty() && checkLDrawDir(dir))
            result.append(dir);
    }

    return result;
}


} // namespace LDraw

#include "moc_library.cpp"
