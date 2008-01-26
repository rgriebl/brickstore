/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CMAPPEDFILE_H__
#define __CMAPPEDFILE_H__

#include <QGlobalStatic>
#include <QFile>
#include <QByteArray>

#if defined( Q_OS_WIN32 )
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#endif

class QDataStream;


class CMappedFile {
public:
    CMappedFile(const QString &filename);
    ~CMappedFile();

    QDataStream *open();
    void close();

    quint32 size() const;

private:
    QFile        m_file;
    quint32      m_filesize;
    const char * m_memptr;
    QByteArray   m_mem;
    QDataStream *m_ds;

#if defined( Q_OS_WIN32 )
    HANDLE       m_maphandle;
#endif
};

#endif
