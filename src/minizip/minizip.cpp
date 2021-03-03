/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include "exception.h"
#include "minizip.h"


void MiniZip::unzip(const QString &zipFileName, QIODevice *destination,
                    const char *extractFileName, const char *extractPassword)
{
    unzFile zip = { };
    QString errorMsg;

#ifdef USEWIN32IOAPI
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64W(&ffunc);
    const ushort *fn = zipFileName.utf16();

    zip = unzOpen2_64(fn, &ffunc);
#else
    QByteArray utf8Fn = zipFileName.toUtf8();
    const char *fn = utf8Fn.constData();

    zip = unzOpen64(fn);
#endif

    if (zip) {
        if (unzLocateFile(zip, extractFileName, 2 /*case insensitive*/) == UNZ_OK) {
            if (unzOpenCurrentFilePassword(zip, extractPassword) == UNZ_OK) {
                QByteArray block;
                block.resize(1024*1024);
                int bytesRead;
                do {
                    bytesRead = unzReadCurrentFile(zip, block.data(), block.size());
                    if (bytesRead > 0)
                        destination->write(block.constData(), bytesRead);
                } while (bytesRead > 0);
                if (bytesRead < 0)
                    errorMsg = tr("Could not extract model.ldr from the Studio ZIP file.");
                unzCloseCurrentFile(zip);
            } else {
                errorMsg = tr("Could not decrypt the model.ldr within the Studio ZIP file.");
            }
        } else {
            errorMsg = tr("Could not locate the model.ldr file within the Studio ZIP file.");
        }
        unzClose(zip);
    } else {
        errorMsg = tr("Could not open the Studio ZIP file");
    }
    if (!errorMsg.isEmpty())
        throw Exception(errorMsg);
}
