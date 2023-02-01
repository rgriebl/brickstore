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
#include <QByteArray>
#include <QFile>

#include "exception.h"

Exception::Exception(const QString &message)
    : QException()
    , m_errorString(message)
{ }

Exception::Exception(const char *message)
    : Exception(QString::fromLatin1(message))
{ }

Exception::Exception(QFileDevice *f, const QString &message)
    : Exception(message + fileMessage(f))
{ }

Exception::Exception(QFileDevice *f, const char *message)
    : Exception(QLatin1String(message) + fileMessage(f))
{ }

Exception::Exception(const Exception &copy)
    : m_errorString(copy.m_errorString)
{ }

Exception::Exception(Exception &&move)
    : m_errorString(move.m_errorString)
{
    std::swap(m_whatBuffer, move.m_whatBuffer);
}

const char *Exception::what() const noexcept
{
    if (!m_whatBuffer)
        m_whatBuffer.reset(new QByteArray);
    *m_whatBuffer = m_errorString.toLocal8Bit();
    return m_whatBuffer->constData();
}

QString Exception::fileMessage(QFileDevice *f)
{
    return f ? QString(u" (" + f->fileName() + u"): " + f->errorString()) : QString();
}



ParseException::ParseException(const char *message)
    : Exception(u"Parse error: "_qs + QLatin1String(message))
{ }

ParseException::ParseException(QIODevice *dev, const char *message)
    : Exception(u"Parse error%1: %2"_qs.arg(fileName(dev)).arg(QLatin1String(message)))
{ }

QString ParseException::fileName(QIODevice *dev)
{
    if (auto file = qobject_cast<QFile *>(dev))
        return u" in file " + file->fileName();
    return { };
}
