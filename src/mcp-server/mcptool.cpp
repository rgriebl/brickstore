// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "mcptool.h"

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

QJsonObject McpTool::inputSchema() const
{
    return QJsonObject {
        { u"type"_s, u"object"_s },
        { u"properties"_s, QJsonObject { } }
    };
}
