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
#include <QStringBuilder>
#include <QByteArray>
#include <QFile>

#include "utility.h"


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

    Exception(QFileDevice *f, const char *message)
        : Exception(QLatin1String(message) % u" (" % f->fileName() % u"): " % f->errorString())
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

    const char *what() const noexcept override;

protected:
    QString m_message;
private:
    mutable QByteArray whatBuffer;

};

class ParseException : public Exception
{
public:
    ParseException(const char *message)
        : Exception("Parse error: "_l1 + QLatin1String(message))
    { }

    ParseException(QIODevice *dev, const char *message)
        : Exception(QString::fromLatin1("Parse error%1: %2")
                    .arg(fileName(dev)).arg(QLatin1String(message)))
    { }

private:
    static QString fileName(QIODevice *dev)
    {
        if (auto file = qobject_cast<QFile *>(dev))
            return u" in file " % file->fileName();
        return { };
    }
};



