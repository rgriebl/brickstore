// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHostAddress>
#include <QObject>
#include <QScopedPointer>


class McpTool;
class McpServerPrivate;

// McpServer exposes a Model Context Protocol (MCP) server over HTTP/SSE.
//
// Transport:  HTTP + Server-Sent Events (SSE), as specified by MCP 2024-11-05.
//
// Endpoints:
//   GET  /sse          – SSE stream; on connect the server emits an "endpoint"
//                        event whose data is the POST URL for this session.
//   POST /message?sessionId=<id>
//                      – JSON-RPC 2.0 messages from the client.
//
// Protocol methods handled:
//   initialize / initialized / ping / tools/list / tools/call
//
// Usage:
//   McpServer *server = new McpServer(this);
//   server->addTool(new MyTool(server));
//   server->listen(QHostAddress::LocalHost, 3000);
class McpServer : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(McpServer)

public:
    explicit McpServer(QObject *parent = nullptr);
    ~McpServer() override;

    // Start listening.  Pass port 0 to let the OS choose a free port.
    // Returns true on success.
    bool listen(const QHostAddress &address = QHostAddress::LocalHost, quint16 port = 0);

    // Stop accepting new connections and close all active SSE sessions.
    void close();

    // Returns the port the server is bound to, or 0 if not listening.
    quint16 serverPort() const;

    // Tool registry --------------------------------------------------------

    // Takes ownership if the tool has no parent; otherwise leaves ownership
    // with its parent.  The tool is keyed by tool->name().
    void addTool(McpTool *tool);

    // Removes the tool with the given name from the registry (does not delete it).
    void removeTool(const QString &name);

    // Returns the tool registered under name, or nullptr.
    McpTool *tool(const QString &name) const;

    // Returns all registered tools in insertion order.
    QList<McpTool *> tools() const;

private:
    QScopedPointer<McpServerPrivate> d_ptr;
};
