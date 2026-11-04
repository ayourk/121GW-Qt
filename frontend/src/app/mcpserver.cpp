// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Mcp/McpHttpServer.cs, LoopbackHttp.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "mcpserver.h"

#include "metertools.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

Q_LOGGING_CATEGORY(lcMcp, "121gw.mcp")

namespace readout {

namespace {

constexpr qsizetype MaxRequest = 1 << 20;   // 1 MiB: tool calls are small
constexpr int RequestTimeoutMs = 30000;
const QByteArray EndpointPath = "/mcp";
const QStringList ProtocolVersions = {"2025-06-18", "2025-03-26", "2024-11-05"};

bool isLocalHost(const QString &host)
{
    return host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("[::1]") || host == QLatin1String("::1");
}

QString hostName(const QByteArray &hostHeader)
{
    const QString h = QString::fromLatin1(hostHeader).trimmed();
    if (h.startsWith(u'[')) {
        const auto close = h.indexOf(u']');
        return close > 0 ? h.left(close + 1) : h;
    }
    const auto colon = h.lastIndexOf(u':');
    return colon >= 0 ? h.left(colon) : h;
}

// Compares in time that doesn't depend on where the first difference is.
bool fixedTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

QJsonObject rpcResult(const QJsonValue &id, const QJsonObject &result)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

QJsonObject rpcError(const QJsonValue &id, int code, const QString &message)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", QJsonObject{{"code", code}, {"message", message}}}};
}

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    default: return "Internal Server Error";
    }
}

} // namespace

McpServer::McpServer(MeterTools *tools, QString version, QObject *parent)
    : QObject(parent), m_tools(tools), m_version(std::move(version)), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *socket = m_server->nextPendingConnection()) {
            // Only this computer (the socket is loopback-bound; this double-checks).
            if (!socket->peerAddress().isLoopback()) {
                socket->abort();
                socket->deleteLater();
                continue;
            }
            m_buffers.insert(socket, {});
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
            QTimer::singleShot(RequestTimeoutMs, socket, [this, socket] {
                if (m_buffers.contains(socket))   // still reading the request: too slow
                    socket->abort();
            });
        }
    });
}

bool McpServer::start(quint16 port, const QByteArray &token)
{
    if (token.isEmpty()) {
        m_error = QStringLiteral("No token.");
        return false;
    }
    m_token = token;
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        m_error = m_server->errorString();
        qCWarning(lcMcp) << "listen failed:" << m_error;
        return false;
    }
    m_error.clear();
    qCInfo(lcMcp) << "MCP server listening on" << endpoint();
    return true;
}

void McpServer::stop()
{
    m_server->close();
}

bool McpServer::isRunning() const
{
    return m_server->isListening();
}

quint16 McpServer::port() const
{
    return m_server->serverPort();
}

QString McpServer::endpoint() const
{
    return QStringLiteral("http://127.0.0.1:%1%2").arg(port()).arg(QString::fromLatin1(EndpointPath));
}

void McpServer::onReadyRead(QTcpSocket *socket)
{
    if (!m_buffers.contains(socket))
        return;   // already answering
    QByteArray &buf = m_buffers[socket];
    buf += socket->readAll();
    if (buf.size() > MaxRequest) {
        m_buffers.remove(socket);
        write(socket, 413);
        return;
    }
    const auto headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    Request req;
    const QList<QByteArray> lines = buf.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    if (requestLine.size() < 3) {
        m_buffers.remove(socket);
        write(socket, 400);
        return;
    }
    req.method = requestLine[0];
    req.path = requestLine[1];
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        const auto colon = line.indexOf(':');
        if (colon > 0)
            req.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }
    if (req.headers.contains("transfer-encoding")) {
        m_buffers.remove(socket);
        write(socket, 400);   // clients send Content-Length for requests this small
        return;
    }
    bool ok = true;
    const qsizetype length = req.headers.value("content-length", "0").toLongLong(&ok);
    if (!ok || length < 0 || length > MaxRequest) {
        m_buffers.remove(socket);
        write(socket, 400);
        return;
    }
    if (buf.size() - (headerEnd + 4) < length)
        return;   // more body to come
    req.body = buf.mid(headerEnd + 4, length);
    m_buffers.remove(socket);
    respond(socket, req);
}

int McpServer::check(const Request &request) const
{
    QByteArray path = request.path;
    if (const auto q = path.indexOf('?'); q >= 0)
        path.truncate(q);
    while (path.endsWith('/') && path.size() > 1)
        path.chop(1);
    if (path != EndpointPath)
        return 404;

    // A web page can't reach it by pointing its own name at 127.0.0.1 (DNS rebinding): the
    // request must be addressed to this computer, and a browser's Origin must be local.
    if (!isLocalHost(hostName(request.headers.value("host"))))
        return 403;
    const QByteArray origin = request.headers.value("origin");
    if (!origin.isEmpty()) {
        const QUrl url(QString::fromLatin1(origin));
        if (!url.isValid() || !isLocalHost(url.host()))
            return 403;
    }

    const QByteArray auth = request.headers.value("authorization");
    if (!auth.toLower().startsWith("bearer ") || !fixedTimeEquals(auth.mid(7).trimmed(), m_token))
        return 401;
    return 0;
}

void McpServer::respond(QTcpSocket *socket, const Request &request)
{
    if (const int refusal = check(request)) {
        write(socket, refusal, {}, refusal == 401 ? QList<QPair<QByteArray, QByteArray>>{{"WWW-Authenticate", "Bearer"}}
                                                  : QList<QPair<QByteArray, QByteArray>>{});
        return;
    }
    if (request.method != "POST") {
        // Stateless: no server-to-client stream (GET) and no sessions to end (DELETE).
        write(socket, 405, {}, {{"Allow", "POST"}});
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(request.body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        write(socket, 400, QJsonDocument(rpcError(QJsonValue::Null, -32700, "Parse error")).toJson(QJsonDocument::Compact),
              {{"Content-Type", "application/json"}});
        return;
    }
    const QJsonObject message = doc.object();
    if (!message.contains("id") || message.contains("result") || message.contains("error")) {
        write(socket, 202);   // a notification or a response: nothing to answer
        return;
    }
    QPointer<QTcpSocket> guard(socket);
    handleMessage(message, [guard](const QJsonObject &reply) {
        if (guard)
            write(guard, 200, QJsonDocument(reply).toJson(QJsonDocument::Compact),
                  {{"Content-Type", "application/json"}, {"Cache-Control", "no-cache, no-store"}});
    });
}

void McpServer::handleMessage(const QJsonObject &message, std::function<void(const QJsonObject &)> reply)
{
    const QJsonValue id = message.value("id");
    const QString method = message.value("method").toString();
    const QJsonObject params = message.value("params").toObject();

    if (method == QLatin1String("initialize")) {
        const QString asked = params.value("protocolVersion").toString();
        const QString version = ProtocolVersions.contains(asked) ? asked : ProtocolVersions.first();
        reply(rpcResult(id, {
            {"protocolVersion", version},
            {"capabilities", QJsonObject{{"tools", QJsonObject{{"listChanged", false}}}}},
            {"serverInfo", QJsonObject{{"name", "121gw-qt"}, {"title", "121gw-qt (EEVblog 121GW)"}, {"version", m_version}}},
            {"instructions", MeterTools::Instructions},
        }));
    } else if (method == QLatin1String("ping")) {
        reply(rpcResult(id, {}));
    } else if (method == QLatin1String("tools/list")) {
        reply(rpcResult(id, {{"tools", m_tools->definitions()}}));
    } else if (method == QLatin1String("tools/call")) {
        const QString name = params.value("name").toString();
        if (!m_tools->has(name)) {
            reply(rpcError(id, -32602, QStringLiteral("Unknown tool: %1").arg(name)));
            return;
        }
        emit toolCalled(name);
        m_tools->call(name, params.value("arguments").toObject(), [reply, id](const ToolResult &r) {
            reply(rpcResult(id, {
                {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", r.text}}}},
                {"isError", r.isError},
            }));
        });
    } else {
        reply(rpcError(id, -32601, QStringLiteral("Method not found: %1").arg(method)));
    }
}

void McpServer::write(QTcpSocket *socket, int status, const QByteArray &body,
                      const QList<QPair<QByteArray, QByteArray>> &headers)
{
    QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
    for (const auto &h : headers)
        out += h.first + ": " + h.second + "\r\n";
    out += "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    socket->write(out);
    socket->disconnectFromHost();
}

} // namespace readout
