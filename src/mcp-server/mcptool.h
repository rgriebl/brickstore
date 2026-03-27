// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>


class McpTool : public QObject
{
    Q_OBJECT

public:
    // Represents the result of a tool invocation.
    // content holds an array of MCP content blocks (typically text items).
    struct Result {
        QJsonArray content;
        bool isError = false;

        // Convenience constructors
        static Result text(const QString &text);
        static Result error(const QString &message);
    };

    explicit McpTool(QObject *parent = nullptr);
    ~McpTool() override = default;

    // Tool metadata – must be implemented by subclasses
    virtual QString name() const = 0;
    virtual QString description() const = 0;

    // JSON Schema describing the tool's input parameters.
    // The default implementation returns an empty object schema (no parameters).
    virtual QJsonObject inputSchema() const;

    // Invoked by McpServer when a client calls this tool.
    // Subclasses must override this to provide the actual implementation.
    virtual Result execute(const QJsonObject &arguments) = 0;
};
