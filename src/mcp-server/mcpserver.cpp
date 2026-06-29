// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "mcpserver.h"
#include "mcptool.h"

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTcpServer>

#include <map>
#include <QHostAddress>
#include <QUrl>
#include <QUuid>
#include <QUrlQuery>

#include <deque>
#include <memory>

using namespace Qt::StringLiterals;


// ============================================================================
//  McpSession  –  one connected SSE client
// ============================================================================

class McpSession
{
    Q_DISABLE_COPY_MOVE(McpSession)

public:
    McpSession(const QString &id, QHttpServerResponder &&responder)
        : m_id(id)
        , m_responder(std::move(responder))
    {
        QHttpHeaders headers;
        headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/event-stream");
        headers.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-cache");
        headers.append(QHttpHeaders::WellKnownHeader::Connection, "keep-alive");
        headers.append("X-Accel-Buffering", "no");

        m_responder.writeBeginChunked(headers, QHttpServerResponder::StatusCode::Ok);
    }

    QString id() const { return m_id; }

    // Send a named SSE event with a raw data payload.
    void sendEvent(const QByteArray &eventType, const QByteArray &data)
    {
        QByteArray chunk;
        chunk += "event: ";
        chunk += eventType;
        chunk += "\ndata: ";
        chunk += data;
        chunk += "\n\n";
        m_responder.writeChunk(chunk);
    }

    // Send a JSON-RPC object as a "message" SSE event.
    void sendMessage(const QJsonObject &json)
    {
        sendEvent("message", QJsonDocument(json).toJson(QJsonDocument::Compact));
    }

private:
    QString m_id;
    QHttpServerResponder m_responder;
};


// ============================================================================
//  McpServerPrivate
// ============================================================================

class McpServerPrivate
{
public:
    explicit McpServerPrivate(McpServer *q) : q_ptr(q) { }

    McpServer *q_ptr;
    QHttpServer *httpServer = nullptr;
    QTcpServer  *tcpServer  = nullptr;

    // Ordered map so tools/list returns tools in insertion order.
    QMap<QString, McpTool *> tools;

    // Active SSE sessions, keyed by their id.  QHttpServerResponder does not expose
    // the underlying connection (and QAbstractHttpServer has no per-connection
    // "disconnected" signal in this Qt version), so we cannot reap a session exactly
    // when its client goes away.  Instead we keep the map bounded: when a new client
    // connects past the cap, the oldest session is evicted.  m_sessionOrder records
    // insertion order so eviction is FIFO.
    static constexpr std::size_t MaxSessions = 32;
    std::map<QString, std::unique_ptr<McpSession>> sessions;
    std::deque<QString> sessionOrder;

    void setupRoutes();

    // Route handlers
    void onSseConnection(const QHttpServerRequest &request, QHttpServerResponder &responder);
    QHttpServerResponse onMessagePost(const QHttpServerRequest &request);
    QHttpServerResponse onStreamableHttpPost(const QHttpServerRequest &request);

    // Computes the JSON-RPC response for a single request object. Returns an empty
    // object for notifications (which must not be answered).
    QJsonObject dispatchJsonRpc(const QJsonObject &rpc);

    // Method handlers – each returns the "result" value for jsonRpcSuccess
    QJsonObject handleInitialize(const QJsonObject &params) const;
    QJsonObject handleToolsList(const QJsonObject &params) const;
    QJsonObject handleToolsCall(const QJsonObject &params) const;

    // JSON-RPC envelope builders
    static QJsonObject jsonRpcSuccess(const QJsonValue &id, const QJsonValue &result)
    {
        return QJsonObject {
            { u"jsonrpc"_s, u"2.0"_s },
            { u"id"_s,      id       },
            { u"result"_s,  result   }
        };
    }

    static QJsonObject jsonRpcError(const QJsonValue &id, int code, const QString &message)
    {
        return QJsonObject {
            { u"jsonrpc"_s, u"2.0"_s },
            { u"id"_s,      id       },
            { u"error"_s, QJsonObject {
                { u"code"_s,    code    },
                { u"message"_s, message }
            }}
        };
    }
};


// ---- Request origin validation ---------------------------------------------

// The MCP server binds to the loopback interface only, but a wildcard CORS policy
// plus a missing Origin/Host check would still let any web page the user has open
// drive the local endpoint (the DNS-rebinding / cross-origin class of issue).
// Accept a request only if it actually came from the loopback interface and, when
// present, its Origin/Host header names a loopback host.
static bool isLocalHost(const QString &authority)
{
    if (authority.isEmpty())
        return false;
    // Parse "host", "host:port" or "[::1]:port" via QUrl's authority parser
    // (the leading "//" makes the rest an authority), which strips the port
    // and any IPv6 brackets for us.
    const QString h = QUrl(u"//"_s + authority).host();
    if (h.isEmpty())
        return false;
    if (h.compare(u"localhost", Qt::CaseInsensitive) == 0)
        return true;
    const QHostAddress addr(h);
    return !addr.isNull() && addr.isLoopback();
}

static bool requestIsLocal(const QHttpServerRequest &request)
{
    if (!request.remoteAddress().isLoopback())
        return false;

    // If the client sent an Origin (browsers always do for cross-origin requests),
    // it must point at a loopback host. The Host header is checked the same way.
    const QByteArray origin = request.value("Origin");
    if (!origin.isEmpty()) {
        const QUrl originUrl(QString::fromLatin1(origin));
        if (!isLocalHost(originUrl.host()))
            return false;
    }
    const QByteArray host = request.value("Host");
    if (!host.isEmpty() && !isLocalHost(QString::fromLatin1(host)))
        return false;

    return true;
}


// ---- Route setup -----------------------------------------------------------

void McpServerPrivate::setupRoutes()
{
    // SSE endpoint: client opens a long-lived GET connection.
    httpServer->route(u"/sse"_s, QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest &request, QHttpServerResponder &responder) {
            onSseConnection(request, responder);
        });

    // Message endpoint: client POSTs JSON-RPC frames here (legacy HTTP+SSE transport).
    httpServer->route(u"/message"_s, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) -> QHttpServerResponse {
            return onMessagePost(request);
        });

    // Streamable HTTP transport (MCP 2025-03-26): request and response share the
    // root endpoint. This is what current clients default to.
    httpServer->route(u"/"_s, QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) -> QHttpServerResponse {
            return onStreamableHttpPost(request);
        });
}


// ---- Route handlers --------------------------------------------------------

void McpServerPrivate::onSseConnection(const QHttpServerRequest &request,
                                       QHttpServerResponder &responder)
{
    if (!requestIsLocal(request)) {
        responder.write(QHttpServerResponder::StatusCode::Forbidden);
        return;
    }

    // Keep the session map bounded (see McpServerPrivate::MaxSessions): evict the
    // oldest sessions before inserting a new one.
    while (sessionOrder.size() >= MaxSessions) {
        sessions.erase(sessionOrder.front());
        sessionOrder.pop_front();
    }

    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Take ownership of the responder so the SSE stream stays open.
    auto session = std::make_unique<McpSession>(sessionId, std::move(responder));

    // Tell the client which URL to POST JSON-RPC messages to.
    const QByteArray endpointUrl = (u"/message?sessionId="_s + sessionId).toUtf8();
    session->sendEvent("endpoint", endpointUrl);

    sessions.emplace(sessionId, std::move(session));
    sessionOrder.push_back(sessionId);
}

QHttpServerResponse McpServerPrivate::onMessagePost(const QHttpServerRequest &request)
{
    if (!requestIsLocal(request))
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Forbidden);

    const QUrlQuery query(request.url());
    const QString sessionId = query.queryItemValue(u"sessionId"_s);

    auto it = sessions.find(sessionId);
    if (it == sessions.end())
        return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

    const QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject())
        return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

    const QJsonObject response = dispatchJsonRpc(doc.object());
    if (!response.isEmpty())
        it->second->sendMessage(response);

    return QHttpServerResponse(QHttpServerResponder::StatusCode::Accepted);
}

// Streamable HTTP transport (MCP 2025-03-26): a single endpoint where the client
// POSTs a JSON-RPC message and gets the response back in the HTTP body. We do not
// keep a server-initiated stream open, so GET is answered with 405 and the optional
// session-id header is not used.
QHttpServerResponse McpServerPrivate::onStreamableHttpPost(const QHttpServerRequest &request)
{
    if (!requestIsLocal(request))
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Forbidden);

    const QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject())
        return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

    const QJsonObject response = dispatchJsonRpc(doc.object());

    // Notifications produce no response body: acknowledge with 202 and no content.
    if (response.isEmpty())
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Accepted);

    return QHttpServerResponse("application/json"_ba,
                               QJsonDocument(response).toJson(QJsonDocument::Compact));
}


// ---- JSON-RPC dispatch -----------------------------------------------------

QJsonObject McpServerPrivate::dispatchJsonRpc(const QJsonObject &rpc)
{
    const QString method     = rpc[u"method"_s].toString();
    const QJsonValue id      = rpc[u"id"_s];
    const QJsonObject params = rpc[u"params"_s].toObject();

    // Notifications have no "id" field – they must not receive a response.
    const bool isNotification = !rpc.contains(u"id"_s);

    if (method == u"initialize"_s)
        return jsonRpcSuccess(id, handleInitialize(params));
    else if (method == u"initialized"_s)
        return { }; // notification – no response
    else if (method == u"ping"_s)
        return jsonRpcSuccess(id, QJsonObject { });
    else if (method == u"tools/list"_s)
        return jsonRpcSuccess(id, handleToolsList(params));
    else if (method == u"tools/call"_s)
        return jsonRpcSuccess(id, handleToolsCall(params));

    if (isNotification)
        return { };
    return jsonRpcError(id, -32601, u"Method not found: "_s + method);
}


// ---- Method handlers -------------------------------------------------------

QJsonObject McpServerPrivate::handleInitialize(const QJsonObject &params) const
{
    // Echo the client's requested protocol version if it is one we understand,
    // otherwise offer the newest we support. Both the legacy HTTP+SSE (2024-11-05)
    // and the Streamable HTTP (2025-03-26) transports share this JSON-RPC layer.
    static const QStringList supportedVersions { u"2025-03-26"_s, u"2024-11-05"_s };
    const QString requested = params[u"protocolVersion"_s].toString();
    const QString version = supportedVersions.contains(requested) ? requested
                                                                   : supportedVersions.constFirst();
    return QJsonObject {
        { u"protocolVersion"_s, version },
        { u"capabilities"_s, QJsonObject {
            { u"tools"_s, QJsonObject { } }
        }},
        { u"serverInfo"_s, QJsonObject {
            { u"name"_s,    u"BrickStore MCP Server"_s },
            { u"version"_s, u"1.0"_s                   }
        }}
    };
}

QJsonObject McpServerPrivate::handleToolsList(const QJsonObject &params) const
{
    Q_UNUSED(params)
    QJsonArray toolsArray;
    for (const McpTool *t : std::as_const(tools)) {
        toolsArray.append(QJsonObject {
            { u"name"_s,        t->name()        },
            { u"description"_s, t->description() },
            { u"inputSchema"_s, t->inputSchema() }
        });
    }
    return QJsonObject { { u"tools"_s, toolsArray } };
}

QJsonObject McpServerPrivate::handleToolsCall(const QJsonObject &params) const
{
    const QString toolName = params[u"name"_s].toString();
    const QJsonObject args = params[u"arguments"_s].toObject();

    McpTool *t = tools.value(toolName);
    if (!t) {
        return QJsonObject {
            { u"content"_s, QJsonArray { QJsonObject {
                { u"type"_s, u"text"_s },
                { u"text"_s, u"Unknown tool: %1"_s.arg(toolName) }
            }}},
            { u"isError"_s, true }
        };
    }

    const McpTool::Result result = t->execute(args);
    return QJsonObject {
        { u"content"_s, result.content },
        { u"isError"_s, result.isError }
    };
}


// ============================================================================
//  McpServer  –  public API
// ============================================================================

McpServer::McpServer(QObject *parent)
    : QObject(parent)
    , d_ptr(new McpServerPrivate(this))
{
    Q_D(McpServer);
    d->httpServer = new QHttpServer(this);
    d->setupRoutes();
}

McpServer::~McpServer() = default;

bool McpServer::listen(const QHostAddress &address, quint16 port)
{
    Q_D(McpServer);
    d->tcpServer = new QTcpServer(this);
    if (!d->tcpServer->listen(address, port)) {
        delete d->tcpServer;
        d->tcpServer = nullptr;
        return false;
    }
    d->httpServer->bind(d->tcpServer);
    return true;
}

void McpServer::close()
{
    Q_D(McpServer);
    d->sessions.clear();
    d->sessionOrder.clear();
    if (d->tcpServer) {
        d->tcpServer->close();
        delete d->tcpServer;
        d->tcpServer = nullptr;
    }
}

quint16 McpServer::serverPort() const
{
    Q_D(const McpServer);
    return d->tcpServer ? d->tcpServer->serverPort() : 0;
}

void McpServer::addTool(McpTool *tool)
{
    Q_D(McpServer);
    if (tool) {
        // Honor the documented ownership contract: adopt a parentless tool so it is
        // destroyed with the server instead of leaking.
        if (!tool->parent())
            tool->setParent(this);
        d->tools.insert(tool->name(), tool);
    }
}

void McpServer::removeTool(const QString &name)
{
    Q_D(McpServer);
    d->tools.remove(name);
}

McpTool *McpServer::tool(const QString &name) const
{
    Q_D(const McpServer);
    return d->tools.value(name);
}

QList<McpTool *> McpServer::tools() const
{
    Q_D(const McpServer);
    return d->tools.values();
}
