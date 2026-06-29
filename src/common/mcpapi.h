// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mcp-server/mcptool.h"


// MCP tool that lists all open documents together with their key properties.
class DocumentListMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentListMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that returns the lots of an open document. Lots are addressed by their
// row index into the unsorted, unfiltered document model; this index is also used
// by the editing tools to reference lots.
class DocumentReadMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentReadMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that adds new lots to an open document. All lots are validated up front
// and added as a single undo step.
class DocumentAddLotsMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentAddLotsMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that edits existing lots of an open document, addressed by their row
// index. All edits are validated up front and applied as a single undo step.
class DocumentEditLotsMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentEditLotsMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that removes lots from an open document, addressed by their row index.
// All rows are validated up front and removed as a single undo step.
class DocumentRemoveLotsMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentRemoveLotsMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that creates a new, empty document.
class DocumentCreateMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentCreateMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that opens a BrickStore (BSX) document from the filesystem.
class DocumentOpenMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentOpenMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that imports a BrickLink XML file into a new document.
class DocumentImportBrickLinkXmlMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentImportBrickLinkXmlMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that imports an LDraw or Studio model file into a new document.
class DocumentImportLDrawMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentImportLDrawMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that parts out a catalog item's inventory into a new document.
class DocumentImportPartInventoryMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentImportPartInventoryMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that saves a document as BSX to the filesystem.
class DocumentSaveMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentSaveMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};

// MCP tool that exports a document as BrickLink XML to the filesystem.
class DocumentExportBrickLinkXmlMcpTool : public McpTool
{
    Q_OBJECT

public:
    explicit DocumentExportBrickLinkXmlMcpTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject inputSchema() const override;
    Result execute(const QJsonObject &arguments) override;
};
