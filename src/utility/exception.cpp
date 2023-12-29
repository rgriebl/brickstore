// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <memory>
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
    : Exception(QString::fromLatin1(message) + fileMessage(f))
{ }

Exception::Exception(const Exception &copy)
    : m_errorString(copy.m_errorString)
{ }

Exception::Exception(Exception &&move) noexcept
    : m_errorString(std::move(move.m_errorString))
{
    std::swap(m_whatBuffer, move.m_whatBuffer);
}

const char *Exception::what() const noexcept
{
    if (!m_whatBuffer)
        m_whatBuffer = std::make_unique<QByteArray>();
    *m_whatBuffer = m_errorString.toLocal8Bit();
    return m_whatBuffer->constData();
}

QString Exception::fileMessage(QFileDevice *f)
{
    return f ? QString(u" (" + f->fileName() + u"): " + f->errorString()) : QString();
}



ParseException::ParseException(const char *message)
    : Exception(u"Parse error: "_qs + QString::fromLatin1(message))
{ }

ParseException::ParseException(QIODevice *dev, const char *message)
    : Exception(u"Parse error%1: %2"_qs.arg(fileName(dev)).arg(QString::fromLatin1(message)))
{ }

QString ParseException::fileName(QIODevice *dev)
{
    if (auto file = qobject_cast<QFile *>(dev))
        return u" in file " + file->fileName();
    return { };
}
