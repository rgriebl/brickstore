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
#include <QIODevice>

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
#include "minizip.h"
#include "stopwatch.h"


MiniZip::MiniZip(const QString &zipFilename)
    : m_zipFileName(zipFilename)
{ }

MiniZip::~MiniZip()
{
    close();
}

bool MiniZip::open()
{
    if (m_zip)
        return false;

#ifdef USEWIN32IOAPI
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64W(&ffunc);
    const ushort *fn = m_zipFileName.utf16();

    m_zip = unzOpen2_64(fn, &ffunc);
#else
    QByteArray utf8Fn = m_zipFileName.toUtf8();
    const char *fn = utf8Fn.constData();

    m_zip = unzOpen64(fn);
#endif
    if (!m_zip)
        return false;

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
        throw Exception(tr("Could not seek to thefile %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);

    unz_file_info64 fileInfo;
    if (unzGetCurrentFileInfo64(m_zip, &fileInfo, 0, 0, 0, 0, 0, 0) != UNZ_OK)
        throw Exception(tr("Could not get info for the file %1 within the ZIP file %2.")).arg(fileName).arg(m_zipFileName);

    if (fileInfo.uncompressed_size >= 0x8000000ULL)
        throw Exception(tr("The file %1 within the ZIP file %3 is too big (%2 bytes).")).arg(fileName).arg(fileInfo.uncompressed_size).arg(m_zipFileName);

    if (unzOpenCurrentFile(m_zip) != UNZ_OK)
        throw Exception(tr("Could not open the file %1 within the ZIP file %2 for reading.")).arg(fileName).arg(m_zipFileName);

    QByteArray data;
    data.resize(fileInfo.uncompressed_size);
    if (unzReadCurrentFile(m_zip, data.data(), data.size()) != data.size()) {
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

    if (zip.open()) {
        if (unzLocateFile(zip.m_zip, extractFileName, 2 /*case insensitive*/) == UNZ_OK) {
            if (unzOpenCurrentFilePassword(zip.m_zip, extractPassword) == UNZ_OK) {
                QByteArray block;
                block.resize(1024*1024);
                int bytesRead;
                do {
                    bytesRead = unzReadCurrentFile(zip.m_zip, block.data(), block.size());
                    if (bytesRead > 0)
                        destination->write(block.constData(), bytesRead);
                } while (bytesRead > 0);
                if (bytesRead < 0)
                    errorMsg = tr("Could not extract model.ldr from the Studio ZIP file.");
                unzCloseCurrentFile(zip.m_zip);
            } else {
                errorMsg = tr("Could not decrypt the model.ldr within the Studio ZIP file.");
            }
        } else {
            errorMsg = tr("Could not locate the model.ldr file within the Studio ZIP file.");
        }
        zip.close();
    } else {
        errorMsg = tr("Could not open the Studio ZIP file");
    }
    if (!errorMsg.isEmpty())
        throw Exception(errorMsg);
}
