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

#pragma once

#include <QException>
#include <QString>
#include <QByteArray>
#include <QFile>


class Exception : public QException
{
public:
    Exception(const QString &message)
        : QException()
        , m_message(message)
    { }

    Exception(const char *message)
        : Exception(QString::fromLatin1(message))
    { }

    Exception(QFile *f, const char *message)
        : Exception(QString::fromLatin1("%1 (%2): %3")
                    .arg(QLatin1String(message)).arg(f->fileName()).arg(f->errorString()))
    { }

    template <typename T> Exception &arg(const T &t)
    {
        m_message = m_message.arg(t);
        return *this;
    }

    QString error() const
    {
        return m_message;
    }

    const char *what() const noexcept override
    {
        whatBuffer = m_message.toLocal8Bit();
        return whatBuffer.constData();
    }

protected:
    QString m_message;
private:
    mutable QByteArray whatBuffer;

};
