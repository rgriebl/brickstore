// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


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
    Exception(Exception &&move) noexcept;

    ~Exception() override = default;

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
