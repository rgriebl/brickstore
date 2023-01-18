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

#pragma once

#include <memory>

#include <QException>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QIODevice)
QT_FORWARD_DECLARE_CLASS(QFileDevice)


class Exception : public QException
{
public:
    explicit Exception(const QString &message = { });
    explicit Exception(const char *message);
    explicit Exception(QFileDevice *f, const QString &message);
    explicit Exception(QFileDevice *f, const char *message);

    Exception(const Exception &copy);
    Exception(Exception &&move);

    virtual ~Exception() = default;

    template <typename... Ts> inline Exception &arg(const Ts & ...ts)
    {
        m_errorString = m_errorString.arg(ts...);
        return *this;
    }

    inline QString errorString() const  { return m_errorString; }
    const char *what() const noexcept override;

protected:
    static QString fileMessage(QFileDevice *f);

    QString m_errorString;

private:
    mutable std::unique_ptr<QByteArray> m_whatBuffer;
};

class ParseException : public Exception
{
public:
    ParseException(const char *message);
    ParseException(QIODevice *dev, const char *message);

private:
    static QString fileName(QIODevice *dev);
};
