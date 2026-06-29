// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mcp-server/mcptool.h"


namespace BrickLink {

// MCP tool that searches the BrickLink item catalog.
// All filter parameters are optional; supplying more of them narrows the result set.
class CatalogQueryMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit CatalogQueryMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that returns the valid schema values for catalog queries:
// item types, categories, colors, and relationship types.
class CatalogSchemaMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit CatalogSchemaMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that returns the BrickLink price guide for an item/color. May block
// briefly while the data is fetched from BrickLink if it is not already cached.
class CatalogPriceGuideMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit CatalogPriceGuideMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that returns the catalog picture of an item/color as an image. May
// block briefly while the image is fetched from BrickLink if it is not cached.
class CatalogPictureMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit CatalogPictureMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

} // namespace BrickLink
