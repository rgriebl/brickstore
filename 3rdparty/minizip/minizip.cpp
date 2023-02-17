// Copyright (C) 2004-2023 Robert Griebl
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

bool MiniZip::openInternal(bool parseTOC)
{
    if (m_zip)
        return false;

    static auto openCallback = [](void *, const void *filename, int mode) -> void * {
        if (filename && (mode == (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_EXISTING))) {
            auto f = std::make_unique<QFile>(*reinterpret_cast<const QString *>(filename));
            if (f->open(QIODevice::ReadOnly))
                return f.release();
        }
        return nullptr;
    };
    static auto readCallback = [](void *, void *stream, void *buf, uLong size) -> uLong {
        return uLong(stream ? reinterpret_cast<QFile *>(stream)->read(reinterpret_cast<char *>(buf), qint64(size)) : -1);
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


    zlib_filefunc64_def ffs { openCallback, readCallback, nullptr, tellCallback, seekCallback,
                closeCallback, errorCallback, nullptr };

    m_zip = unzOpen2_64(&m_zipFileName, &ffs);

    if (!m_zip)
        return false;

    if (parseTOC) {
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
            auto buffer = QString::fromUtf8(QByteArray(filename, filenameSize)).toLower().toUtf8();

            // this is slow, because unzGetCurrentFileInfo64() re-parses the file header
            // which was already done in GoTo{First|Next}File()
            //                QByteArray buffer;
            //                buffer.resize(1024);
            //                unz_file_info64 fileInfo;
            //                if (unzGetCurrentFileInfo64(zip, &fileInfo, buffer.data(), buffer.size(), nullptr, 0, nullptr, 0) != UNZ_OK)
            //                    break;

            //                buffer.truncate(fileInfo.size_filename);
            //                buffer.squeeze();

            m_contents.insert(buffer, qMakePair(fpos.pos_in_zip_directory, fpos.num_of_file));
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
        unzClose(m_zip);
        m_zip = nullptr;
    }
    m_contents.clear();
}

QStringList MiniZip::fileList() const
{
    QStringList l;
    l.reserve(m_contents.size());
    for (auto it = m_contents.keyBegin(); it != m_contents.keyEnd(); ++it)
        l << QString::fromUtf8(*it);
    return l;
}


QByteArray MiniZip::readFile(const QString &fileName)
{
    if (!m_zip)
        throw Exception(tr("ZIP file %1 has not been opened for reading")).arg(m_zipFileName);

    QByteArray fn = fileName.toLower().toUtf8();
    auto it = m_contents.constFind(fn);
    if (it == m_contents.cend())
        throw Exception(tr("Could not locate the file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);

    unz64_file_pos fpos { it.value().first, it.value().second };
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
    return data;
}

void MiniZip::unzip(const QString &zipFileName, QIODevice *destination,
                    const char *extractFileName, const char *extractPassword)
{
    QString errorMsg;
    MiniZip zip(zipFileName);

    if (zip.openInternal(false)) {
        if (unzLocateFile(zip.m_zip, extractFileName, 2 /*case insensitive*/) == UNZ_OK) {
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
