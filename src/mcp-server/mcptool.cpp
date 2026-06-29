// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "mcptool.h"

#include <QBuffer>
#include <QImage>

using namespace Qt::StringLiterals;


McpTool::McpTool(QObject *parent)
    : QObject(parent)
{ }

McpTool::Result McpTool::Result::text(const QString &text)
{
    return { QJsonArray { QJsonObject { { u"type"_s, u"text"_s }, { u"text"_s, text } } }, false };
}

McpTool::Result McpTool::Result::error(const QString &message)
{
    return { QJsonArray { QJsonObject { { u"type"_s, u"text"_s }, { u"text"_s, message } } }, true };
}

McpTool::Result McpTool::Result::image(const QImage &image, const QString &caption)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    QJsonArray content;
    if (!caption.isEmpty())
        content.append(QJsonObject { { u"type"_s, u"text"_s }, { u"text"_s, caption } });
    content.append(QJsonObject {
        { u"type"_s,     u"image"_s },
        { u"data"_s,     QString::fromLatin1(png.toBase64()) },
        { u"mimeType"_s, u"image/png"_s }
    });
    return { content, false };
}

QJsonObject McpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject { } }
    };
}
