// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QFile>

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
#  error "The read() optimizations in unzip.c are incompatible with big endian machines"
#endif


// vvv copied from minizip's miniunz.c
#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
#  ifndef __USE_FILE_OFFSET64
#    define __USE_FILE_OFFSET64
#  endif
#  ifndef __USE_LARGEFILE64
#    define __USE_LARGEFILE64
#  endif
#  ifndef _LARGEFILE64_SOURCE
#    define _LARGEFILE64_SOURCE
#  endif
#  ifndef _FILE_OFFSET_BIT
#    define _FILE_OFFSET_BIT 64
#  endif
#endif

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#  define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#  define FTELLO_FUNC(stream) ftello(stream)
#  define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#  define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#  define FTELLO_FUNC(stream) ftello64(stream)
#  define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif

#include "unzip.h"
#include "zip.h"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME (256)

#ifdef _WIN32
#  define USEWIN32IOAPI
#  include "iowin32.h"
#endif
// ^^^ copied from minizip's miniunz.c

#include "utility/exception.h"
#include "utility/stopwatch.h"
#include "minizip.h"


MiniZip::MiniZip(const QString &zipFilename)
    : m_zipFileName(zipFilename)
{ }

MiniZip::~MiniZip()
{
    close();
}

bool MiniZip::open()
{
    return openInternal(true);
}

bool MiniZip::create()
{
    if (m_zip)
        return false;
    m_writing = true;
    return openInternal(false);
}

bool MiniZip::openInternal(bool parseTOC)
{
    if (m_zip)
        return false;

    static auto openCallback = [](void *, const void *filename, int mode) -> void * {
        if (filename) {
            auto f = std::make_unique<QFile>(*reinterpret_cast<const QString *>(filename));
            QIODevice::OpenMode m;
            if (mode & ZLIB_FILEFUNC_MODE_READ)
                m |= QIODevice::ReadOnly;
            if (mode & ZLIB_FILEFUNC_MODE_WRITE)
                m |= QIODevice::WriteOnly;
            if (mode & ZLIB_FILEFUNC_MODE_CREATE)
                m |= QIODevice::Truncate;
            if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
                m |= QIODevice::Append;
            if (f->open(m))
                return f.release();
        }
        return nullptr;
    };
    static auto readCallback = [](void *, void *stream, void *buf, uLong size) -> uLong {
        return uLong(stream ? reinterpret_cast<QFile *>(stream)->read(reinterpret_cast<char *>(buf), qint64(size)) : -1);
    };
    static auto writeCallback = [](void *, void *stream, const void *buf, uLong size) -> uLong {
        return uLong(stream ? reinterpret_cast<QFile *>(stream)->write(reinterpret_cast<const char *>(buf), qint64(size)) : -1);
    };

    static auto tellCallback = [](void *, void *stream) -> quint64 {
        return quint64(stream ? reinterpret_cast<QFile *>(stream)->pos() : -1);
    };
    static auto seekCallback = [](void *, void *stream, quint64 pos, int origin) -> long {
        if (auto *f = reinterpret_cast<QFile *>(stream)) {
            qint64 ipos = qint64(pos);
            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_END: ipos = f->size() - ipos; break;
            case ZLIB_FILEFUNC_SEEK_SET: break;
            case ZLIB_FILEFUNC_SEEK_CUR: ipos = f->pos() + ipos; break;
            default: return -1;
            }
            return f->seek(ipos) ? 0 : -1;
        }
        return -1;
    };
    static auto closeCallback = [](void *, void *stream) -> int {
        if (stream)
            delete reinterpret_cast<QFile *>(stream);
        return 0;
    };
    static auto errorCallback = [](void *, void *stream) -> int {
        return stream && (reinterpret_cast<QFile *>(stream)->error() == QFileDevice::NoError) ? 0 : -1;
    };


    zlib_filefunc64_def ffs { openCallback, readCallback, writeCallback, tellCallback, seekCallback,
                closeCallback, errorCallback, nullptr };

    if (m_writing)
        m_zip = zipOpen2_64(&m_zipFileName, APPEND_STATUS_CREATE, nullptr, &ffs);
    else
        m_zip = unzOpen2_64(&m_zipFileName, &ffs);

    if (!m_zip)
        return false;

    if (parseTOC && !m_writing) {
        stopwatch sw("Reading ZIP directory");
        do {
            unz64_file_pos fpos;
            if (unzGetFilePos64(m_zip, &fpos) != UNZ_OK)
                break;

            // extension for BrickStore for fast content scanning (50% faster)
            char *filename;
            int filenameSize;
            if (unz__GetCurrentFilename(m_zip, &filename, &filenameSize) != UNZ_OK)
                break;
            QByteArray fn(filename, filenameSize);
            auto key = QString::fromUtf8(fn).toLower().toUtf8();

            // this is slow, because unzGetCurrentFileInfo64() re-parses the file header
            // which was already done in GoTo{First|Next}File()
            //                QByteArray buffer;
            //                buffer.resize(1024);
            //                unz_file_info64 fileInfo;
            //                if (unzGetCurrentFileInfo64(zip, &fileInfo, buffer.data(), buffer.size(), nullptr, 0, nullptr, 0) != UNZ_OK)
            //                    break;

            //                buffer.truncate(fileInfo.size_filename);
            //                buffer.squeeze();

            m_contents.insert(key, { fn, fpos.pos_in_zip_directory, fpos.num_of_file });
        } while (unzGoToNextFile(m_zip) == UNZ_OK);
    }

    return true;
}

bool MiniZip::contains(const QString &fileName) const
{
    return m_contents.contains(fileName.toLower().toUtf8());
}

void MiniZip::close()
{
    if (m_zip) {
        if (m_writing)
            zipClose(m_zip, nullptr);
        else
            unzClose(m_zip);
        m_zip = nullptr;
    }
    m_contents.clear();
}

bool MiniZip::isOpen() const
{
    return (m_zip);
}

QString MiniZip::fileName() const
{
    return m_zipFileName;
}

QStringList MiniZip::fileList() const
{
    QStringList l;
    l.reserve(m_contents.size());
    for (auto it = m_contents.cbegin(); it != m_contents.cend(); ++it)
        l << QString::fromUtf8(std::get<0>(it.value()));
    return l;
}


std::tuple<QByteArray, QDateTime> MiniZip::readFileAndLastModified(const QString &fileName)
{
    if (!m_zip || m_writing)
        throw Exception(tr("ZIP file %1 has not been opened for reading")).arg(m_zipFileName);

    QByteArray fn = fileName.toLower().toUtf8();
    auto it = m_contents.constFind(fn);
    if (it == m_contents.cend())
        throw Exception(tr("Could not locate the file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);

    unz64_file_pos fpos { std::get<1>(it.value()), std::get<2>(it.value()) };
    if (unzGoToFilePos64(m_zip, &fpos) != UNZ_OK)
        throw Exception(tr("Could not seek to the file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);

    unz_file_info64 fileInfo;
    if (unzGetCurrentFileInfo64(m_zip, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
        throw Exception(tr("Could not get info for the file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);

    if (fileInfo.uncompressed_size >= 0x8000000ULL)
        throw Exception(tr("The file %1 within the ZIP file %3 is too big (%2 bytes).")).arg(fileName).arg(fileInfo.uncompressed_size).arg(m_zipFileName);

    if (unzOpenCurrentFile(m_zip) != UNZ_OK)
        throw Exception(tr("Could not open the file %1 within the ZIP file %2 for reading.")).arg(fileName).arg(m_zipFileName);

    QByteArray data;
    data.resize(int(fileInfo.uncompressed_size));
    if (unzReadCurrentFile(m_zip, data.data(), unsigned(data.size())) != data.size()) {
        data.clear();
        unzCloseCurrentFile(m_zip);
        throw Exception(tr("Could not read the file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);
    }
    unzCloseCurrentFile(m_zip);

    const auto &tm = fileInfo.tmu_date;
    QDateTime lastModified(QDate { tm.tm_year, tm.tm_mon + 1, tm.tm_mday},
                           QTime { tm.tm_hour, tm.tm_min, tm.tm_sec });
    return { data, lastModified };
}

void MiniZip::writeFile(const QString &fileName, const QByteArray &data, const QDateTime &dateTime)
{
    if (!m_zip || !m_writing)
        throw Exception(tr("ZIP file %1 has not been opened for writing")).arg(m_zipFileName);

    auto dt = dateTime;
    if (!dt.isValid())
        dt = QDateTime::currentDateTime();

    zip_fileinfo info { { dt.time().second(), dt.time().minute(), dt.time().hour(),
                       dt.date().day(), dt.date().month() - 1, dt.date().year() },
                      0, 0, 0 };

    if (zipOpenNewFileInZip(m_zip, fileName.toUtf8().constData(), &info, nullptr, 0,
                            nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK) {
        throw Exception(tr("Could not create file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);
    }
    if (zipWriteInFileInZip(m_zip, data.constData(), unsigned(data.size())) != ZIP_OK) {
        zipCloseFileInZip(m_zip);
        throw Exception(tr("Could not write to file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);
    }
    zipCloseFileInZip(m_zip);
}

QByteArray MiniZip::readFile(const QString &fileName)
{
    return std::get<0>(readFileAndLastModified(fileName));
}

void MiniZip::unzip(const QString &zipFileName, QIODevice *destination,
                    const char *extractFileName, const char *extractPassword)
{
    QString errorMsg;
    MiniZip zip(zipFileName);

    if (zip.openInternal(false)) {
        if (unzLocateFile(zip.m_zip, extractFileName, 2 /*case insensitive*/) == UNZ_OK) {
            if (extractPassword) { // check if the file is actually encrypted
                unz_file_info64 ufi;
                ::memset(&ufi, 0, sizeof(ufi));
                unzGetCurrentFileInfo64(zip.m_zip, &ufi, nullptr, 0, nullptr, 0, nullptr, 0);
                if (!(ufi.flag & 1)) // bit 1 = encrypted
                    extractPassword = nullptr;
            }

            if (unzOpenCurrentFilePassword(zip.m_zip, extractPassword) == UNZ_OK) {
                QByteArray block;
                block.resize(1024*1024);
                int bytesRead;
                do {
                    bytesRead = unzReadCurrentFile(zip.m_zip, block.data(), unsigned(block.size()));
                    if (bytesRead > 0)
                        destination->write(block.constData(), bytesRead);
                } while (bytesRead > 0);
                if (bytesRead < 0)
                    errorMsg = tr("Could not read the file %1 within the ZIP file %2.")
                            .arg(QLatin1String(extractFileName)).arg(zipFileName);
                unzCloseCurrentFile(zip.m_zip);
            } else {
                errorMsg = tr("Could not decrypt the file %1 within the ZIP file %2.")
                        .arg(QLatin1String(extractFileName)).arg(zipFileName);
            }
        } else {
            errorMsg = tr("Could not locate the file %1 within the ZIP file %2.")
                    .arg(QLatin1String(extractFileName)).arg(zipFileName);
        }
        zip.close();
    } else {
        errorMsg = tr("Could not open the ZIP file %1").arg(zipFileName);
    }
    if (!errorMsg.isEmpty())
        throw Exception(errorMsg);
}
